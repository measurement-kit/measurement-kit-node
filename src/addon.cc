// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include <list>
#include <measurement_kit/common.hpp>
#include <measurement_kit/nettests.hpp>
#include <mutex>
#include <nan.h>
#include <uv.h>

extern "C" {
static void mkuv_resume(uv_async_t *async);
static void mkuv_destroy(uv_handle_t *handle);
}

namespace mk {

// Var was renamed SharedPtr in MK v0.8.0-dev
#if MK_VERSION_MAJOR > 0 || MK_VERSION_MINOR > 7
template <typename T> using Var = SharedPtr<T>;
#endif

namespace node {

// # Runner
//
// The `Runner` class takes care of running a generic Nettest in the
// Node execution environment. Specifically, it handles callbacks dispatching
// and memory management, as explained in detail below.
//
// In the common case, we run the Nettest in a MK background thread, from
// which we cannot call directly the API of Node. Therefore, whenever we need
// to callback, we suspend execution of the lambda that we would actually
// want to run and notify the libuv I/O loop. We append lambdas into a queue
// because libuv may coalesce many wakeup calls into a single call. We need
// to protect the queue using a mutex, because we're accessing it from at least
// two different threads: MK's background thread and libuv I/O loop thread.
//
// This is an asynchronous class that must be used through a shared pointer
// abstraction and whose lifecycle is controlled by a self reference. To enforce
// this usage pattern, most methods are static and receive a `self` smart
// pointer as their first argument. The self reference is created when we're
// starting the test and is removed when the `async` structure used to
// communicate with libuv event loop has been successfully closed.
class Runner {
  public:
    // ## Constructor
    //
    // The constructor takes ownership of the specific test instance and
    // stores it into a publicly accessible smart pointer than can be later
    // used to further configure the test instance.
    Runner(nettests::BaseTest *ptr) : nettest{ptr} {}

    // ## Resume
    //
    // The Resume() method is called by libuv's I/O loop thread to resume all
    // the MK callback whose exection has been suspended.
    //
    // XXX reason about the exception policy.
    static void Resume(Var<Runner> self) {
        std::list<std::function<void()>> functions;
        {
            std::unique_lock<std::recursive_mutex> _{self->mutex};
            std::swap(self->suspended, functions);
        }
        for (auto &fn : functions) {
            fn();
        }
    }

    // ## RunOrStart
    //
    // The RunOrStart() method is called by the Nettest Node object when the
    // user has asked it to run or start the test.
    static void RunOrStart(Var<Runner> self) {
        std::unique_lock<std::recursive_mutex> _{self->mutex};

        // RunOrStart() is where we bind the shared pointer to this object with
        // the async structure used by libuv, and where we register the async
        // with libuv's internal loop. This will keep the loop running even if
        // there are no other pending I/O events or calls.
        if (!!self->selfref) {
            throw std::runtime_error("already_selfref");
        }
        self->async.data = self.get();
        self->selfref = self;
        if (uv_async_init(uv_default_loop(), &self->async, mkuv_resume) != 0) {
            throw std::runtime_error("uv_async_init");
        }

        // When the internal structure used to run the test will be destroyed,
        // RunOrStart() calls one last time libuv's event loop, to tell
        // libuv that we want to close our async handle. In turn, libuv will
        // call the `mkuv_destroy` callback when the handle has been closed
        // and finally in such callback we will remove the self reference, so
        // that we can finally free this object. Important points:
        //
        // 1. we must call `uv_close()` from libuv's loop thread (i.e. we
        //    must call `SuspendOn()`, because otherwise on Linux the callback
        //    won't ever be called and we will hang forever;
        //
        // 2. memory must be freed after the callback has been called because,
        //    before that, libuv is still using the memory;
        //
        // 3. after the async handle has been closed, the libuv event loop
        //    may exit if the async handle was the last handle keeping alive
        //    the event loop (i.e. this is why a Node script running a test
        //    exits right after the end of the test).
        self->nettest->on_destroy([self]() {
            self->SuspendOn([self]() {
                uv_close((uv_handle_t *)&self->async, mkuv_destroy);
            });
        });

        // Whenever an MK callback is called, RunOrStart() captures all the
        // state pertaining to such callback into a lambda closure and suspends
        // the execution of the lambda. Such execution will resume when libuv
        // will callback us from its main I/O loop.
        //
        // As usual, it is very important to store into the closure persistent
        // state (i.e. no pointers or references) or we'll segfault. It is
        // also important, for robustness, to pass to lambdas only shared
        // pointers, numbers and other POD types, or types that are explicitly
        // moved into the lambdas, to avoid concurrent destruction.
        //
        // The act of adding a lambda to a list of suspended lambda requires
        // us to lock a mutex to prevent data races (see SuspendOn). Apart from
        // that, the act of invoking callabacks should not IMHO require any
        // locking because we should not modify the callbacks after the test
        // has been started.

        if (!!self->begin_cb) {
            self->nettest->on_begin([self]() {
                self->SuspendOn([self]() {
                    Nan::HandleScope scope;
                    self->begin_cb->Call(0, nullptr);
                });
            });
        }

        if (!!self->end_cb) {
            self->nettest->on_end([self]() {
                self->SuspendOn([self]() {
                    Nan::HandleScope scope;
                    self->end_cb->Call(0, nullptr);
                });
            });
        }

        if (!!self->entry_cb) {
            self->nettest->on_entry([self](std::string s) {
                self->SuspendOn([self, s]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(s).ToLocalChecked()};
                    self->entry_cb->Call(argc, argv);
                });
            });
        }

