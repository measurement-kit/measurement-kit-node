#include "{{ nettestSnakeNoSuffix }}.hpp"

Nan::Persistent<v8::Function> {{ nettestPascal}}::constructor;

{{ nettestPascal }}::{{ nettestPascal }}(v8::Local<v8::Object> options) : options_(options) {
}

{{ nettestPascal }}::~{{ nettestPascal }}() {
}

void {{ nettestPascal }}::Init(v8::Local<v8::Object> exports) {

  // Prepare constructor template
  Nan::HandleScope scope;
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("{{ nettestPascal }}").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "add_input_filepath", AddInputFilePath);
  Nan::SetPrototypeMethod(tpl, "add_input", AddInput);

  // These are common to all tests. We should move this into some base class
  Nan::SetPrototypeMethod(tpl, "set_options", SetOptions);
  Nan::SetPrototypeMethod(tpl, "set_verbosity", SetVerbosity);
  Nan::SetPrototypeMethod(tpl, "on_progress", OnProgress);
  Nan::SetPrototypeMethod(tpl, "on_log", OnLog);
  Nan::SetPrototypeMethod(tpl, "run", Run);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("{{ nettestPascal }}").ToLocalChecked(), tpl->GetFunction());
}


void {{ nettestPascal}}::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new {{ nettestPascal}}(...)`
		v8::Local<v8::Object> options;
    if (info[0]->IsUndefined() || !info[0]->IsObject()) {
      options = Nan::New<v8::Object>();
    } else {
      options = info[0]->ToObject();
    }

    // XXX how do we abstract this so that objects don't get sliced and we
    // don't have to duplicate this in every sub-class?
    {{ nettestPascal}}* obj = new {{ nettestPascal}}(options);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // Invoked as plain function `{{ nettestPascal}}(...)`, turn into construct call.
    const int argc = 1;
    v8::Local<v8::Value> argv[] = { info[0] };
    v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}

void {{ nettestPascal}}::SetOptions(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 2);

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());

  v8::String::Utf8Value utf8Name(info[0]->ToString());
  v8::String::Utf8Value utf8Value(info[1]->ToString());
  const auto name = std::string(*utf8Name);
  const auto value = std::string(*utf8Value);
  obj->test.set_options(name, value);
}


void {{ nettestPascal}}::SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());
  const uint32_t level = info[0]->Uint32Value();
  obj->test.set_verbosity(level);
}

void {{ nettestPascal}}::OnProgress(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
  // What we probably want to do is write a lambda that wraps the native C++
  // callback and calls the v8 callback by doing something like:

  assert(info.Length() >= 1);

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());

  obj->test.on_progress([&](double percent, std::string msg) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 2;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(percent),
      Nan::New(msg).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
}

void {{ nettestPascal}}::OnLog(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());

  // XXX am I passing in the info pointer properly?
  obj->test.on_log([&](int level, const char *s) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    const int argc = 2;
    v8::Local<v8::Value> argv[argc] = {
      Nan::New(level),
      Nan::New(std::string(s)).ToLocalChecked()
    };
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
  });
}

void {{ nettestPascal}}::AddInputFilePath(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  // See: https://github.com/nodejs/node-addon-examples/blob/master/3_callbacks/nan/addon.cc
}

void {{ nettestPascal}}::AddInput(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());
  v8::String::Utf8Value utf8Input(info[0]->ToString());
  const auto input = std::string(*utf8Input);
  obj->test.add_input(input);
}

void {{ nettestPascal}}::Run(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  assert(info.Length() >= 1);

  v8::Local<v8::Function> cb = info[0].As<v8::Function>();

  {{ nettestPascal}}* obj = ObjectWrap::Unwrap<{{ nettestPascal}}>(info.Holder());

  // XXX this should actually be done wrapping the Async methods of v8
  obj->test.run();

  const unsigned argc = 1;
  v8::Local<v8::Value> argv[argc] = { Nan::New("ok").ToLocalChecked() };
  Nan::MakeCallback(Nan::GetCurrentContext()->Global(), cb, argc, argv);
}