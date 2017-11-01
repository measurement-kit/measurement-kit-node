// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_NETTEST_WRAP_HPP
#define PRIVATE_NODE_NETTEST_WRAP_HPP

#include "private/node/uv_async_ctx.hpp"
#include "private/node/wrap_callback.hpp"
#include <measurement_kit/nettests.hpp>
#include <nan.h>

namespace mk {
namespace node {

// # NettestWrap
//
// The NettestWrap class template is the Node-visible object. It wraps
// a mk::nettest::<T> test instance and uses a mk::node::UvAsyncCtx<> context
// to safely route test callbacks to libuv I/O loop (i.e. Node loop).
template <typename Nettest> class NettestWrap : public Nan::ObjectWrap {
  public:
    // ## Constructors

    // The static constructor() factory is used to retrieve the Node function
    // called when the user doesn't use `new` to create an object, e.g.:
    //
    // ```C++
    //     let foo = FooTest()
    // ```
    //
    // We use the static factory pattern to access the Node constructor, rather
    // than declaring a static C++ field, to avoid compiler warnings and
    // refactoring issues caused by the facts that:
    //
    // 1. when you define a static attribute, you should also instantiate it
    //    in the C++ file
    //
    // 2. this is a template, hence it's more syntax work to instantiate
    //    also considering that we don't have a C++ file
    //
    // Thus, a static factory makes things simpler.
    static Nan::Persistent<v8::Function> &constructor() {
        static Nan::Persistent<v8::Function> instance;
        return instance;
    }

    // The initialize() static method will create the function template that
    // JavaScript will use to create an instance of this class, and will
    // store such function template into the exports object.
    static void initialize(const char *cname, v8::Local<v8::Object> exports) {
        Nan::HandleScope scope;

        // initialize() will bind the function template to the make() method.
        v8::Local<v8::String> name = Nan::New(cname).ToLocalChecked();
        auto tpl = Nan::New<v8::FunctionTemplate>(make);

        // initialize() will also perform other initialization actions, e.g.
        // adding all the methods to JavaScript object's prototype.
        tpl->SetClassName(name);
        tpl->InstanceTemplate()->SetInternalFieldCount(1);
        Nan::SetPrototypeMethod(tpl, "add_input", add_input);
        Nan::SetPrototypeMethod(tpl, "add_input_filepath", add_input_filepath);
        Nan::SetPrototypeMethod(tpl, "set_error_filepath", set_error_filepath);
        Nan::SetPrototypeMethod(tpl, "set_options", set_options);
        Nan::SetPrototypeMethod(
                tpl, "set_output_filepath", set_output_filepath);
        Nan::SetPrototypeMethod(tpl, "set_verbosity", set_verbosity);
        Nan::SetPrototypeMethod(tpl, "on_begin", on_begin);
        Nan::SetPrototypeMethod(tpl, "on_end", on_end);
        Nan::SetPrototypeMethod(tpl, "on_entry", on_entry);
        Nan::SetPrototypeMethod(tpl, "on_event", on_event);
        Nan::SetPrototypeMethod(tpl, "on_log", on_log);
        Nan::SetPrototypeMethod(tpl, "on_progress", on_progress);
        Nan::SetPrototypeMethod(tpl, "run", run);
        Nan::SetPrototypeMethod(tpl, "start", start);

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

    // The make() static method is the JavaScript object "constructor".
    // Here we deal both with the case where `new` is used (e.g.
    // `let foo = new FooTest()`) and with the case in which instead
    // `new` is not used (e.g. `let foo = FooTest()`).
    static void make(const Nan::FunctionCallbackInfo<v8::Value> &info) {
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

    // NettestWrap() is the C++ constructor. It creates an instance of the
    // UvAsyncCtx<> context to route callbacks from C++ to Node.
    NettestWrap() { async_ctx = UvAsyncCtx<>::make(); }

    // ## Value Setters

    // The add_input setter adds one input string to the list of input strings
    // to be processed by this test. If the test takes no input, adding one
    // extra input has basically no visible effect.
    static void add_input(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.add_input(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The add_input_filepath setter adds one input file to the list of input
    // file to be processed by this test. If the test takes no input, adding one
    // extra input file has basically no visible effect.
    static void add_input_filepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.add_input_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The set_error_filepath setter sets the path where logs will be written.
    // Not setting the error filepath will prevent logs from being written on
    // disk.
    static void set_error_filepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.set_error_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The set_options setter allows to set test-specific options. You should
    // consult MK documentation for more information on available options.
    static void set_options(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(2, info, [&info](NettestWrap *self) {
            self->nettest.set_options(
                    *v8::String::Utf8Value{info[0]->ToString()},
                    *v8::String::Utf8Value{info[1]->ToString()});
        });
    }

    // The set_output_filepath setter sets the path where test report will be
    // written. Not setting the output filepath will cause MK to try to write
    // the report on an filepath with a test- and time-dependent name.
    static void set_output_filepath(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.set_output_filepath(
                    *v8::String::Utf8Value{info[0]->ToString()});
        });
    }

    // The set_verbosity setter allows to set logging verbosity. Zero is
    // equivalent to WARNING, one to INFO, two to DEBUG and more than two
    // makes MK even more verbose.
    static void set_verbosity(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.set_verbosity(info[0]->Uint32Value());
        });
    }

    // ## Callback Setters

    // The on_begin setter allows to set the callback called right at the
    // beginning of the network test.
    static void on_begin(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_begin([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ]() {
                UvAsyncCtx<>::suspend(async_ctx, [callback]() {
                    // Implementation note: even if it seems superfluous, here
                    // we must add the scope otherwise the following call is
                    // going to fail because it's missing a scope.
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        });
    }

    // The on_end setter allows to set the callback called after all
    // measurements have been performed and before closing the report.
    static void on_end(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_end([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ]() {
                UvAsyncCtx<>::suspend(async_ctx, [callback]() {
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        });
    }

    /* clang-format off */

    // The on_entry setter allows to set the callback called after each
    // measurement. The callback receives a serialized JSON as argument.
    static void on_entry(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_entry([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ](std::string s) {
                UvAsyncCtx<>::suspend(async_ctx, [callback, s]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(s).ToLocalChecked()};
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The on_event setter allows to set the callback called during the test
    // to report test-specific events that occurred.
    static void on_event(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_event([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ](const char *s) {
                UvAsyncCtx<>::suspend(async_ctx, [
                        callback, s = std::string(s)
                ]() {
                    Nan::HandleScope scope;
                    const int argc = 1;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(s).ToLocalChecked()};
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The on_log setter allows to set the callback called for each log line
    // emitted by the test. Not setting this callback means that MK will
    // attempt to write logs on the standard error.
    static void on_log(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_log([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ](uint32_t level, const char *s) {
                UvAsyncCtx<>::suspend(
                        async_ctx, [callback, level, s = std::string(s) ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {Nan::New(level),
                            Nan::New(s).ToLocalChecked()};
                    callback->Call(argc, argv);
                });
            });
        });
    }

    // The on_progress setter allows to set the callback called to inform you
    // about the progress of the test in percentage.
    static void on_progress(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        set_value(1, info, [&info](NettestWrap *self) {
            self->nettest.on_progress([
                async_ctx = self->async_ctx, callback = wrap_callback(info[0])
            ](double percentage, const char *s) {
                UvAsyncCtx<>::suspend(async_ctx, [
                        callback, percentage, s = std::string(s)
                ]() {
                    Nan::HandleScope scope;
                    const int argc = 2;
                    v8::Local<v8::Value> argv[argc] = {
                            Nan::New(percentage),
                            Nan::New(s).ToLocalChecked()};
                    callback->Call(argc, argv);
                });
            });
        });
    }

    /* clang-format on */

    // ## runners

    // The run method runs the test synchronously. This will block Node until
    // the test is over. Perhaps not what you want in the common case, but
    // it may be useful in some specific corner cases.
    static void run(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        run_or_start(0, info);
    }

    // The start method runs the test asynchronously and calls the callback
    // passed as argument when the test is done. Note that calling this method
    // will cause Node's event loop to wait for the test to finish, but,
    // unlike run, start will allow you to do also other things while you're
    // waiting for the test to finish.
    static void start(const Nan::FunctionCallbackInfo<v8::Value> &info) {
        run_or_start(1, info);
    }

    // ## Internals

  private:
    // The set_value method is a convenience method. It will make sure
    // that the number of arguments is the expected one. It also check whether
    // either run() or start() was already called and throw in such case.
    static void set_value(int argc,
            const Nan::FunctionCallbackInfo<v8::Value> &info,
            std::function<void(NettestWrap *)> &&next) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        next(get_this(info));
        info.GetReturnValue().Set(info.This());
    }

    // The get_this() method is a convenience method used by many others to
    // quickly get the `this` pointer of the class.
    static NettestWrap *get_this(
            const Nan::FunctionCallbackInfo<v8::Value> &info) {
        return ObjectWrap::Unwrap<NettestWrap>(info.Holder());
    }

    // The run_or_start method implements run() and start().
    static void run_or_start(
            int argc, const Nan::FunctionCallbackInfo<v8::Value> &info) {
        Nan::HandleScope scope;
        if (info.Length() != argc) {
            Nan::ThrowError("invalid number of arguments");
            return;
        }
        get_this(info)->nettest.on_destroy([
                async_ctx = get_this(info)->async_ctx
        ]() {
            UvAsyncCtx<>::start_delete(async_ctx);
        });
        if (argc >= 1) {
            get_this(info)->nettest.start([
                async_ctx = get_this(info)->async_ctx,
                callback = wrap_callback(info[0])
            ]() {
                UvAsyncCtx<>::suspend(async_ctx, [callback]() {
                    Nan::HandleScope scope;
                    callback->Call(0, nullptr);
                });
            });
        } else {
            get_this(info)->nettest.run();
        }
    }

    // Async is the object used to route MK callbacks to libuv loop.
    SharedPtr<UvAsyncCtx<>> async_ctx;

    // Nettest is the test we want to execute.
    Nettest nettest;
};

} // namespace node
} // namespace mk

#endif
