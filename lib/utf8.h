#pragma once

#include <cstddef>
#include <cstdint>


//
// Basic Types
//


typedef uint32_t Unicode;
typedef uint8_t  Utf8;
typedef uint16_t Utf16;


const   Unicode  ReplacementCharacter = 0xfffd;

// The Utf8 byte order mark is the same 0xfeff Unicode character value
// but written out as Utf8.
const Utf8     Utf8BOMString[ ] = { 0xef, 0xbb, 0xbf };


// SizeOfUtf8 tells the number of bytes it will take to encode the
// specified value as Utf8.  Assumes values over 31 bits will be replaced.

// SizeOfUTF8( GetUtf8( p ) ) does not tell how many bytes encode
// the character pointed to by p because p may point to a malformed
// character.
size_t SizeOfUtf8(Unicode c) {
    if (c <= 0x7F) return 1;
    if (c <= 0x7FF) return 2;
    if (c <= 0xFFFF) return 3;
    if (c <= 0x1FFFFF) return 4;
    if (c <= 0x3FFFFFF) return 5;
    if (c <= 0x7FFFFFFF) return 6;
    return 2; // Values that need more than 31 bits become the replacement character
}

// IndicatedLength looks at the first byte of a Utf8 sequence and
// determines the expected length.  Return 1 for an invalid first byte.
size_t IndicatedLength(const Utf8 *p) {
    // Weird that instructions say to return 1 for an invalid first byte
    // Since 1 byte Utf8 characters can exist
    if ((*p & 0x80) == 0x00) return 1;
    if ((*p & 0xE0) == 0xC0) return 2;
    if ((*p & 0XF0) == 0xE0) return 3;
    if ((*p & 0xF8) == 0xF0) return 4; 
    if ((*p & 0xFC) == 0xF8) return 5;
    if ((*p & 0xFE) == 0xFC) return 6;
    return 1;
}

// Read the next Utf8 character beginning at **pp and bump *pp past
// the end of the character.  if bound != null, bound points one past
// the last valid byte.
//
// 1.  If *pp already points to or past the last valid byte, do not
//     advance *pp and return the null character.
// 2.  If there is at least one byte, but the character runs past the
//     last valid byte or is invalid, return the replacement character
//     and set *pp pointing to just past the last byte consumed.
//
// Overlong characters and character values 0xd800 to 0xdfff (UTF-16
// surrogates), "noncharacters" 0xfdd0 to 0xfdef, and the values 0xfffe
// and 0xffff should be returned as the replacement character.
Unicode ReadUtf8(const Utf8 **p, const Utf8 *bound) {
    if (bound && *p >= bound) return 0x00;

    Utf8 first = **p;
    int n_bytes = (first & 0x80) == 0      ? 1 :
                    (first & 0xE0) == 0xC0   ? 2 :
                    (first & 0xF0) == 0xE0   ? 3 :
                    (first & 0xF8) == 0xF0   ? 4 :
                    (first & 0xFC) == 0xF8   ? 5 :
                    (first & 0xFE) == 0xFC   ? 6 : 0;

    // Advance pointer past first byte
    (*p)++;
    if (n_bytes == 0) {
        return ReplacementCharacter; // Invalid first byte
    }

    if (n_bytes == 1) {
        return first;
    }
    
    // Extract initial bits from first byte using mask table
    static const Utf8 first_byte_mask[6] = {0xFF, 0x1F, 0x0F, 0x07, 0x03, 0x01};
    Unicode res = first & first_byte_mask[n_bytes - 1];

    // Read continuation bytes
    for (int i = 1; i < n_bytes; i++) {
        if ((bound && *p >= bound) || (**p & 0xC0) != 0x80) return ReplacementCharacter;  // invalid continuation byte
        res = (res << 6) | (**p & 0x3F);
        (*p)++;
    }

    // Overlong sequence checks
    static const Unicode utf8_min[7] = {0, 0, 0x80, 0x800, 0x10000, 0x200000, 0x4000000};
    if (res < utf8_min[n_bytes]) { return ReplacementCharacter; }

    // Invalid Unicode ranges
    if ((res >= 0xD800 && res <= 0xDFFF) ||
        (res >= 0xFDD0 && res <= 0xFDEF) ||
        res == 0xFFFE || res == 0xFFFF) {
        return ReplacementCharacter;
    }

    return res;
}

// Scan backward for the first PREVIOUS byte which could
// be the start of a UTF-8 character.  If bound != null,
// bound points to the first (leftmost) valid byte.
const Utf8* PreviousUtf8(const Utf8 *p, const Utf8 *bound) {
    if (bound && p <= bound) return p;
    p--;

    // This doesn't check whether the character is overlong -- hope that's what spec intends
    while ((!bound || p > bound) && (*p & 0xC0) == 0x80) p--;

    return p;
}

// Write a Unicode character in UTF-8, returning one past
// the the last byte that was written.
//
// If bound != null, it points one past the last valid location
// in the buffer. (If p is already at or past the bound, do
// nothing and return p.)
//
// If c > 0x7fffffff (31 bits) write the replacement character.
Utf8* WriteUtf8(Utf8 *p, Unicode c, Utf8 *bound) {
    // Replacement for invalid codepoints
    if (c > 0x7FFFFFFF) c = ReplacementCharacter;

    // Determine number of bytes
    int num_bytes = (c < 0x80) ? 1 :
                    (c < 0x800) ? 2 :
                    (c < 0x10000) ? 3 :
                    (c < 0x200000) ? 4 :
                    (c < 0x4000000) ? 5 : 6;

    if (bound && p + num_bytes > bound) return p;

    if (num_bytes == 1) {
        *p++ = (Utf8)c;
        return p;
    }

    // Precompute the first byte
    static const Utf8 first_byte_mask[7] = {0, 0, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
    *p++ = first_byte_mask[num_bytes] | (c >> (6 * (num_bytes - 1)));

    // Write continuation bytes
    for (int i = 1; i < num_bytes; i++) {
        int shift = 6 * (num_bytes - 1 - i);
        *p++ = 0x80 | ((c >> shift) & 0x3F);
    }

    return p;
}

// Read the character but don't advance the pointer
Unicode GetUtf8(const Utf8 *p, const Utf8 *bound) {
    Unicode res = ReadUtf8(&p, bound);
    return res;
}

// Advance the pointer but throw away the value
const Utf8* NextUtf8(const Utf8 *p, const Utf8 *bound) {
    ReadUtf8(&p, bound);
    return p;
}
