// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_NETTEST_WRAP_HPP
#define PRIVATE_NODE_NETTEST_WRAP_HPP

// TODO: cleanup headers
#include "private/node/uv_async_ctx.hpp"
#include "private/node/wrap_callback.hpp"
#include <list>
#include <measurement_kit/common.hpp>
#include <measurement_kit/nettests.hpp>
#include <mutex>
#include <nan.h>
#include <uv.h>
#include <iostream>

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
        nw->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }

    NettestWrap() {
        async = UvAsyncCtx<>::Make();
        nettest.reset(new Nettest);
    }

    // ## Value Setters

    // The AddInput setter adds one input string to the list of input strings
    // to be processed by this test. If the test takes no input, adding one
    // extra input has basically no visible effect.
    static void AddInput(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->add_input(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The AddInputFilepath setter adds one input file to the list of input file
    // to be processed by this test. If the test takes no input, adding one
    // extra input file has basically no visible effect.
    static void AddInputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->add_input_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetErrorFilepath setter sets the path where logs will be written. Not
    // setting the error filepath will prevent logs from being written on disk.
    static void SetErrorFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->set_error_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetOptions setter allows to set test-specific options. You should
    // consult MK documentation for more information on available options.
    static void SetOptions(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(2, info, [&info](NettestWrap *self) {
            self->nettest->set_options(
                    *v8::String::Utf8Value{info[0]->ToString()},
                    *v8::String::Utf8Value{info[1]->ToString()});
        });
    }

    // The SetOutputFilepath setter sets the path where test report will be
    // written. Not setting the output filepath will cause MK to try to write
    // the report on an filepath with a test- and time-dependent name.
    static void SetOutputFilepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->set_output_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The SetVerbosity setter allows to set logging verbosity. Zero is
    // equivalent to WARNING, one to INFO, two to DEBUG and more than two
    // makes MK even more verbose.
    static void SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->set_verbosity(info[0]->Uint32Value());
        });
    }

    // ## Callback Setters

    // The OnBegin setter allows to set the callback called right at the
    // beginning of the network test.
    static void OnBegin(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_begin([
                async = self->async, callback = WrapCallback(info[0])
            ]() {
                UvAsyncCtx<>::SuspendOn(async, [callback]() {
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        });
    }

    // The OnEnd setter allows to set the callback called after all
    // measurements have been performed and before closing the report.
    static void OnEnd(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_end([
                async = self->async, callback = WrapCallback(info[0])
            ]() {
                UvAsyncCtx<>::SuspendOn(async, [callback]() {
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        });
    }

    // The OnEntry setter allows to set the callback called after each
    // measurement. The callback receives a serialized JSON as argument.
    static void OnEntry(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_entry([
                async = self->async, callback = WrapCallback(info[0])
            ](std::string s) {
                UvAsyncCtx<>::SuspendOn(async, [callback, s]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                        Nan::New(s).ToLocalChecked()
                    };
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The OnEvent setter allows to set the callback called during the test
    // to report test-specific events that occurred.
    static void OnEvent(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_event([
                async = self->async, callback = WrapCallback(info[0])
            ](const char *s) {
                UvAsyncCtx<>::SuspendOn(async, [
                    callback, s = std::string(s)
                ]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                        Nan::New(s).ToLocalChecked()
                    };
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The OnLog setter allows to set the callback called for each log line
    // emitted by the test. Not setting this callback means that MK will
    // attempt to write logs on the standard error.
    static void OnLog(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_log([
                async = self->async, callback = WrapCallback(info[0])
            ](uint32_t level, const char *s) {
                UvAsyncCtx<>::SuspendOn(async, [
                    callback, level, s = std::string(s)
                ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {
                        Nan::New(level),
                        Nan::New(s).ToLocalChecked()
                    };
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The OnProgress setter allows to set the callback called to inform you
    // about the progress of the test in percentage.
    static void OnProgress(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        SetValue(1, info, [&info](NettestWrap *self) {
            self->nettest->on_progress([
                async = self->async, callback = WrapCallback(info[0])
            ](double percentage, const char *s) {
                UvAsyncCtx<>::SuspendOn(async, [
                    callback, percentage, s = std::string(s)
                ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {
                        Nan::New(percentage),
                        Nan::New(s).ToLocalChecked()
                    };
                    callback->Call(argc, argv);
                });
            });
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
            std::function<void(NettestWrap *)> &&next) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        next(This(info));
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
        std::clog << "Use count nettest: " << This(info)->nettest.use_count() << std::endl;
        std::clog << "Use count async: " << This(info)->async.use_count() << std::endl;
        This(info)->nettest->on_destroy([
            async = This(info)->async
        ]() {
            UvAsyncCtx<>::StartDelete(async);
        });
        if (argc >= 1) {
            This(info)->nettest->start([
                async = This(info)->async, callback = WrapCallback(info[0])
            ]() {
                std::clog << "Use count async: " << async.use_count() << std::endl;
                UvAsyncCtx<>::SuspendOn(async, [callback]() {
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        } else {
            This(info)->nettest->run();
        }
        std::clog << "Use count nettest: " << This(info)->nettest.use_count() << std::endl;
        std::clog << "Use count async: " << This(info)->async.use_count() << std::endl;
        //This(info)->nettest.reset();
        //This(info)->async.reset();
    }

    Var<UvAsyncCtx<>> async;
    Var<Nettest> nettest;
};

} // namespace node
} // namespace mk

#endif
