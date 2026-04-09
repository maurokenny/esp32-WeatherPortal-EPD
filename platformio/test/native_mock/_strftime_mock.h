/**
 * @file _strftime_mock.h
 * @brief Mock implementation of _strftime for native unit tests
 * @details Uses standard strftime instead of locale-dependent implementation
 */

#ifndef _STRFTIME_MOCK_H
#define _STRFTIME_MOCK_H

#include <time.h>
#include <stddef.h>

// Mock implementation using standard strftime
inline size_t _strftime(char* buf, size_t maxsize, const char* format, const struct tm* timeptr) {
    return strftime(buf, maxsize, format, timeptr);
}

#endif // _STRFTIME_MOCK_H