        if (!!self->event_cb) {
            self->nettest->on_event([self](const char *s) {
                self->SuspendOn([ self, mcopy = std::string(s) ]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(mcopy).ToLocalChecked()};
                    self->event_cb->Call(argc, argv);
                });
            });
        }

        if (!!self->log_cb) {
            self->nettest->on_log([self](uint32_t level, const char *msg) {
                self->SuspendOn([ self, level, mcopy = std::string(msg) ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(level), Nan::New(mcopy).ToLocalChecked()};
                    self->log_cb->Call(argc, argv);
                });
            });
        }

        if (!!self->progress_cb) {
            self->nettest->on_progress([self](double percent, const char *msg) {
                self->SuspendOn([ self, percent, mcopy = std::string(msg) ]() {
                    std::unique_lock<std::recursive_mutex> _{self->mutex};
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {Nan::New(percent),
                            Nan::New(mcopy).ToLocalChecked()};
                    self->progress_cb->Call(argc, argv);
                });
            });
        }

        // RunOrStart could either run asynchronously or synchronously, and
        // its behavior depends on whether a final callback was specified
        // or not. If there is no final callback we run synchronously.

        if (!!self->complete_cb) {
            self->nettest->start([self]() {
                self->SuspendOn([self]() {
                    Nan::HandleScope scope;
                    self->complete_cb->Call(0, nullptr);
                });
            });
        } else {
            self->nettest->run();
        }

        // At the end of RunOrStart() we dispose of our reference to the
        // nettest, which breaks references loop. (XXX How so? Explain!)
        self->nettest = nullptr;
    }

    // ## Public Attributes
    // The following are public fields used by Node's NettestWrap to
    // configure the test more conveniently and/or required to be public
    // so that libuv C code can call us back.

    // The begin_cb field is an optional callback called right before the
    // test starts executing any operation.
    Var<Nan::Callback> begin_cb;

    // The complete_cb field is the optional final callback called when the
    // test is complete. If this field is not set, the test will run
    // synchronously, otherwise it will run asynchronously.
    Var<Nan::Callback> complete_cb;

    // The end_cb field is an optional callback called after the measurements
    // have been run, but before closing the report.
    Var<Nan::Callback> end_cb;

    // The entry_cb field is an optional callback called after each
    // measurement and provoding the measurement entry.
    Var<Nan::Callback> entry_cb;

    // The event_cb field is an optional callback that can intercept test
    // specific events (encoded as JSON) occurring during the test.
    Var<Nan::Callback> event_cb;

    // The log_cb field is an optional callback that is called whenever
    // a log line is emitted.
    Var<Nan::Callback> log_cb;

    // The nettest field contains the test that should be run.
    Var<nettests::BaseTest> nettest;

    // The progress_cb field is an optional callback called to update the
    // user regarding the progress of the test (in percentage).
    Var<Nan::Callback> progress_cb;

    // The selfref field is used to keep the object alive until it can be
    // safely deleted. C code manipulating this object should, as usual, create
    // a copy of the shared pointer on the stack for safety.
    Var<Runner> selfref;

  private:
    // ## Private Methods and Attributes

    // The SuspendOn private method registers a specific function (passed
    // by move) to be called later in the context of libuv I/O thread. And,
    // in addition, this method also tells libuv I/O thread that we have
    // one or more callback we would like to execute in its context.
    //
    // Passing by move here is very important, because it gives the libuv
    // thread exclusive control over the callback lifecycle.
    void SuspendOn(std::function<void()> &&func) {
        std::unique_lock<std::recursive_mutex> _{mutex};
        if (uv_async_send(&async) != 0) {
            throw std::runtime_error("uv_async_send");
        }
        suspended.push_back(std::move(func));
    }

    // The suspended field is the list of suspended callbacks.
    std::list<std::function<void()>> suspended;

    // The async structure (which we must explicitly initialize because
    // it is a POD) is the structure we use for coordinating with libuv's
    // I/O loop to keep it alive and tell it we need some callbacks to
    // execute in its context. When we will close this structure, we
    // will also prevent libuv I/O loop from exiting.
    uv_async_t async{};

    // The mutex structure protects `suspended` and `async` (and possibly other
    // fields) from modification by concurrent threads of execution.
    std::recursive_mutex mutex;
};

} // namespace node
} // namespace mk

