/*
 * win32_core.c -- see win32_core.h for the why and the scope.
 */

#include "win32_core.h"
#include "vsl/vsl_nt_bridge.h"      /* NTSTATUS constants */

#include <string.h>

/* ---- last error ------------------------------------------------------ */

static _Thread_local uint32_t t_last_error = W32_ERROR_SUCCESS;

void SetLastError(uint32_t err)     { t_last_error = err; }
uint32_t GetLastError(void)         { return t_last_error; }

/* ---- NTSTATUS -> Win32 ----------------------------------------------- */

uint32_t Win32FromNTStatus(int64_t status)
{
    switch ((uint32_t)status) {
    case 0x00000000u /* NT_STATUS_SUCCESS             */: return W32_ERROR_SUCCESS;
    case 0xC0000001u /* NT_STATUS_UNSUCCESSFUL       */: return W32_ERROR_GEN_FAILURE;
    case 0xC0000002u /* NT_STATUS_NOT_IMPLEMENTED    */: return W32_ERROR_INVALID_FUNCTION;
    case 0xC0000008u /* NT_STATUS_INVALID_HANDLE     */: return W32_ERROR_INVALID_HANDLE;
    case 0xC000000Du /* NT_STATUS_INVALID_PARAMETER  */: return W32_ERROR_INVALID_PARAMETER;
    case 0xC0000011u /* NT_STATUS_END_OF_FILE        */: return W32_ERROR_HANDLE_EOF;
    case 0xC0000022u /* NT_STATUS_ACCESS_DENIED      */: return W32_ERROR_ACCESS_DENIED;
    case 0xC0000023u /* NT_STATUS_BUFFER_TOO_SMALL   */: return W32_ERROR_INSUFFICIENT_BUFFER;
    case 0xC0000034u /* NT_STATUS_OBJECT_NAME_NOT_FOUND */:
    case 0xC0000035u /* NT_STATUS_OBJECT_PATH_NOT_FOUND */:
        return W32_ERROR_FILE_NOT_FOUND;
    /* A create that collides with an existing name. Windows surfaces this as
     * ERROR_FILE_EXISTS from the Create* family; noted here because callers
     * commonly test for either ERROR_FILE_EXISTS or ERROR_ALREADY_EXISTS. */
    case 0xC000003Au /* NT_STATUS_OBJECT_NAME_COLLISION */:
    case 0xC0000055u /* NT_STATUS_DUPLICATE_NAME      */: return W32_ERROR_FILE_EXISTS;
    case 0x80000005u /* NT_STATUS_BUFFER_OVERFLOW     */: return W32_ERROR_INSUFFICIENT_BUFFER;
    default:
        /* Unknown. Deliberately not ERROR_SUCCESS-adjacent and deliberately not
         * a specific code that would make a caller think it understood the
         * failure. Documented in the header. */
        return W32_ERROR_INTERNAL_ERROR;
    }
}

/* ---- UTF-8 helpers --------------------------------------------------- */

/* Length in bytes of the UTF-8 sequence starting at `c`, or 0 if `c` cannot
 * begin one. Does NOT validate continuation bytes; callers do that. */
static int utf8_seq_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0;                       /* continuation byte or invalid lead */
}

static bool utf8_cont(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

/* Encode one code point as UTF-8 into `out` (which must hold 4 bytes).
 * Returns bytes written, or -1 for a value outside the encodable range. */
static int utf8_encode(uint32_t cp, char out[4])
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return -1;
}

/* Decode one UTF-8 sequence from `s` (length `len`) into `*cp`.
 * Rejects overlong forms, surrogates, values above U+10FFFF and truncated or
 * mis-continued sequences -- all of which Windows refuses too. */
