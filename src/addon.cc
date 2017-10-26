// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "private/node/nettest_wrap.hpp"

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
