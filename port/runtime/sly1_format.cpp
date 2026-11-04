/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Native implementation used by Sly's port-only variadic format bridges. */
#include "openrac/guest.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

char* guest_pointer(gaddr address, size_t size) {
    if (openrac_guest_base == nullptr) return nullptr;
    const gaddr offset = openrac_guest_ram_address(address);
    if (offset > openrac_guest_ram_size || size > openrac_guest_ram_size - offset)
        return nullptr;
    return reinterpret_cast<char*>(openrac_guest_base + offset);
}

bool append_format_piece(std::string* output, const std::string& spec,
                         char conversion, char length, va_list* arguments) {
    char piece[4096]{};
    int written = -1;
    switch (conversion) {
    case 'd': case 'i':
        if (length == 'q')
            written = std::snprintf(piece, sizeof(piece), spec.c_str(),
                                    va_arg(*arguments, long long));
        else if (length == 'l' || length == 'z' || length == 't' || length == 'j')
            written = std::snprintf(piece, sizeof(piece), spec.c_str(),
                                    static_cast<long>(va_arg(*arguments, int)));
        else if (length == 'L')
            return false;
        else
            written = std::snprintf(piece, sizeof(piece), spec.c_str(), va_arg(*arguments, int));
        break;
    case 'u': case 'o': case 'x': case 'X':
        if (length == 'q')
            written = std::snprintf(piece, sizeof(piece), spec.c_str(),
                                    va_arg(*arguments, unsigned long long));
        else if (length == 'l' || length == 'z' || length == 't' || length == 'j')
            written = std::snprintf(piece, sizeof(piece), spec.c_str(),
                                    static_cast<unsigned long>(va_arg(*arguments, unsigned int)));
        else if (length == 'L')
            return false;
        else
            written = std::snprintf(piece, sizeof(piece), spec.c_str(),
                                    va_arg(*arguments, unsigned int));
        break;
    case 'c':
        written = std::snprintf(piece, sizeof(piece), spec.c_str(), va_arg(*arguments, int));
        break;
    case 's': {
        const gaddr address = va_arg(*arguments, gaddr);
        const char* value = address == 0 ? "(null)" : guest_pointer(address, 1);
        if (value == nullptr) {
            value = "(invalid)";
        } else {
            const size_t offset = openrac_guest_ram_address(address);
            const size_t room = openrac_guest_ram_size - offset;
            if (std::memchr(value, '\0', room) == nullptr) value = "(unterminated)";
        }
        written = std::snprintf(piece, sizeof(piece), spec.c_str(), value);
        break;
    }
    case 'p': {
        const gaddr address = va_arg(*arguments, gaddr);
        written = std::snprintf(piece, sizeof(piece), "0x%08x", address);
        break;
    }
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
        written = std::snprintf(piece, sizeof(piece), spec.c_str(), va_arg(*arguments, double));
        break;
    case 'n': {
        const gaddr address = va_arg(*arguments, gaddr);
        if (char* value = guest_pointer(address, sizeof(int))) {
            const int count = static_cast<int>(output->size());
            std::memcpy(value, &count, sizeof(count));
        }
        return true;
    }
    case '%':
        output->push_back('%');
        return true;
    default:
        output->append(spec);
        return true;
    }
    if (written < 0) return false;
    output->append(piece, static_cast<size_t>(std::min<int>(written, sizeof(piece) - 1)));
    return true;
}

bool format_guest_string(std::string* output, const char* format, va_list arguments) {
    va_list ap;
    va_copy(ap, arguments);
    const char* cursor = format;
    while (*cursor != '\0') {
        if (*cursor != '%') {
            output->push_back(*cursor++);
            continue;
        }
        const char* start = cursor++;
        if (*cursor == '%') {
            output->push_back('%');
            ++cursor;
            continue;
        }

        std::string spec("%");
        while (*cursor != '\0' && std::strchr("-+ #0'", *cursor) != nullptr)
            spec.push_back(*cursor++);
        if (*cursor == '*') {
            int width = va_arg(ap, int);
            if (width < 0) {
                if (spec.find('-') == std::string::npos) spec.push_back('-');
                width = -width;
            }
            char digits[32];
            std::snprintf(digits, sizeof(digits), "%d", width);
            spec += digits;
            ++cursor;
        } else {
            while (*cursor >= '0' && *cursor <= '9') spec.push_back(*cursor++);
        }
        if (*cursor == '.') {
            spec.push_back(*cursor++);
            if (*cursor == '*') {
                const int precision = va_arg(ap, int);
                if (precision >= 0) {
                    char digits[32];
                    std::snprintf(digits, sizeof(digits), "%d", precision);
                    spec += digits;
                }
                ++cursor;
            } else {
                while (*cursor >= '0' && *cursor <= '9') spec.push_back(*cursor++);
            }
        }
        char length = 0;
        if (*cursor == 'h' || *cursor == 'l' || *cursor == 'z' ||
            *cursor == 't' || *cursor == 'j' || *cursor == 'L') {
            length = *cursor;
            spec.push_back(*cursor++);
            if ((length == 'h' || length == 'l') && *cursor == length) {
                spec.push_back(*cursor++);
                length = length == 'h' ? 'h' : 'q';
            }
        }
        if (*cursor == '\0') {
            output->append(start);
            break;
        }
        const char conversion = *cursor++;
        spec.push_back(conversion);
        if (!append_format_piece(output, spec, conversion, length, &ap)) {
            va_end(ap);
            return false;
        }
    }
    va_end(ap);
    return true;
}

} // namespace

extern "C" int openrac_lib_sly1_vformat(gaddr destination, int capacity, gaddr format,
                                         va_list arguments) {
    const char* format_text = guest_pointer(format, 1);
    if (format_text == nullptr) return -1;
    const size_t format_offset = openrac_guest_ram_address(format);
    const size_t format_room = openrac_guest_ram_size - format_offset;
    if (std::memchr(format_text, '\0', format_room) == nullptr) return -1;

    char* output = guest_pointer(destination, 1);
    if (output == nullptr || (capacity < 0)) return -1;
    const size_t destination_offset = openrac_guest_ram_address(destination);
    const size_t destination_room = openrac_guest_ram_size - destination_offset;
    const size_t limit = capacity == 0
        ? destination_room
        : std::min(destination_room, static_cast<size_t>(capacity));
    if (limit == 0) return -1;

    std::string formatted;
    if (!format_guest_string(&formatted, format_text, arguments)) return -1;
    const size_t copied = std::min(formatted.size(), limit - 1);
    std::memcpy(output, formatted.data(), copied);
    output[copied] = '\0';
    if (capacity > 0 && formatted.size() >= static_cast<size_t>(capacity)) {
        output[limit - 1] = '\0';
        return -1;
    }
    if (formatted.size() >= limit) return -1;
    return static_cast<int>(formatted.size());
}
