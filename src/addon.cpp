#include <nan.h>
#include "nettests/nettests.hpp"   // NOLINT(build/include)

using v8::FunctionTemplate;
using v8::Handle;
using v8::Object;
using v8::String;
using Nan::GetFunction;
using Nan::New;
using Nan::Set;

void InitAll(v8::Local<v8::Object> exports) {
  WebConnectivityTest::Init(exports);
}

NODE_MODULE(addon, InitAll)
