#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_

#include <stdint.h>

template<typename A, typename B>
struct is_same { enum { value = 0 }; };
template<typename A>
struct is_same<A, A> { enum { value = 1 }; };

#ifdef _WIN32
    static constexpr bool const is_windows = true;
    // mingw doesn't expect windows to be as dumb as it is, so suppress these warnings/errors
    #pragma GCC diagnostic ignored "-Wformat"
#else
    static constexpr bool const is_windows = false;
#endif

static constexpr char const* const int64_format = (
    is_windows ? "%I64i" :
    is_same<int64_t, long int>::value ? "%li" :
    is_same<int64_t, long long int>::value ? "%lli" :
    nullptr);
static_assert(int64_format != nullptr, "");

#endif
