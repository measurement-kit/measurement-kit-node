// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_COMMON_COMPAT_HPP
#define PRIVATE_COMMON_COMPAT_HPP

#include <measurement_kit/common.hpp>

// MK_VERSION_MAJOR was added in MK v0.7.10, will be part of v0.8.0-beta,
// and was committed during the development of v0.9.0-dev. Releases before
// the ones indicated above will not work with these bindings.
#ifndef MK_VERSION_MAJOR
#error "Unsupported version of measurement-kit: MK_VERSION_MAJOR not defined"
#endif

// Before MK v0.7.11, nodejs tests did not terminate because there was a self
// reference that has been fixed in measurement-kit/measurement-kit#1453. It
// is interesting to note that 1453 is also the year when the one hundred years
// war actually finished (but I don't think that alone has fixed the issue).
#if MK_VERSION_MAJOR < 1 && MK_VERSION_MINOR < 7 && MK_VERSION_PATCH < 11
#error "MK >= 0.7.11 is required."
#endif

// Before MK v0.8.0-dev, SharedPointer was actually named Var.
#if MK_VERSION_MAJOR < 1 && MK_VERSION_MINOR < 8
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
