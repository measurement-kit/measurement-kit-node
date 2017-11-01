// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_WRAP_CALLBACK_HPP
#define PRIVATE_NODE_WRAP_CALLBACK_HPP

#include "private/common/compat.hpp"
#include <nan.h>

namespace mk {
namespace node {

// The wrap_callback() free function is a syntactic sugar for converting
// a v8::Value into a Nan::Callback. The latter is a persistent type suitable
// for wrapping a function to be called at a later time. It turns out that
// such callback must be called from Node's main loop (i.e. uv_run()).
SharedPtr<Nan::Callback> wrap_callback(v8::Local<v8::Value> value) {
    return SharedPtr<Nan::Callback>{
            new Nan::Callback{value.As<v8::Function>()}};
}

} // namespace node
} // namespace mk
#endif
