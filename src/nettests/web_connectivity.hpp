#ifndef MK_NODE_NETTESTS_WEB_CONNECTIVITY_H
#define MK_NODE_NETTESTS_WEB_CONNECTIVITY_H

#include <nan.h>
#include <measurement_kit/nettests.hpp>

class WebConnectivityTest : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);

 private:
  explicit WebConnectivityTest(v8::Local<v8::Object> options);
  ~WebConnectivityTest();

  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void SetOptions(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void SetVerbosity(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void OnProgress(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void OnLog(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void AddInputFilePath(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void AddInput(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void Run(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static Nan::Persistent<v8::Function> constructor;
  mk::nettests::WebConnectivityTest test;
  v8::Local<v8::Object> options_;
};

#endif
