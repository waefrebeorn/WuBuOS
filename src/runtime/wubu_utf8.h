/* wubu_utf8.h -- UTF-8 as a decoded sequence, not as bytes that happen to be short.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2011). Every place that currently
 * cuts a string by bytes (labels, ellipsis, file names, agent messages) splits a
 * multibyte character when it does so; this is the one owner of "how many bytes is
 * one character" and "how wide does it draw".
 */
#ifndef WUBU_UTF8_H
#define WUBU_UTF8_H

#include <stddef.h>

#define WUBU_UTF8_REPLACEMENT 0xFFFDu

/* Decode one codepoint from `s` (at most `len` bytes).
 * Returns the number of bytes consumed (1..4), or 0 if the sequence is invalid
 * (overlong, surrogate, beyond U+10FFFF, truncated, or a stray continuation byte).
 * On invalid input *cp is set to WUBU_UTF8_REPLACEMENT. Pass NULL for cp if unneeded. */
size_t wubu_utf8_decode(const char *s, size_t len, unsigned *cp);

/* Encode `cp` into `out` (at least 4 bytes). Returns bytes written, or 0 if `cp`
 * is a surrogate or beyond U+10FFFF. */
size_t wubu_utf8_encode(unsigned cp, char out[4]);

/* 1 if the whole buffer is valid UTF-8, else 0. */
int wubu_utf8_valid(const char *s, size_t len);

/* Number of characters: invalid bytes count as one each, so this never lies low. */
size_t wubu_utf8_count(const char *s, size_t len);

/* Display width in terminal cells: 0 for combining marks and controls, 2 for East
 * Asian wide characters, otherwise 1. */
int wubu_utf8_width(unsigned cp);

/* Widest prefix of `s` that fits in `max_cols` cells without splitting a character.
 * Always makes progress: at least one character is kept when max_cols > 0. */
size_t wubu_utf8_fit(const char *s, size_t len, int max_cols);

/* Simple case mapping. Covers ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic;
 * anything else is returned unchanged. A full Unicode table belongs in a data file,
 * and pretending otherwise would be worse than saying so. */
unsigned wubu_utf8_tolower(unsigned cp);
unsigned wubu_utf8_toupper(unsigned cp);

#endif
