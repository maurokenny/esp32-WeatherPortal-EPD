/// @file _strftime.h
/// @brief Custom strftime implementation with locale support
/// @copyright Copyright (C) 2023 Luke Marzen
/// @license GNU General Public License v3.0
///
/// @details
/// Provides a custom implementation of strftime that respects the selected
/// locale from config.h. Standard strftime may not properly handle all
/// locale-specific formatting on embedded systems.
///
/// Supported format specifiers match standard strftime where applicable.

#ifndef ___STRFTIME_H__
#define ___STRFTIME_H__

#include <time.h>

/// @brief Format time as string with locale support
/// @param s Output buffer
/// @param maxsize Maximum bytes to write (including null terminator)
/// @param format Format string with strftime specifiers
/// @param timeptr Time structure to format
/// @return Number of bytes written (excluding null), or 0 on error
///
/// @details Format specifiers:
/// - %a: Abbreviated weekday name
/// - %A: Full weekday name
/// - %b: Abbreviated month name
/// - %B: Full month name
/// - %c: Date and time representation
/// - %d: Day of month (01-31)
/// - %e: Day of month (1-31, space-padded)
/// - %H: Hour (00-23)
/// - %I: Hour (01-12)
/// - %m: Month (01-12)
/// - %M: Minute (00-59)
/// - %p: AM/PM designation
/// - %S: Second (00-59)
/// - %y: Year (00-99)
/// - %Y: Year (full)
/// - %Z: Timezone name
/// - %%: Literal %
size_t _strftime(char *s, size_t maxsize, const char *format,
                 const struct tm *timeptr);

#endif // ___STRFTIME_H__