// ## C Callbacks

// The mkuv_resume C callback is called by libuv's I/O loop thread to give us
// a chance of running the suspended callbacks in its context. As an extra
// safety (and correctness) measure, we keep on the stack a copy of the smart
// pointer keeping alive the Runner structure.
static void mkuv_resume(uv_async_t *async) {
    using namespace mk::node;
    using namespace mk;
    Runner *runner = static_cast<Runner *>(async->data);
    Var<Runner> keepalive = runner->selfref;
    Runner::Resume(keepalive);
}

// The mkuv_destroy C callback is called by libuv's I/O loop thread when
// libuv has successfully closed the async handle. Since we schedule closure
// of the handle after MK's internal object for running the test is gone,
// this means there are no other users of the Runner structure (since
// the two user's are MK's nettest thread and libuv I/O loop thread). Hence,
// we can at this pointer remove the self reference that was preventing
// the object from being destroyed.
static void mkuv_destroy(uv_handle_t *handle) {
    using namespace mk::node;
    using namespace mk;
    uv_async_t *async = reinterpret_cast<uv_async_t *>(handle);
    Runner *runner = static_cast<Runner *>(async->data);
    runner->selfref = nullptr;
}

namespace mk {
namespace node {

// # NettestWrap
//
// The NettestWrap class template is the Node-visible object. Internally it
// contains a smart pointer to a Runner. Methods on the NettestWrap
// template class will actually set the Runner internals. When it will
// be time to run the test, this template class will basically defer all the
// work to the Runner class.
//
// After you have called either Run or Start, it will be an error to attempt
// to call any other method of the test again. The reason why we enforce
// this rule is that, once the test has been configured and is running, it
// is not anyway possible to perform any change, since in MK the test actually
// runs using an internal object with fields copied from the external
// object. This pattern is implemented to have a clear separation from
// facade and backround objects, which leads to easier memory management
// because objects are clearly separated. As such, forbidding the user to
// further fiddle with the Node-created object seems good because anyway
// there is no way for us to relate any changes performed.
//
// (Honestly, an improvement we can apply may be that test objects are
// like templates that schedule internal objects, but in that case I
// would like to make sure there are no memory management hazards caused
// by possibly multiple threads, so I'd defer this change for now.)
template <typename Nettest> class NettestWrap : public Nan::ObjectWrap {
  public:
    // ## Constructors

    // The static constructor is used when the user invokes the template
    // function without `new` (e.g. `let foo = FooTest()`).
    //
    // We use the static factory pattern to access the static constructor, to
    // avoid compiler warnings and refactoring issues caused by the facts that:
    //
    // 1. when you define a static attribute, you should also instantiate it
    //
    // 2. this is a template, hence it's more syntax work to instantiate
    //
    // Thus, a static factory makes things simpler.
    static Nan::Persistent<v8::Function> &constructor() {
        static Nan::Persistent<v8::Function> instance;
        return instance;
    }

    // The Initialize() static method will create the function template that
    // JavaScript will use to create an instance of this class, and will
    // store such function template into the exports object.
    static void Initialize(const char *cname, v8::Local<v8::Object> exports) {
        Nan::HandleScope scope;

        // Initialize() will bind the function template to the New() method.
        v8::Local<v8::String> name = Nan::New(cname).ToLocalChecked();
        auto tpl = Nan::New<v8::FunctionTemplate>(New);

        // Initialize() will also perform other initialization actions, e.g.
        // adding all the methods to JavaScript object's prototype.
        tpl->SetClassName(name);
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        Nan::SetPrototypeMethod(tpl, "add_input", AddInput);
        Nan::SetPrototypeMethod(tpl, "add_input_filepath", AddInputFilepath);
        Nan::SetPrototypeMethod(tpl, "set_error_filepath", SetErrorFilepath);
        Nan::SetPrototypeMethod(tpl, "set_options", SetOptions);
        Nan::SetPrototypeMethod(tpl, "set_output_filepath", SetOutputFilepath);
        Nan::SetPrototypeMethod(tpl, "set_verbosity", SetVerbosity);
        Nan::SetPrototypeMethod(tpl, "on_begin", OnBegin);
        Nan::SetPrototypeMethod(tpl, "on_end", OnEnd);
        Nan::SetPrototypeMethod(tpl, "on_entry", OnEntry);
        Nan::SetPrototypeMethod(tpl, "on_event", OnEvent);
        Nan::SetPrototypeMethod(tpl, "on_log", OnLog);
        Nan::SetPrototypeMethod(tpl, "on_progress", OnProgress);
        Nan::SetPrototypeMethod(tpl, "run", Run);
        Nan::SetPrototypeMethod(tpl, "start", Start);

        // Once we have configured the function template, we register it into
        // the exports, so that it is not garbage collected. This MUST be
        // performed after we've finished configuring `tpl`, otherwise not
        // all the fields will be exported to Node.
        exports->Set(name, tpl->GetFunction());

        // We also store a copy of `tpl` in `constructor()`, such that
        // it's possible to deal with the case where `new` is not used when
        // an object is constructed (i.e. `let foo = FooTest();`).
        constructor().Reset(tpl->GetFunction());
    }

    // The New static method is the JavaScript object "constructor". We store
    // a pointer to `NettestWrap` into the internal field. In turn, such
    // pointer will contain a smart pointer to a Runner. The reason
    // why we have a second layer of indirection with Runner is
    // to keep separate internal and external objects and thus simplify
    // reference based memory management (been there, learned that).
    //
    // Here we deal both with the case where `new` is used (e.g.
    // `let foo = new FooTest()`) and with the case in which instead
    // `new` is not used (e.g. `let foo = FooTest()`).
    static void New(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        if (info.Length() != 0) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        if (!info.IsConstructCall()) {
            // Case: `let foo = FooTest()`
            Nan::HandleScope scope;
            v8::Local<v8::Function> tpl = Nan::New<v8::Function>(constructor());
            info.GetReturnValue().Set(
                    tpl->NewInstance(info.GetIsolate()->GetCurrentContext(), 0,
                               nullptr)
                            .ToLocalChecked());
            return;
        }
        // Case: `let foo = new FooTest()`
        NettestWrap *nw = new NettestWrap{};
        nw->runner.reset(new Runner{new Nettest{}});
        nw->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    // ## Value Setters

    // The AddInput setter adds one input string to the list of input strings
    // to be processed by this test. If the test takes no input, adding one
    // extra input has basically no visible effect.
    static void AddInput(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->nettest->add_input(*v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The AddInputFilepath setter adds one input file to the list of input file
    // to be processed by this test. If the test takes no input, adding one
    // extra input file has basically no visible effect.
    static void AddInputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->nettest->add_input_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetErrorFilepath setter sets the path where logs will be written. Not
    // setting the error filepath will prevent logs from being written on disk.
    static void SetErrorFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->nettest->set_error_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetOptions setter allows to set test-specific options. You should
    // consult MK documentation for more information on available options.
    static void SetOptions(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(2, info, [&info](Var<Runner> r) {
            r->nettest->set_options(*v8::String::Utf8Value{info[0]->ToString()},
                    *v8::String::Utf8Value{info[1]->ToString()});
        });
    }

    // The SetOutputFilepath setter sets the path where test report will be
    // written. Not setting the output filepath will cause MK to try to write
    // the report on an filepath with a test- and time-dependent name.
    static void SetOutputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->nettest->set_output_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetVerbosity setter allows to set logging verbosity. Zero is
    // equivalent to WARNING, one to INFO, two to DEBUG and more than two
    // makes MK even more verbose.
    static void SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->nettest->set_verbosity(info[0]->Uint32Value());
        });
    }

    // ## Callback Setters

    // The OnBegin setter allows to set the callback called right at the
    // beginning of the network test.
    static void OnBegin(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->begin_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEnd setter allows to set the callback called after all
    // measurements have been performed and before closing the report.
    static void OnEnd(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->end_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEntry setter allows to set the callback called after each
    // measurement. The callback receives a serialized JSON as argument.
    static void OnEntry(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->entry_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEvent setter allows to set the callback called during the test
    // to report test-specific events that occurred.
    static void OnEvent(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->event_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnLog setter allows to set the callback called for each log line
    // emitted by the test. Not setting this callback means that MK will
    // attempt to write logs on the standard error.
    static void OnLog(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->log_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnProgress setter allows to set the callback called to inform you
    // about the progress of the test in percentage.
    static void OnProgress(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<Runner> r) {
            r->progress_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // ## Runners

    // The Run method runs the test synchronously. This will block Node until
    // the test is over. Perhaps not what you want in the common case, but
    // it may be useful in some specific corner cases.
    static void Run(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        RunOrStart(0, info);
    }

    // The Start method runs the test asynchronously and calls the callback
    // passed as argument when the test is done. Note that calling this method
    // will cause Node's event loop to wait for the test to finish, but,
    // unlike Run, Start will allow you to do also other things while you're
    // waiting for the test to finish.
    static void Start(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        RunOrStart(1, info);
    }

    // ## Internals

  private:
    // The SetValue method is a convenience method. It will make sure
    // that the number of arguments is the expected one. It also check whether
    // either Run() or Start() was already called and throw in such case.
    static void SetValue(int argc,
            const Nan::FunctionCallbackInfo<v8::Value> &info,
            std::function<void(Var<Runner>)> &&next) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        if (!This(info)->runner) {
            Nan::ThrowError(
                    "cannot modify object after Run() or Start() was called");
            return;
        }
        next(This(info)->runner);
        info.GetReturnValue().Set(info.This());
    }

    // The This() method is a convenience method used by many others to
    // quickly get the `this` pointer of the class.
    static NettestWrap *This(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        return ObjectWrap::Unwrap<NettestWrap>(info.Holder());
    }

    // The RunOrStart method implements Run() and Start(). It is important
    // to remark that this method will swap the internal running field with
    // an empty running field. This is because one should not attempt to use
    // a test object after a test has been started. (If we ever forget in
    // here about checking for running validity before using it, we will then
    // get a std::runtime_error complaining about a null pointer.)
    static void RunOrStart(
            int argc, const Nan::FunctionCallbackInfo<v8::Value> &info) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        if (!This(info)->runner) {
            Nan::ThrowError(
                    "cannot modify object after Run() or Start() was called");
            return;
        }
        Var<Runner> runner;
        std::swap(runner, This(info)->runner);
        if (argc >= 1) {
            runner->complete_cb.reset(
                    new Nan::Callback(info[0].As<v8::Function>()));
        }
        Runner::RunOrStart(runner);
    }

    // The runner internal field is the shared pointer reference to the
    // Runner that will be used to actually run the test. After you call
    // Start() or Run(), this shared pointer will be cleared, to detach
    // Node-facing objects and test-running objects. Attempting to use
    // this class afterwards results in methods raising errors.
    Var<Runner> runner;
};

} // namespace node
} // namespace mk

// # Node Methods and Initializers

// The Version function returns MK version.
static NAN_METHOD(Version) {
    info.GetReturnValue().Set(Nan::New(mk_version()).ToLocalChecked());
}

// The REGISTER_FUNC macro is a convenience macro to register a free
// function into the exports dictionary.
#define REGISTER_FUNC(name, func)                                              \
    Nan::Set(target, Nan::New<v8::String>(name).ToLocalChecked(),              \
            Nan::GetFunction(Nan::New<v8::FunctionTemplate>(func))             \
                    .ToLocalChecked());

// The REGISTER_TEST macro is a convenience macro to register a test
// class into the exports dictionary.
#define REGISTER_TEST(name)                                                    \
    mk::node::NettestWrap<mk::nettests::name>::Initialize(#name, target)

// The Initialize function fills in the exports for this module.
NAN_MODULE_INIT(Initialize) {
    REGISTER_FUNC("version", Version);
    REGISTER_TEST(HttpHeaderFieldManipulationTest);
    REGISTER_TEST(WebConnectivityTest);
}

// The NODE_MODULE macro declares that this is a Node module with
// initialization function called Initialize.
NODE_MODULE(measurement_kit, Initialize)
