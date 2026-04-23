#pragma once

#include <cstring>

#include "string.h"

// Source: https://github.com/BurntRouter/filtered-word-lists/blob/master/worldfilter_blanketbanned
static constexpr const char *BLOCKED_URL_KEYWORDS[] = {
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
    "phonesex",
    "phuck",
    "phuk",
    "pigfucker",
    "rimjaw",
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
    "webcam",
    "camsite",
    "camgirls",
    "camboys",
    "sexcams",
    "nudechat",
    "fleshlight",
    "erotic",
    "erotica",
    "hookup",
    "nudes",
    "nsfwpic",
    "sexting",
    "kinky",
    "bdsm",
    "callgirl",
    "sugardaddy",
    "sugarbaby",
    "hornywife",
    "nakedgirl",
    "xxxrated",
    "buyfollowers",
    "buylikes",
    "buyviews",
    "buyvotes",
    "buysubs",
    "clickbank",
    "one-weird-trick",
    "this-one-simple",
    "dont-click-this",
    "verify-account",
    "update-billing",
    "account-locked",
    "password-reset-required",
};

static constexpr size_t BLOCKED_URL_KEYWORDS_COUNT = sizeof(BLOCKED_URL_KEYWORDS) / sizeof(BLOCKED_URL_KEYWORDS[0]);

