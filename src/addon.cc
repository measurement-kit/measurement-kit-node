#include <nan.h>
#include "nettests/dash.hpp"
#include "nettests/dns_injection.hpp"
#include "nettests/http_header_field_manipulation.hpp"
#include "nettests/http_invalid_request_line.hpp"
#include "nettests/meek_fronted_requests.hpp"
#include "nettests/multi_ndt.hpp"
#include "nettests/ndt.hpp"
#include "nettests/tcp_connect.hpp"
#include "nettests/web_connectivity.hpp"

using v8::FunctionTemplate;
using v8::Handle;
using v8::Object;
using v8::String;
using Nan::GetFunction;
using Nan::New;
using Nan::Set;

void InitAll(v8::Local<v8::Object> exports) {
  DashTest::Init(exports);
  DnsInjectionTest::Init(exports);
  HttpHeaderFieldManipulationTest::Init(exports);
  HttpInvalidRequestLineTest::Init(exports);
  MeekFrontedRequestsTest::Init(exports);
  MultiNdtTest::Init(exports);
  NdtTest::Init(exports);
  TcpConnectTest::Init(exports);
  WebConnectivityTest::Init(exports);
}

NODE_MODULE(addon, InitAll)
