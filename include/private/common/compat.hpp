// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_COMMON_COMPAT_HPP
#define PRIVATE_COMMON_COMPAT_HPP

#include <measurement_kit/common.hpp>

// Currently, this code does not work reliably with MK < 0.8
#if MK_VERSION_MAJOR < 1 && MK_VERSION_MINOR < 8
#error "You need MK >= 0.8.0"
#endif

namespace mk {

// Bindings initially written for MK v0.7.x and currently still using
// `Var<T>` rather than `SharedPtr<T>` (renamed happened in v0.8).
template <typename T> using Var = SharedPtr<T>;

#if (!defined MK_MOCK && !defined MK_MOCK_AS)
/*
Simplifies life when you use templates for mocking APIs because
allows you to write the following:

    template <MK_MOCK(event_base_new)>
    void foobar() {
        event_base *p = event_base_new();
    }

Which is arguably faster than writing the following:

    template <decltype(event_base_new) event_base_new = ::event_base_new>
    void foobar() { ...
*/
#define MK_MOCK(name_) MK_MOCK_AS(name_, name_)

// Similar to MK_MOCK but with alias
#define MK_MOCK_AS(name_, alias_) decltype(name_) alias_ = name_

#endif // !defined MK_MOCK && !defined MK_MOCK_AS

} // namespace mk
#endif
