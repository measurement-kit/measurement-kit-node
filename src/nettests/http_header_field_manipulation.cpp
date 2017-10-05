// This file is autogenerated by measurement-kit-node do not edit.

#include "http_header_field_manipulation.hpp"

Nan::Persistent<v8::Function> HttpHeaderFieldManipulationTest::constructor;

HttpHeaderFieldManipulationTest::HttpHeaderFieldManipulationTest(v8::Local<v8::Object> options) : options_(options) {
}

HttpHeaderFieldManipulationTest::~HttpHeaderFieldManipulationTest() {
}

void HttpHeaderFieldManipulationTest::Init(v8::Local<v8::Object> exports) {
  Nan::HandleScope scope;
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("HttpHeaderFieldManipulationTest").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "add_input_filepath", AddInputFilepath);
  Nan::SetPrototypeMethod(tpl, "add_input", AddInput);

  Nan::SetPrototypeMethod(tpl, "set_output_filepath", SetOutputFilepath);
  Nan::SetPrototypeMethod(tpl, "set_error_filepath", SetErrorFilepath);

  Nan::SetPrototypeMethod(tpl, "set_options", SetOptions);
  Nan::SetPrototypeMethod(tpl, "set_verbosity", SetVerbosity);
  Nan::SetPrototypeMethod(tpl, "on_progress", OnProgress);
  Nan::SetPrototypeMethod(tpl, "on_log", OnLog);
  Nan::SetPrototypeMethod(tpl, "on_entry", OnEntry);
  Nan::SetPrototypeMethod(tpl, "on_event", OnEvent);
  Nan::SetPrototypeMethod(tpl, "on_end", OnEnd);
  Nan::SetPrototypeMethod(tpl, "on_begin", OnBegin);
  Nan::SetPrototypeMethod(tpl, "run", Run);
  Nan::SetPrototypeMethod(tpl, "start", Start);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("HttpHeaderFieldManipulationTest").ToLocalChecked(), tpl->GetFunction());
}


void HttpHeaderFieldManipulationTest::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new HttpHeaderFieldManipulationTest(...)`
		v8::Local<v8::Object> options;
    if (info[0]->IsUndefined() || !info[0]->IsObject()) {
      options = Nan::New<v8::Object>();
    } else {
      options = info[0]->ToObject();
    }

    // XXX how do we abstract this so that objects don't get sliced and we
    // don't have to duplicate this in every sub-class?
    HttpHeaderFieldManipulationTest* obj = new HttpHeaderFieldManipulationTest(options);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // Invoked as plain function `HttpHeaderFieldManipulationTest(...)`, turn into construct call.
    const int argc = 1;
    v8::Local<v8::Value> argv[] = { info[0] };
    v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}

void HttpHeaderFieldManipulationTest::SetOptions(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 2);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  v8::String::Utf8Value utf8Name(info[0]->ToString());
  v8::String::Utf8Value utf8Value(info[1]->ToString());
  const auto name = std::string(*utf8Name);
  const auto value = std::string(*utf8Value);
  obj->test.set_options(name, value);
}


void HttpHeaderFieldManipulationTest::SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());
  const uint32_t level = info[0]->Uint32Value();
  obj->test.set_verbosity(level);
}

void HttpHeaderFieldManipulationTest::OnProgress(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
  // What we probably want to do is write a lambda that wraps the native C++
  // callback and calls the v8 callback by doing something like:

  assert(info.Length() >= 1);

  /*
  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.on_progress([&](double percent, std::string msg) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 2;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(percent),
      Nan::New(msg).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
  */
}

void HttpHeaderFieldManipulationTest::OnLog(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>> pCallback(
    isolate,
    info[0].As<v8::Function>()
  );

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());
  obj->test.on_log([pCallback](int level, const char *s) {
    v8::Local<v8::Function> cb = Nan::New(pCallback);

    // XXX We need to handle the deletion of these somewhere
    // gCallback.Reset();
    const int argc = 2;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(level),
      Nan::New(std::string(s)).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
}

void HttpHeaderFieldManipulationTest::AddInputFilepath(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
}

void HttpHeaderFieldManipulationTest::AddInput(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());
  v8::String::Utf8Value utf8Input(info[0]->ToString());
  const auto input = std::string(*utf8Input);
  obj->test.add_input(input);
}

void HttpHeaderFieldManipulationTest::OnEntry(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  /*
  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.on_entry([&](std::string s) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(s).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
  */
}

void HttpHeaderFieldManipulationTest::OnEvent(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  /*
  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.on_event([&](const char *s) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(std::string(s)).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
  */
}

void HttpHeaderFieldManipulationTest::OnEnd(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  /*
  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.on_end([&]() {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {
      Nan::Null()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
  */
}

void HttpHeaderFieldManipulationTest::OnBegin(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  /*
  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.on_begin([&]() {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {
      Nan::Null()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
  */
}

void HttpHeaderFieldManipulationTest::SetErrorFilepath(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  v8::String::Utf8Value utf8Path(info[0]->ToString());
  const auto path = std::string(*utf8Path);
  obj->test.set_error_filepath(path);
}

void HttpHeaderFieldManipulationTest::SetOutputFilepath(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  v8::String::Utf8Value utf8Path(info[0]->ToString());
  const auto path = std::string(*utf8Path);
  obj->test.set_output_filepath(path);
}

void HttpHeaderFieldManipulationTest::Start(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.start([&](){
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const unsigned argc = 1;
    v8::Local<v8::Value> argv[argc] = {
      Nan::Null()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
}

void HttpHeaderFieldManipulationTest::Run(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // This method is blocking
  assert(info.Length() >= 1);

  v8::Local<v8::Function> cb = info[0].As<v8::Function>();

  HttpHeaderFieldManipulationTest* obj = ObjectWrap::Unwrap<HttpHeaderFieldManipulationTest>(info.Holder());

  obj->test.run();

  const unsigned argc = 1;
  v8::Local<v8::Value> argv[argc] = {
    Nan::Null()
  };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
}
