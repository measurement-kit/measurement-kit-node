// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_COMMON_COMPAT_HPP
#define PRIVATE_COMMON_COMPAT_HPP

#include <measurement_kit/common.hpp>

namespace mk {

// Before MK v0.8.0, SharedPtr was called Var. In this library we use Var and
// we create a specific alias for when MK is >= v0.8.0.
#if MK_VERSION_MAJOR > 0 || MK_VERSION_MINOR > 7
template <typename T> using Var = SharedPtr<T>;
#endif

} // namespace mk
#endif
