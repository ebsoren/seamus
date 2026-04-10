#pragma once

#include <cstring>
#include "string.h"

// Source: https://github.com/BurntRouter/filtered-word-lists/blob/master/worldfilter_blanketbanned
static constexpr const char* BLOCKED_URL_KEYWORDS[] = {
    "milf",
    "orgy",
    "porn",
    "p0rn",
    "pussy",
    "pussi",
    "pusse",
    "niggar",
    "nigga",
    "nigger",
    "nigg3r",
    "nigg4h",
    "n1gga",
    "n1gger",
    "assfuck",
    "b00b",
    "ballsack",
    "beastial",
    "bestial",
    "blowjob",
    "buttplug",
    "c0ck",
    "cl1t",
    "clit",
    "cockface",
    "cockhead",
    "cockmunch",
    "cocksuck",
    "cocksuka",
    "cocksukka",
    "cokmuncher",
    "coksucka",
    "cummer",
    "cumshot",
    "cuniling",
    "cuntlick",
    "cunts",
    "cyalis",
    "cyberfuc",
    "d1ck",
    "dickhead",
    "dildo",
    "dog-fucker",
    "donkeyribber",
    "doosh",
    "ejakulate",
    "f4nny",
    "fagg",
    "fatass",
    "felch",
    "fellat",
    "fingerfuck",
    "fistfuck",
    "fuckhead",
    "fuckme",
    "gangbang",
    "gaysex",
    "goatse",
    "hardcoresex",
    "hotsex",
    "jism",
    "jizm",
    "jizz",
    "kawk",
    "kondum",
    "kunilingus",
    "l3itch",
    "m0f0",
    "m0fo",
    "m45terbate",
    "ma5terb8",
    "ma5terbate",
    "master-bate",
    "masterb8",
    "masterbat",
    "masturbat",
    "orgasim",
    "orgasm",
    "phonesex",
    "phuck",
    "phuk",
    "pigfucker",
    "rimjaw",
    "schlong",
    "shitdick",
    "shitfuck",
    "shithead",
    "smegma",
    "t1tt",
    "titfuck",
    "tittie",
    "tittyfuck",
    "tittywank",
    "titwank",
    "tw4t",
    "twathead",
    "twatty",
    "twunt",
    "v14gra",
    "v1gra",
    "viagra",
    "w00se",
    "wanker",
    "wanky",
    "whore",
    "xrated",
    "xvideos",
    "xnxx",
    "xhamster",
    "redtube",
    "brazzers",
    "hentai",
    "rule34",
    "onlyfans",
    "chaturbate",
    "livejasmin",
    "stripchat",
    "bongacams",
    "cam4",
    "fapello",
    "spankbang",
    "eporner",
    "tube8",
    "beeg",
    "xossip",
    "motherless",
    "xtube",
    "tnaflix",
    "nuvid",
    "fuq",
    "nsfw",
    "handjob",
    "threesome",
    "sexcam",
    "camgirl",
    "adultsearch",
    "adultfriendfinder",
};

static constexpr size_t BLOCKED_URL_KEYWORDS_COUNT = sizeof(BLOCKED_URL_KEYWORDS) / sizeof(BLOCKED_URL_KEYWORDS[0]);

