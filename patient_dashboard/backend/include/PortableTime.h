#pragma once
#include <ctime>

// std::gmtime itself isn't thread-safe (returns a pointer into a shared
// static buffer). Its two reentrant replacements aren't portable on their
// own: POSIX has gmtime_r(const time_t*, tm*) -> tm*, while MSVC has
// gmtime_s(tm*, const time_t*) -> errno_t (note the reversed argument
// order, and no direct MSVC equivalent to gmtime_r). This wraps both
// behind one signature so callers don't need an #ifdef at every call site.
inline std::tm gmtimeUtc(std::time_t t) {
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &t);
#else
    gmtime_r(&t, &result);
#endif
    return result;
}
