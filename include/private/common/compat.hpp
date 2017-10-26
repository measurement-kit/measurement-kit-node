// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_COMMON_COMPAT_HPP
#define PRIVATE_COMMON_COMPAT_HPP

#include <measurement_kit/common.hpp>

#ifndef MK_VERSION_MAJOR
#error "Too old version of measurement-kit"
#endif

// We have seen that with MK v0.7.10 most tests do not break out of libuv
// loop, most likely because of reference loops. The only test that currently
// works correctly is web connectivity. I wonder if it's because of the test
// or because of the fact it's the only test with multiple input. I've also
// seen that MK v0.9.0-dev has much less problems instead.
#if MK_VERSION_MAJOR < 1 && MK_VERSION_MINOR < 9
#warning "This addon will not work reliably with measurement-kit < v0.9.0"

namespace mk {
template <typename T> using SharedPtr = Var<T>;
} // namespace mk
#endif

namespace mk {

// These macros are not defined as of measurement-kit v0.8.0-alpha
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
