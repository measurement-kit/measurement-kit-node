// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_WRAP_CALLBACK_HPP
#define PRIVATE_NODE_WRAP_CALLBACK_HPP

#include "private/common/compat.hpp"
#include <nan.h>

namespace mk {
namespace node {

Var<Nan::Callback> WrapCallback(v8::Local<v8::Value> value) {
    return Var<Nan::Callback>{new Nan::Callback{value.As<v8::Function>()}};
}

} // namespace node
} // namespace mk
#endif