static int utf8_decode(const unsigned char *s, size_t len, uint32_t *cp)
{
    int n = utf8_seq_len(s[0]);
    if (n == 0 || (size_t)n > len) return -1;

    uint32_t v;
    if (n == 1) {
        v = s[0];
    } else {
        v = (uint32_t)(s[0] & (0xFF >> (n + 1)));
        for (int i = 1; i < n; i++) {
            if (!utf8_cont(s[i])) return -1;
            v = (v << 6) | (uint32_t)(s[i] & 0x3F);
        }
    }

    /* Overlong, surrogate and out-of-range rejection. */
    static const uint32_t minv[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    if (v < minv[n]) return -1;
    if (v >= 0xD800 && v <= 0xDFFF) return -1;
    if (v > 0x10FFFF) return -1;

    *cp = v;
    return n;
}

/* ---- conversions ----------------------------------------------------- */

int WideCharToUtf8(const uint16_t *wide, int wide_chars,
                   char *utf8, int utf8_cap)
{
    if (!wide || !utf8 || utf8_cap <= 0) {
        SetLastError(W32_ERROR_INVALID_PARAMETER);
        return -1;
    }
    if (wide_chars < 0) {
        /* NUL-terminated: measure first so we can refuse before writing. */
        int n = 0;
        while (wide[n]) n++;
        wide_chars = n;
    }

    /* Pass 1: compute the exact byte count and validate every surrogate pair.
     * Validating up front is what keeps the destination untouched on failure. */
    size_t need = 0;
    for (int i = 0; i < wide_chars; ) {
        uint32_t u = (uint32_t)wide[i++];
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (i >= wide_chars) { SetLastError(W32_ERROR_NO_UNICODE_TRANSLATION); return -1; }
            uint32_t lo = (uint32_t)wide[i];
            if (lo < 0xDC00 || lo > 0xDFFF) { SetLastError(W32_ERROR_NO_UNICODE_TRANSLATION); return -1; }
            u = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
            i++;
        } else if (u >= 0xDC00 && u <= 0xDFFF) {
            SetLastError(W32_ERROR_NO_UNICODE_TRANSLATION);
            return -1;                  /* lone low surrogate */
        }
        char tmp[4];
        int k = utf8_encode(u, tmp);
        if (k < 0) { SetLastError(W32_ERROR_NO_UNICODE_TRANSLATION); return -1; }
        need += (size_t)k;
    }

    if (need + 1 > (size_t)utf8_cap) {
        SetLastError(W32_ERROR_INSUFFICIENT_BUFFER);
        return -1;
    }

    /* Pass 2: write. Nothing after this point can fail. */
    size_t o = 0;
    for (int i = 0; i < wide_chars; ) {
        uint32_t u = (uint32_t)wide[i++];
        if (u >= 0xD800 && u <= 0xDBFF) {
            uint32_t lo = (uint32_t)wide[i++];
            u = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
        }
        o += (size_t)utf8_encode(u, utf8 + o);
    }
    utf8[o] = '\0';
    SetLastError(W32_ERROR_SUCCESS);
    return (int)o;
}

int Utf8ToWideChar(const char *utf8, int utf8_len,
                   uint16_t *wide, int wide_cap)
{
    if (!utf8 || !wide || wide_cap <= 0) {
        SetLastError(W32_ERROR_INVALID_PARAMETER);
        return -1;
    }
    if (utf8_len < 0) {
        utf8_len = (int)strlen(utf8);
    }

    /* Pass 1: validate everything and count units before writing anything. */
    int units = 0;
    size_t i = 0;
    while (i < (size_t)utf8_len) {
        uint32_t cp;
        int n = utf8_decode((const unsigned char *)utf8 + i, (size_t)utf8_len - i, &cp);
        if (n < 0) { SetLastError(W32_ERROR_NO_UNICODE_TRANSLATION); return -1; }
        i += (size_t)n;
        units += (cp >= 0x10000) ? 2 : 1;
    }

    if (units + 1 > wide_cap) {
        SetLastError(W32_ERROR_INSUFFICIENT_BUFFER);
        return -1;
    }

    /* Pass 2: write. */
    int w = 0;
    i = 0;
    while (i < (size_t)utf8_len) {
        uint32_t cp;
        int n = utf8_decode((const unsigned char *)utf8 + i, (size_t)utf8_len - i, &cp);
        i += (size_t)n;
        if (cp >= 0x10000) {
            cp -= 0x10000u;
            wide[w++] = (uint16_t)(0xD800u | (cp >> 10));
            wide[w++] = (uint16_t)(0xDC00u | (cp & 0x3FF));
        } else {
            wide[w++] = (uint16_t)cp;
        }
    }
    wide[w] = 0;
    SetLastError(W32_ERROR_SUCCESS);
    return w;
}
