#include "web_connectivity.hpp"

/*
 * For examples see:
 * * https://github.com/nodejs/node-addon-examples
 * * https://github.com/fcanas/node-native-boilerplate
 *
 */

Nan::Persistent<v8::Function> WebConnectivityTest::constructor;

WebConnectivityTest::WebConnectivityTest(v8::Local<v8::Object> options) : options_(options) {
}

WebConnectivityTest::~WebConnectivityTest() {
}

void WebConnectivityTest::Init(v8::Local<v8::Object> exports) {

  // Prepare constructor template
  Nan::HandleScope scope;
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("WebConnectivityTest").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "add_input_filepath", AddInputFilePath);
  Nan::SetPrototypeMethod(tpl, "add_input", AddInput);

  // These are common to all tests. We should move this into some base class
  Nan::SetPrototypeMethod(tpl, "set_options", SetOptions);
  Nan::SetPrototypeMethod(tpl, "set_verbosity", SetVerbosity);
  Nan::SetPrototypeMethod(tpl, "on_progress", OnProgress);
  Nan::SetPrototypeMethod(tpl, "on_log", OnLog);
  Nan::SetPrototypeMethod(tpl, "run", Run);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("WebConnectivityTest").ToLocalChecked(), tpl->GetFunction());
}


void WebConnectivityTest::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new WebConnectivityTest(...)`
		v8::Local<v8::Object> options;
    if (info[0]->IsUndefined() || !info[0]->IsObject()) {
      options = Nan::New<v8::Object>();
    } else {
      options = info[0]->ToObject();
    }
    WebConnectivityTest* obj = new WebConnectivityTest(options);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // Invoked as plain function `WebConnectivityTest(...)`, turn into construct call.
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = { info[0] };
    v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}

void WebConnectivityTest::SetOptions(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 2);

  WebConnectivityTest* obj = ObjectWrap::Unwrap<WebConnectivityTest>(info.Holder());

  v8::String::Utf8Value utf8Name(info[0]->ToString());
  v8::String::Utf8Value utf8Value(info[1]->ToString());
  const auto name = std::string(*utf8Name);
  const auto value = std::string(*utf8Value);
  obj->test.set_options(name, value);
}


void WebConnectivityTest::SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  WebConnectivityTest* obj = ObjectWrap::Unwrap<WebConnectivityTest>(info.Holder());
  const uint32_t level = info[0]->Uint32Value();
  obj->test.set_verbosity(level);
}

void WebConnectivityTest::OnProgress(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
  // What we probably want to do is write a lambda that wraps the native C++
  // callback and calls the v8 callback by doing something like:

  /*
  v8::Local<v8::Function> cb = info[0].As<v8::Function>();
  const unsigned argc = 1;
  v8::Local<v8::Value> argv[argc] = { Nan::New("hello world").ToLocalChecked() };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  */
}

void WebConnectivityTest::OnLog(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
}

void WebConnectivityTest::AddInputFilePath(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
}

void WebConnectivityTest::AddInput(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  WebConnectivityTest* obj = ObjectWrap::Unwrap<WebConnectivityTest>(info.Holder());
  v8::String::Utf8Value utf8Input(info[0]->ToString());
  const auto input = std::string(*utf8Input);
  obj->test.add_input(input);
}

void WebConnectivityTest::Run(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  v8::Local<v8::Function> cb = info[0].As<v8::Function>();

  WebConnectivityTest* obj = ObjectWrap::Unwrap<WebConnectivityTest>(info.Holder());

  // XXX this should actually be done wrapping the Async methods of v8
  obj->test.run();

  const unsigned argc = 1;
  v8::Local<v8::Value> argv[argc] = { Nan::New("ok").ToLocalChecked() };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
}
