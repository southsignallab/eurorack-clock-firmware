/**
 * @file text_format.cpp
 * @brief Bounded printf-style formatting with one documented truncation rule.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "text_format.h"

#include <cstdarg>
#include <cstdio>

namespace clockfw::text {
namespace {
#if defined(CLOCK_HOST_TEST)
std::uint32_t gTruncationCount = 0U;
#endif

/** Returns complete; host tests count every incomplete result. */
bool report(const bool complete) {
#if defined(CLOCK_HOST_TEST)
    if (!complete) {
        ++gTruncationCount;
    }
#endif
    return complete;
}
}  // namespace

// A C-style variadic is deliberate: only it lets the compiler check every format string against
// its arguments ([[gnu::format]]); a parameter pack would forward an unchecked format.
// NOLINTNEXTLINE(cert-dcl50-cpp)
bool formatInto(char* const destination, const std::size_t size, const char* const format, ...) {
    if (destination == nullptr || size == 0U) {
        return report(false);
    }
    std::va_list arguments{};
    va_start(arguments, format);
    const int written = std::vsnprintf(destination, size, format, arguments);
    va_end(arguments);
    // vsnprintf returns a negative value on an encoding error and otherwise the length of
    // the complete text; a length >= size means the output was cut and NUL-terminated.
    const bool complete = written >= 0 && static_cast<std::size_t>(written) < size;
    if (written < 0) {
        destination[0] = '\0';
    }
    return report(complete);
}

#if defined(CLOCK_HOST_TEST)
std::uint32_t truncationCountForTest() {
    return gTruncationCount;
}

void resetTruncationCountForTest() {
    gTruncationCount = 0U;
}
#endif

}  // namespace clockfw::text