inline bool url_contains_ci(const char *haystack, size_t haystack_len, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len > haystack_len) return false;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char) haystack[i + j]) != tolower((unsigned char) needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

inline bool is_nsfw_url(const string &url) {
    for (size_t i = 0; i < BLOCKED_URL_KEYWORDS_COUNT; i++) {
        if (url_contains_ci(url.data(), url.size(), BLOCKED_URL_KEYWORDS[i])) return true;
    }
    return false;
}


// ---- URL normalization + pattern blacklist ----

// Tracking/session query params to drop during normalization.
// Exact matches; utm_* is handled as a prefix below.
static constexpr const char *TRACKING_PARAMS[] = {
    "fbclid", "gclid",     "dclid",      "msclkid",   "yclid", "zanpid",  "wbraid",   "gbraid",
    "mc_cid", "mc_eid",    "_ga",        "_gl",       "ref",   "ref_src", "referrer", "igshid",
    "igsh",   "sessionid", "jsessionid", "phpsessid", "sid",   "sess",    "cb",       "_t",
};
static constexpr size_t TRACKING_PARAMS_COUNT = sizeof(TRACKING_PARAMS) / sizeof(TRACKING_PARAMS[0]);

inline bool is_tracking_param(const char *key, size_t keylen) {
    if (keylen >= 4 && memcmp(key, "utm_", 4) == 0) return true;
    for (size_t i = 0; i < TRACKING_PARAMS_COUNT; i++) {
        size_t tlen = strlen(TRACKING_PARAMS[i]);
        if (keylen == tlen && memcmp(key, TRACKING_PARAMS[i], tlen) == 0) return true;
    }
    return false;
}

// Binary / non-content file extensions we never want to crawl.
// Matched case-insensitively against the path (ignoring query string).
static constexpr const char *BAD_URL_EXTENSIONS[] = {
    ".jpg",  ".jpeg", ".png",  ".gif", ".bmp",  ".svg",  ".ico",  ".webp",  ".tif", ".tiff", ".mp3", ".mp4",
    ".mov",  ".avi",  ".mkv",  ".wav", ".ogg",  ".flac", ".webm", ".m4a",   ".m4v", ".zip",  ".tar", ".gz",
    ".rar",  ".7z",   ".bz2",  ".xz",  ".exe",  ".dmg",  ".iso",  ".bin",   ".deb", ".rpm",  ".msi", ".apk",
    ".pkg",  ".css",  ".js",   ".mjs", ".map",  ".ttf",  ".woff", ".woff2", ".eot", ".otf",  ".pdf", ".doc",
    ".docx", ".xls",  ".xlsx", ".ppt", ".pptx", ".epub", ".json", ".xml",   ".rss", ".atom",
};
static constexpr size_t BAD_URL_EXTENSIONS_COUNT = sizeof(BAD_URL_EXTENSIONS) / sizeof(BAD_URL_EXTENSIONS[0]);

inline bool has_bad_extension(const char *path, size_t len) {
    for (size_t i = 0; i < BAD_URL_EXTENSIONS_COUNT; i++) {
        const char *ext = BAD_URL_EXTENSIONS[i];
        size_t elen = strlen(ext);
        if (len < elen) continue;
        bool match = true;
        for (size_t j = 0; j < elen; j++) {
            char c = path[len - elen + j];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != ext[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Top-level domain filter
static constexpr const char *ALLOWED_TLDS[] = {
    // Generic
    "com",
    "org",
    "net",
    "edu",
    "gov",
    "mil",
    "int",
    // Tech
    "io",
    "dev",
    "app",
    "ai",
    "co",
    "me",
    "cc",
    "tv",
    "fm",
    "ly",
    "gg",
    "sh",
    "to",
    "tech",
    "cloud",
    "so",
    "gl",
    "is",
    "ws",
    "ac",
    // Knowledge
    "wiki",
    "news",
    "pro",
    "museum",
    "jobs",
    "page",
    "blog",
    "science",
    "health",
    "media",
    // English-speaking countries
    "uk",
    "us",
    "au",
    "ca",
    "nz",
    "ie",
    "za",
    "sg",
    "hk",
    "in",
};
static constexpr size_t ALLOWED_TLDS_COUNT = sizeof(ALLOWED_TLDS) / sizeof(ALLOWED_TLDS[0]);

inline bool has_allowed_tld(const char *in, size_t host_start, size_t host_end) {
    size_t last_dot = host_end;
    for (size_t i = host_end; i > host_start; i--) {
        if (in[i - 1] == '.') {
            last_dot = i;
            break;
        }
    }
    if (last_dot >= host_end) return false;
    size_t tld_len = host_end - last_dot;
    for (size_t i = 0; i < ALLOWED_TLDS_COUNT; i++) {
        size_t alen = strlen(ALLOWED_TLDS[i]);
        if (tld_len != alen) continue;
        bool match = true;
        for (size_t j = 0; j < alen; j++) {
            char c = in[last_dot + j];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != ALLOWED_TLDS[i][j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// non-english country codes
static constexpr const char *NON_ENGLISH_CODES[] = {
    "ar", "bg", "bn", "cs", "da", "de", "el", "es", "fa", "fi", "fr", "he", "hi", "hu",
    "ja", "ko", "nl", "pl", "pt", "ro", "ru", "sk", "sl", "sr", "sv", "th", "vi", "zh",
};
static constexpr size_t NON_ENGLISH_CODES_COUNT = sizeof(NON_ENGLISH_CODES) / sizeof(NON_ENGLISH_CODES[0]);

inline bool is_non_english_code(const char *s) {
    char a = s[0], b = s[1];
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    for (size_t i = 0; i < NON_ENGLISH_CODES_COUNT; i++) {
        if (a == NON_ENGLISH_CODES[i][0] && b == NON_ENGLISH_CODES[i][1]) return true;
    }
    return false;
}

inline bool has_non_english_code(const char *in, size_t host_start, size_t host_end, size_t path_start,
                                 size_t path_end) {
    if (host_end - host_start > 3 && in[host_start + 2] == '.') {
        if (is_non_english_code(in + host_start)) return true;
    }
    // Path prefix: /xx/ or /xx- (language-region like /zh-cn/)
    if (path_end - path_start >= 4 && in[path_start] == '/') {
        char after = in[path_start + 3];
        if (after == '/' || after == '-') {
            if (is_non_english_code(in + path_start + 1)) return true;
        }
    }
    return false;
}

// Rejects hosts where any dot-separated label contains a run of 10+
// consecutive non-vowels (a/e/i/o/u/y are vowels; everything else —
// consonants, digits, hyphens — counts as non-vowel). Catches
// random/auto-generated spam hostnames like
// "truxddjqxnwp950mblgmvwcbe8.hjxfj.com" without touching legitimate
// short-but-vowel-light brand labels (flickr, tumblr, twitch, etc.).
inline bool label_has_long_nonvowel_run(const char *in, size_t start, size_t end) {
    size_t run = 0;
    for (size_t i = start; i < end; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        bool is_vowel = (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
        if (is_vowel) {
            run = 0;
        } else {
            run++;
            if (run >= 10) return true;
        }
    }
    return false;
}

inline bool has_gibberish_host(const char *in, size_t host_start, size_t host_end) {
    size_t label_start = host_start;
    for (size_t i = host_start; i <= host_end; i++) {
        if (i == host_end || in[i] == '.') {
            if (i > label_start && label_has_long_nonvowel_run(in, label_start, i)) {
                return true;
            }
            label_start = i + 1;
        }
    }
    return false;
}

// Query-time wrapper: extract host from a full URL and run the gibberish
// check. Used to filter already-indexed junk at query time without
// needing a re-crawl.
inline bool url_has_gibberish_host(const string &url) {
    const char *d = url.data();
    size_t sz = url.size();
    size_t i = 0;
    while (i + 2 < sz && d[i] != ':') i++;
    size_t dom_start = (i + 2 < sz && d[i] == ':' && d[i + 1] == '/' && d[i + 2] == '/') ? i + 3 : 0;
    size_t dom_end = dom_start;
    while (dom_end < sz) {
        char c = d[dom_end];
        if (c == '/' || c == '?' || c == '#' || c == ':') break;
        dom_end++;
    }
    return has_gibberish_host(d, dom_start, dom_end);
}

// Avoid dead end pages
static constexpr const char *JUNK_PATH_SEGMENTS[] = {
    "/login",
    "/signin",
    "/logout",
    "/signout",
    "/register",
    "/signup",
    "/api/",
    "/feed/",
    "/rss",
    "/atom",
    "/print/",
    "/share/",
    "/calendar/",
    "/tag/",
    "/tags/",
    "/category/",
    "/categories/",
    "/search?",
    "/search/",
    "/wp-admin",
    "/wp-content/",
    "/wp-includes/",
    "/wp-json/",
    "/admin/",
    "/cgi-bin/",
    "/cart",
    "/checkout",
    // Low-content / duplicate patterns
    "/author/",
    "/authors/",
    "/comments/",
    "/comment-page",
    "/replytocom",
    "/trackback/",
    "/trackback",
    "/archive/",
    "/archives/",
    "/page/",   // pagination
    "/amp/",    // AMP duplicates
    "/profile/",
    "/user/",
    "/users/",
    "/members/",
    "/download/",
    "/downloads/",
    "/subscribe",
    "/unsubscribe",
    "/terms",
    "/privacy",
    "/cookie",
    "/cookies",
    "/contact",
    "/contact-us",
    "/sitemap",
};
static constexpr size_t JUNK_PATH_SEGMENTS_COUNT = sizeof(JUNK_PATH_SEGMENTS) / sizeof(JUNK_PATH_SEGMENTS[0]);

inline bool has_junk_path(const char *path, size_t len) {
    for (size_t i = 0; i < JUNK_PATH_SEGMENTS_COUNT; i++) {
        const char *seg = JUNK_PATH_SEGMENTS[i];
        size_t slen = strlen(seg);
        if (slen > len) continue;
        for (size_t j = 0; j <= len - slen; j++) {
            bool match = true;
            for (size_t k = 0; k < slen; k++) {
                char c = path[j + k];
                if (c >= 'A' && c <= 'Z') c += 32;
                if (c != seg[k]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Domains we never want to index — too noisy, too shim-heavy, or mostly non-content.
// Matched case-insensitively against the host (after www. stripping). An exact
// match OR suffix match after a dot both count (so "pinterest.com" and
// "co.pinterest.com" both get dropped via "pinterest.com").
static constexpr const char *BLOCKED_DOMAINS[] = {
    // URL shorteners (always just a redirect)
    "bit.ly",
    "tinyurl.com",
    "t.co",
    "goo.gl",
    "ow.ly",
    "buff.ly",
    "j.mp",
    "rebrand.ly",
    "cutt.ly",
    "is.gd",
    "shorturl.at",
    "lnkd.in",
    // Pinterest (pin galleries, near-zero text per page, spammy outbound)
    "pinterest.com",
    "pinterest.co.uk",
    "pinterest.ca",
    "pinterest.de",
    "pinterest.fr",
    "pinterest.com.au",
    "pinterest.nz",
    // Social media shims (mostly non-content, unreachable bodies)
    "tiktok.com",
    "instagram.com",
    "facebook.com",
    "fb.com",
    "x.com",
    "t.me",
    // Paste / dump sites
    "pastebin.com",
    "paste2.org",
    "paste.ee",
    "hastebin.com",
    // Low-value content farms
    "squidoo.com",
    "hubpages.com",
    // Fan-fiction / user-generated story sites (high volume, low density)
    "wattpad.com",
    "fanfiction.net",
};
static constexpr size_t BLOCKED_DOMAINS_COUNT = sizeof(BLOCKED_DOMAINS) / sizeof(BLOCKED_DOMAINS[0]);

inline bool is_blocked_domain(const char *in, size_t host_start, size_t host_end) {
    size_t host_len = host_end - host_start;
    for (size_t i = 0; i < BLOCKED_DOMAINS_COUNT; i++) {
        const char *d = BLOCKED_DOMAINS[i];
        size_t dlen = strlen(d);
        if (dlen > host_len) continue;
        // Match from the right: host must end with d, and either d is the whole
        // host or the char before d is '.'.
        size_t hoff = host_end - dlen;
        bool match = true;
        for (size_t j = 0; j < dlen; j++) {
            char c = in[hoff + j];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != d[j]) {
                match = false;
                break;
            }
        }
        if (!match) continue;
        if (hoff == host_start) return true;    // exact host
        if (in[hoff - 1] == '.') return true;   // subdomain of d
    }
    return false;
}

// Strict URL character whitelist. Allowed: [A-Za-z0-9/_.-:]. Anything else
// (query separators '?' '&' '=', encoded chars '%', tilde '~', brackets,
// semicolons, commas, emoji, etc.) causes the URL to be dropped. This is
// intentionally aggressive: noisy URLs with encoded bytes, session tokens,
// or exotic punctuation are usually tracking, search results, or bot traps.
inline bool is_allowed_url_char(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
    switch (c) {
    case '/':
    case '-':
    case '_':
    case '.':
    case ':':
        return true;
    default:
        return false;
    }
}

inline bool host_path_has_only_allowed_chars(const char *in, size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
        if (!is_allowed_url_char(in[i])) return false;
    }
    return true;
}

// Avoid ridiculously long paths
static constexpr size_t MAX_PATH_DEPTH = 8;

inline bool exceeds_path_depth(const char *in, size_t path_start, size_t path_end) {
    size_t depth = 0;
    for (size_t i = path_start; i < path_end; i++) {
        if (in[i] == '/') depth++;
        if (depth > MAX_PATH_DEPTH) return true;
    }
    return false;
}

// Normalize a URL in place. Returns an empty string if the URL should be
// dropped entirely (non-http(s), bad extension, too long, non-English, junk path).
//
// Transformations applied:
//  - scheme lowercased; non-http(s) rejected
//  - leading "www." stripped from host
//  - host lowercased
//  - default ports (:80/:443) stripped
//  - fragment (#...) stripped
//  - non-allowed TLDs rejected
//  - non-English locale in subdomain/path rejected
//  - junk path patterns rejected (login, admin, wp-*, etc.)
//  - excessive path depth (>8) rejected
//  - tracking/session query params dropped (utm_*, fbclid, jsessionid, ...)
//  - trailing '/' stripped (except on root path)
inline string normalize_url(const string &url) {
    const char *in = url.data();
    size_t in_len = url.size();

    if (in_len == 0 || in_len > 2048) return string("", 0);

    // Scheme
    size_t pos = 0;
    bool is_https;
    if (in_len >= 8 && memcmp(in, "https://", 8) == 0) {
        is_https = true;
        pos = 8;
    } else if (in_len >= 7 && memcmp(in, "http://", 7) == 0) {
        is_https = false;
        pos = 7;
    } else
        return string("", 0);

    // Strip "www."
    if (in_len - pos >= 4 && (in[pos] == 'w' || in[pos] == 'W') && (in[pos + 1] == 'w' || in[pos + 1] == 'W')
        && (in[pos + 2] == 'w' || in[pos + 2] == 'W') && in[pos + 3] == '.') {
        pos += 4;
    }

    size_t host_start = pos;
    size_t host_end = pos;
    while (host_end < in_len && in[host_end] != '/' && in[host_end] != '?' && in[host_end] != '#') {
        host_end++;
    }
    if (host_end == host_start) return string("", 0);   // empty host

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
        if (in[i] == '#') {
            end = i;
            break;
        }
    }

    // Locate query split (within [host_end, end))
    size_t path_start = host_end;
    size_t query_start = end;
    for (size_t i = host_end; i < end; i++) {
        if (in[i] == '?') {
            query_start = i;
            break;
        }
    }

    // Reject binary/media file extensions (based on path, not query)
    if (has_bad_extension(in + path_start, query_start - path_start)) return string("", 0);

    // Reject URLs with any query string at all. Most content-quality URLs are
    // clean; queries are typically search results, pagination, or session state.
    if (query_start < end) return string("", 0);

    // Strict char whitelist on host + path: letters, digits, and /_-.:
    // Rejects encoded bytes (%XX), tildes, brackets, semicolons, commas, etc.
    if (!host_path_has_only_allowed_chars(in, host_start, end)) return string("", 0);

    // Reject non-allowed TLDs
    if (!has_allowed_tld(in, host_start, host_content_end)) return string("", 0);

    // Reject explicitly blocked domains (shorteners, social shims, pin farms, etc.)
    if (is_blocked_domain(in, host_start, host_content_end)) return string("", 0);

    // Reject non-English locale in subdomain or path prefix
    if (has_non_english_code(in, host_start, host_content_end, path_start, query_start)) return string("", 0);

    // Reject hosts with gibberish labels (10+ consecutive non-vowels)
    if (has_gibberish_host(in, host_start, host_content_end)) return string("", 0);

    // Reject junk path patterns
    if (has_junk_path(in + path_start, query_start - path_start)) return string("", 0);

    // Reject excessive path depth
    if (exceeds_path_depth(in, path_start, query_start)) return string("", 0);

    // Build normalized output into scratch buffer.
    char buf[2056];
    size_t out_len = 0;

    if (is_https) {
        memcpy(buf, "https://", 8);
        out_len = 8;
    } else {
        memcpy(buf, "http://", 7);
        out_len = 7;
    }

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
        size_t i = query_start + 1;   // skip '?'
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
