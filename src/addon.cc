#include <measurement_kit/common.hpp>
#include <measurement_kit/nettests.hpp>
#include <nan.h>
#include <uv.h>

extern "C" {
static void mkuv_resume(uv_async_t *handle);
static void mkuv_destroy(uv_handle_t *handle);
}

namespace mk {
namespace node {

// # RunningNettest
//
// The `RunningNettest` class takes care of running a generic Nettest in the
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
// starting the test, consistently with the pattern that we want memory to
// be available as long as we are using it. The self reference is then removed
// when the `async` object used to communicate with libuv has been successfully
// closed by libuv itself (i.e. when it is safe to dispose of memory).
class RunningNettest {
  public:
    // ## Constructor
    //
    // The constructor takes ownership of the specific test instance and
    // stores it into a publicly accessible smart pointer than can be later
    // used to further configure the test instance.
    RunningNettest(nettests::BaseTest *ptr) : nettest{ptr} {}

    // ## Resume
    //
    // The Resume() static async method is called by libuv's I/O loop thread
    // to resume all the callbacks on which we have suspended.
    static void Resume(Var<RunningNettest> self) {
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
    // The RunOrStart() static async method is called by the Nettest Node
    // object when the user has asked it to run/start the test.
    static void RunOrStart(Var<RunningNettest> self) {
        std::unique_lock<std::recursive_mutex> _{self->mutex};

        // This method is where we bind the shared pointer to this object with
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
        // we will call one last time libuv's event loop, this time to tell
        // libuv that we want to close our async handle. In turn, libuv will
        // call the `mkuv_destroy` callback when the handle has been closed
        // and there we will remove the self reference, so that we can finally
        // free this object. Important things to remember here:
        //
        // 1. we must call `uv_close()` from libuv's loop thread (i.e. we
        //    must call `SuspendOn()`, because otherwise on Linux the callback
        //    won't ever be called and we will hang forever;
        //
        // 2. memory must be freed after the callback has been called because,
        //    before that, libuv is still using the memory and it would not
        //    be polite to destroy while it is manipulating it.
        //
        // 3. after the async handle has been closed, the libuv event loop
        //    may exit if the async handle was previously the only task
        //    keeping the event loop alive (i.e. if Node was only running
        //    this test, it will exit at the end of this test).
        self->nettest->on_destroy([self]() {
            self->SuspendOn([self]() {
                uv_close((uv_handle_t *)&self->async, mkuv_destroy);
            });
        });

        // Whenever an MK callback is called, this method captures all the
        // persistent state pertaining to such callback into a lambda closure
        // and suspends the execution of the lambda. Such execution will
        // resume when libuv will callback us from its main I/O loop.
        //
        // The act of adding a lambda to a list of suspended lambda requires
        // us to lock a mutex to prevent data races. Apart from that, the
        // act of invoking callabacks should not, in my opinion, require any
        // locking because we should not modify the callbacks after the
        // test has been started.

        if (!!self->log_cb) {
            self->nettest->on_log([self](uint32_t level, const char *msg) {
                self->SuspendOn([ self, level, mcopy = std::string(msg) ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {Nan::New(level),
                            Nan::New(mcopy).ToLocalChecked()};
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

        // This method could either run asynchronously or synchronously, and
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

        // At the end of the method, we dispose of our reference to the
        // Nettest. This shouldn't be necessary but it's additional
        // precaution to make sure there are no reference loops. (Note
        // that, in MK, running a test means that an internal object
        // is copied from the user-visible test-structure fields, and
        // the test actually runs using such internal object.)

        self->nettest = nullptr;
    }

    // ## Public Attributes
    // The following are public fields used by Node's test objects to
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

    // The nettest field contains the test that should be run. This field is
    // reset after the test has started (i.e. if the user attempts to use again
    // this object after a test is started, runtime_errors will be thrown).
    Var<nettests::BaseTest> nettest;

    // The progress_cb field is an optional callback called to update the
    // user regarding the progress of the test (in percentage).
    Var<Nan::Callback> progress_cb;

    // The selfref field is used to keep the object alive until it can be
    // safely disposed. A copy of this object should be held by C code when
    // it is manipulating us, to make sure that we'll be safe until the
    // stack of the C function has unwind.
    Var<RunningNettest> selfref;

  private:
    // ## Private Methods and Attributes

    // The SuspendOn private method registers a specific function (passed
    // by move) to be called later in the context of libuv I/O thread. And,
    // in addition, this method also tells libuv I/O thread that we have
    // one or more callback we would like to execute in its context.
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

    // The mutex structure protects suspended and async (and possibly other
    // fields) from modification by concurrent threads of execution.
    std::recursive_mutex mutex;
};

} // namespace node
} // namespace mk

// ## RunningNettest's C Callbacks

// The mkuv_resume C callback is called by libuv's I/O loop thread to give us
// a chance of running the suspended callbacks in its context. As an extra
// safety (and correctness) measure, we keep on the stack a copy of the smart
// pointer keeping alive the RunningNettest structure.
static void mkuv_resume(uv_async_t *handle) {
    using namespace mk::node;
    using namespace mk;
    auto selfref = static_cast<RunningNettest *>(handle->data)->selfref;
    RunningNettest::Resume(selfref);
}

// The mkuv_destroy C callback is called by libuv's I/O loop thread when
// libuv has successfully closed the async handle. Since we schedule closure
// of the handle after MK's internal object for running the test is gone,
// this means there are no other users of the RunningNettest structure (since
// the two user's are MK's test thread and libuv I/O loop thread). Hence,
// we can now destroy the RunningNettest by removing the self reference.
static void mkuv_destroy(uv_handle_t *handle) {
    using namespace mk::node;
    using namespace mk;
    auto selfref = static_cast<RunningNettest *>(
            reinterpret_cast<uv_async_t *>(handle)->data)
                           ->selfref;
    selfref->selfref = nullptr;
}

namespace mk {
namespace node {

// # NettestWrap
//
// The NettestWrap class template is the Node-visible object. Internally it
// contains a smart pointer to a NettestRunner. Methods on the NettestWrap
// template class will actually set the NettestRunner internals. When it will
// be time to run the test, this template class will basically defer all the
// work to the NettestRunner class.
//
// After you have called either Run or Start, it will be an error to attempt
// to call any other method of the test again. The code at this level will
// check that and throw an exception if that occurs.
template <typename Nettest> class NettestWrap : public Nan::ObjectWrap {
  public:
    // ## Constructors

    // The Initialize static method will create the function template that
    // JavaScript will use to create an instance of this class, and will
    // store such function into the exports object.
    static void Initialize(const char *cname, v8::Local<v8::Object> exports) {
        Nan::HandleScope scope;

        // The function template will cause the New static method of this
        // class to be invoked for constructing a new object.
        v8::Local<v8::String> name = Nan::New(cname).ToLocalChecked();
        auto ctor = Nan::New<v8::FunctionTemplate>(New);

        // We configure the function template such that an initial object is
        // created that is bound to the specific class name, has an internal
        // field for communication with C++, and has all the required methods.
        ctor->SetClassName(name);
        ctor->InstanceTemplate()->SetInternalFieldCount(1);
        Nan::SetPrototypeMethod(ctor, "add_input", AddInput);
        Nan::SetPrototypeMethod(ctor, "add_input_filepath", AddInputFilepath);
        Nan::SetPrototypeMethod(ctor, "set_error_filepath", SetErrorFilepath);
        Nan::SetPrototypeMethod(ctor, "set_options", SetOptions);
        Nan::SetPrototypeMethod(ctor, "set_output_filepath", SetOutputFilepath);
        Nan::SetPrototypeMethod(ctor, "set_verbosity", SetVerbosity);
        Nan::SetPrototypeMethod(ctor, "on_begin", OnBegin);
        Nan::SetPrototypeMethod(ctor, "on_end", OnEnd);
        Nan::SetPrototypeMethod(ctor, "on_entry", OnEntry);
        Nan::SetPrototypeMethod(ctor, "on_event", OnEvent);
        Nan::SetPrototypeMethod(ctor, "on_log", OnLog);
        Nan::SetPrototypeMethod(ctor, "on_progress", OnProgress);
        Nan::SetPrototypeMethod(ctor, "run", Run);
        Nan::SetPrototypeMethod(ctor, "start", Start);

        // Once we have configured the function template, we register it into
        // the exports, so that it is not garbage collected. This MUST be the
        // last action we perform. Doing it earlier will mean that not all
        // the methods will be available from Node.
        exports->Set(name, ctor->GetFunction());
    }

    // The New static method is the JavaScript object constructor. We store
    // a pointer to `NettestWrap` into the internal field. In turn, such
    // pointer will contain a smart pointer to a NettestRunner. The reason
    // why we have a second layer of indirection with NettestRunner is
    // to keep separate internal and external objects and thus simplify
    // reference based memory management (been there, learned that).
    static void New(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        if (!info.IsConstructCall()) {
            Nan::ThrowError("not invoked as constructor");
            return;
        }
        if (info.Length() != 0) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        NettestWrap *nw = new NettestWrap{};
        nw->running.reset(new RunningNettest{new Nettest{}});
        nw->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    // ## Value Setters

    // The AddInput setter adds one input string to the list of input strings
    // to be processed by this test. If the test takes no input, adding one
    // extra input has basically no visible effect.
    static void AddInput(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->nettest->add_input(*v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The AddInput setter adds one input file to the list of input file
    // to be processed by this test. If the test takes no input, adding one
    // extra input file has basically no visible effect.
    static void AddInputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->nettest->add_input_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetErrorFilepath setter sets the path where logs will be written. Not
    // setting the error filepath will prevent logs from being written.
    static void SetErrorFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->nettest->set_error_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetOptions setter allows to set test-specific options. You should
    // consult MK documentation for more information.
    static void SetOptions(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(2, info, [&info](Var<RunningNettest> r) {
            r->nettest->set_options(*v8::String::Utf8Value{info[0]->ToString()},
                    *v8::String::Utf8Value{info[1]->ToString()});
        });
    }

    // The SetErrorFilepath setter sets the path where report will be written.
    // Not setting the output filepath will cause MK to attempt to write the
    // report on an output filepath with a test- and time-dependent name.
    static void SetOutputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->nettest->set_output_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetVerbosity setter allows to set logging verbosity. Zero is
    // equivalent to WARNING, one to INFO, two to DEBUG and more than two
    // makes MK even more verbose.
    static void SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->nettest->set_verbosity(info[0]->Uint32Value());
        });
    }

    // ## Callback Setters

    // The OnBegin setter allows to set the callback called right at the
    // beginning of the network test.
    static void OnBegin(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->begin_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEnd setter allows to set the callback called after all the
    // measurements have been performed and before closing the report.
    static void OnEnd(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->end_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEnd setter allows to set the callback called after each
    // measurement with the current entry as argument.
    static void OnEntry(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->entry_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEnd setter allows to set the callback called during the test
    // to report test-specific events that occurred.
    static void OnEvent(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->event_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnEnd setter allows to set the callback called for each log line
    // emitted by the test. Not setting this callback means that MK will
    // attempt to write logs on the standard error.
    static void OnLog(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->log_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // The OnProgress setter allows to set the callback called to inform you
    // about the progress of the test in percentage.
    static void OnProgress(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](Var<RunningNettest> r) {
            r->progress_cb.reset(new Nan::Callback{info[0].As<v8::Function>()});
        });
    }

    // ## Runners

    // The Run static method runs the test synchronously. This will block Node
    // until the test is over. It does not make much sense in the Node context,
    // but is an MK API and so we provide it here.
    static void Run(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        RunOrStart(0, info);
    }

    // The Start static method runs the test asynchronously and calls the
    // callback passed as argument when the test is done.
    static void Start(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        RunOrStart(1, info);
    }

    // ## Internals

  private:
    // The SetValue static method is a convenience method. It will check the
    // required number of arguments and whether the internal NettestRunner has
    // been already consumed by running a test or not.
    static void SetValue(int argc,
            const Nan::FunctionCallbackInfo<v8::Value> &info,
            std::function<void(Var<RunningNettest>)> &&next) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        if (!This(info)->running) {
            Nan::ThrowError("cannot modify object with test running");
            return;
        }
        next(This(info)->running);
        info.GetReturnValue().Set(info.This());
    }

    // The This static method is a convenience method used by many others.
    static NettestWrap *This(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        return ObjectWrap::Unwrap<NettestWrap>(info.Holder());
    }

    // The RunOrStart static method implements Run and Start. It is important
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
        if (!This(info)->running) {
            Nan::ThrowError("cannot modify object with test running");
            return;
        }
        Var<RunningNettest> running;
        std::swap(running, This(info)->running);
        if (argc >= 1) {
            running->complete_cb.reset(
                    new Nan::Callback(info[0].As<v8::Function>()));
        }
        RunningNettest::RunOrStart(running);
    }

    // The running internal field is the shared pointer reference to the
    // RunningNettest that will be used to actually run the test.
    Var<RunningNettest> running;
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