inline bool url_contains_ci(const char* haystack, size_t haystack_len, const char* needle) {
    size_t needle_len = strlen(needle);
    if (needle_len > haystack_len) return false;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

inline bool is_nsfw_url(const string& url) {
    for (size_t i = 0; i < BLOCKED_URL_KEYWORDS_COUNT; i++) {
        if (url_contains_ci(url.data(), url.size(), BLOCKED_URL_KEYWORDS[i]))
            return true;
    }
    return false;
}


// ---- URL normalization + pattern blacklist ----

// Tracking/session query params to drop during normalization.
// Exact matches; utm_* is handled as a prefix below.
static constexpr const char* TRACKING_PARAMS[] = {
    "fbclid", "gclid", "dclid", "msclkid", "yclid", "zanpid", "wbraid", "gbraid",
    "mc_cid", "mc_eid", "_ga", "_gl",
    "ref", "ref_src", "referrer",
    "igshid", "igsh",
    "sessionid", "jsessionid", "phpsessid", "sid", "sess",
    "cb", "_t",
};
static constexpr size_t TRACKING_PARAMS_COUNT = sizeof(TRACKING_PARAMS) / sizeof(TRACKING_PARAMS[0]);

inline bool is_tracking_param(const char* key, size_t keylen) {
    if (keylen >= 4 && memcmp(key, "utm_", 4) == 0) return true;
    for (size_t i = 0; i < TRACKING_PARAMS_COUNT; i++) {
        size_t tlen = strlen(TRACKING_PARAMS[i]);
        if (keylen == tlen && memcmp(key, TRACKING_PARAMS[i], tlen) == 0) return true;
    }
    return false;
}

// Binary / non-content file extensions we never want to crawl.
// Matched case-insensitively against the path (ignoring query string).
static constexpr const char* BAD_URL_EXTENSIONS[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".svg", ".ico", ".webp", ".tif", ".tiff",
    ".mp3", ".mp4", ".mov", ".avi", ".mkv", ".wav", ".ogg", ".flac", ".webm", ".m4a", ".m4v",
    ".zip", ".tar", ".gz", ".rar", ".7z", ".bz2", ".xz",
    ".exe", ".dmg", ".iso", ".bin", ".deb", ".rpm", ".msi", ".apk", ".pkg",
    ".css", ".js", ".mjs", ".map", ".ttf", ".woff", ".woff2", ".eot", ".otf",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".epub",
    ".json", ".xml", ".rss", ".atom",
};
static constexpr size_t BAD_URL_EXTENSIONS_COUNT = sizeof(BAD_URL_EXTENSIONS) / sizeof(BAD_URL_EXTENSIONS[0]);

inline bool has_bad_extension(const char* path, size_t len) {
    for (size_t i = 0; i < BAD_URL_EXTENSIONS_COUNT; i++) {
        const char* ext = BAD_URL_EXTENSIONS[i];
        size_t elen = strlen(ext);
        if (len < elen) continue;
        bool match = true;
        for (size_t j = 0; j < elen; j++) {
            char c = path[len - elen + j];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != ext[j]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// Normalize a URL in place. Returns an empty string if the URL should be
// dropped entirely (non-http(s), bad extension, too long, NSFW).
//
// Transformations applied:
//  - scheme lowercased; non-http(s) rejected
//  - leading "www." stripped from host
//  - host lowercased
//  - default ports (:80/:443) stripped
//  - fragment (#...) stripped
//  - tracking/session query params dropped (utm_*, fbclid, jsessionid, ...)
//  - trailing '/' stripped (except on root path)
inline string normalize_url(const string& url) {
    const char* in = url.data();
    size_t in_len = url.size();

    if (in_len == 0 || in_len > 2048) return string("", 0);

    // Scheme
    size_t pos = 0;
    bool is_https;
    if (in_len >= 8 && memcmp(in, "https://", 8) == 0) { is_https = true;  pos = 8; }
    else if (in_len >= 7 && memcmp(in, "http://",  7) == 0) { is_https = false; pos = 7; }
    else return string("", 0);

    // Strip "www."
    if (in_len - pos >= 4 && (in[pos] == 'w' || in[pos] == 'W') &&
                             (in[pos+1] == 'w' || in[pos+1] == 'W') &&
                             (in[pos+2] == 'w' || in[pos+2] == 'W') &&
                              in[pos+3] == '.') {
        pos += 4;
    }

    size_t host_start = pos;
    size_t host_end = pos;
    while (host_end < in_len && in[host_end] != '/' && in[host_end] != '?' && in[host_end] != '#') {
        host_end++;
    }
    if (host_end == host_start) return string("", 0); // empty host

    // Find colon in host for port stripping
    size_t host_content_end = host_end;
    for (size_t i = host_start; i < host_end; i++) {
        if (in[i] == ':') {
            size_t port_start = i + 1;
            size_t port_len = host_end - port_start;
            if (is_https && port_len == 3 && memcmp(in + port_start, "443", 3) == 0) {
                host_content_end = i;
            } else if (!is_https && port_len == 2 && memcmp(in + port_start, "80", 2) == 0) {
                host_content_end = i;
            }
            break;
        }
    }

    // Strip fragment
    size_t end = in_len;
    for (size_t i = host_end; i < end; i++) {
        if (in[i] == '#') { end = i; break; }
    }

    // Locate query split (within [host_end, end))
    size_t path_start = host_end;
    size_t query_start = end;
    for (size_t i = host_end; i < end; i++) {
        if (in[i] == '?') { query_start = i; break; }
    }

    // Reject binary/media file extensions (based on path, not query)
    if (has_bad_extension(in + path_start, query_start - path_start)) return string("", 0);

    // Build normalized output into scratch buffer.
    char buf[2048];
    size_t out_len = 0;

    if (is_https) { memcpy(buf, "https://", 8); out_len = 8; }
    else          { memcpy(buf, "http://",  7); out_len = 7; }

    for (size_t i = host_start; i < host_content_end; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        buf[out_len++] = c;
    }

    // Ensure path has a leading '/'
    size_t path_len_in_buf_start = out_len;
    if (path_start == end || in[path_start] != '/') {
        buf[out_len++] = '/';
    }

    // Copy path (case preserved)
    for (size_t i = path_start; i < query_start; i++) {
        buf[out_len++] = in[i];
    }

    // Filter query params
    if (query_start < end) {
        bool first = true;
        size_t i = query_start + 1; // skip '?'
        while (i < end) {
            size_t seg_start = i;
            while (i < end && in[i] != '&') i++;
            size_t seg_len = i - seg_start;
            if (seg_len > 0) {
                size_t eq = seg_start;
                while (eq < seg_start + seg_len && in[eq] != '=') eq++;
                size_t keylen = eq - seg_start;
                if (!is_tracking_param(in + seg_start, keylen)) {
                    buf[out_len++] = first ? '?' : '&';
                    first = false;
                    memcpy(buf + out_len, in + seg_start, seg_len);
                    out_len += seg_len;
                }
            }
            if (i < end && in[i] == '&') i++;
        }
    }

    // Strip trailing '/' if path is non-root (i.e., more than one char after host)
    if (out_len > path_len_in_buf_start + 1 && buf[out_len - 1] == '/') {
        out_len--;
    }

    return string(buf, out_len);
}
