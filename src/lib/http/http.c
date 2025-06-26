#include <stdlib.h>
#include <stdio.h>

#include "../../app/shared.h"
#include "./http.h"
#include "./gethttpheader.inc"

#define SHRT_MAX 0x7fff

int is_digit(int c) {
  return '0' <= c && c <= '9';
}

/**
 * Set of standard comma-separate HTTP headers that may span lines.
 *
 * These headers may specified on multiple lines, e.g.
 *
 *     Allow: GET
 *     Allow: POST
 *
 * Is the same as:
 *
 *     Allow: GET, POST
 *
 * Standard headers that aren't part of this set will be overwritten in
 * the event that they're specified multiple times. For example,
 *
 *     Content-Type: application/octet-stream
 *     Content-Type: text/plain; charset=utf-8
 *
 * Is the same as:
 *
 *     Content-Type: text/plain; charset=utf-8
 *
 * This set exists to optimize header lookups and parsing. The existence
 * of standard headers that aren't in this set is an O(1) operation. The
 * repeatable headers in this list require an O(1) operation if they are
 * not present, otherwise the extended headers list needs to be crawled.
 *
 * Please note non-standard headers exist, e.g. Cookie, that may span
 * multiple lines, even though they're not comma-delimited. For those
 * headers we simply don't add them to the perfect hash table.
 *
 * @note we choose to not recognize this grammar for HttpConnection
 * @note `grep '[A-Z][a-z]*".*":"' rfc2616`
 * @note `grep ':.*#' rfc2616`
 * @see RFC7230 § 4.2
 */
const boolean http_repeatable[HttpHeadersMax] = {
    [HttpAcceptCharset] = true,
    [HttpAcceptEncoding] = true,
    [HttpAcceptLanguage] = true,
    [HttpAccept] = true,
    [HttpAllow] = true,
    [HttpCacheControl] = true,
    [HttpContentEncoding] = true,
    [HttpContentLanguage] = true,
    [HttpExpect] = true,
    [HttpIfMatch] = true,
    [HttpIfNoneMatch] = true,
    [HttpPragma] = true,
    [HttpProxyAuthenticate] = true,
    [HttpPublic] = true,
    [HttpTe] = true,
    [HttpTrailer] = true,
    [HttpTransferEncoding] = true,
    [HttpUpgrade] = true,
    [HttpVary] = true,
    [HttpVia] = true,
    [HttpWarning] = true,
    [HttpWwwAuthenticate] = true,
    [HttpXForwardedFor] = true,
    [HttpAccessControlAllowHeaders] = true,
    [HttpAccessControlAllowMethods] = true,
    [HttpAccessControlRequestHeaders] = true,
    [HttpAccessControlRequestMethods] = true,
};

//	    present            absent
//	    ────────────────   ────────────────
//	                       ∅☺☻♥♦♣♠•◘○◙♂♀♪♫☼   0x00
//	                       ►◄↕‼¶§▬↨↑↓→←∟↔▲▼   0x10
//	     ! #$%&‘  *+ -.    ␠ “     ()  ,  /   0x20
//	    0123456789                   :;<=>⁇   0x30
//	     ABCDEFGHIJKLMNO   @                  0x40
//	    PQRSTUVWXYZ   ^_              [⭝]     0x50
//	    `abcdefghijklmno                      0x60
//	    pqrstuvwxyz | ~               { } ⌂   0x70
//	                       ÇüéâäàåçêëèïîìÄÅ   0x80
//	                       ÉæÆôöòûùÿÖÜ¢£¥€ƒ   0x90
//	                       áíóúñÑªº¿⌐¬½¼¡«»   0xa0
//	                       ░▒▓│┤╡╢╖╕╣║╗╝╜╛┐   0xb0
//	                       └┴┬├─┼╞╟╚╔╩╦╠═╬╧   0xc0
//	                       ╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀   0xd0
//	                       αßΓπΣσμτΦΘΩδ∞φε∩   0xe0
//	                       ≡±≥≤⌠⌡÷≈°∙×√ⁿ²■λ   0xf0
//	@see RFC2616
//	CHAR           = <any US-ASCII character (octets 0 - 127)>
//	SP             = <US-ASCII SP, space (32)>
//	HT             = <US-ASCII HT, horizontal-tab (9)>
//	CTL            = <any US-ASCII control character
//	                 (octets 0 - 31) and DEL (127)>
//	token          = 1*<any CHAR except CTLs or separators>
//	separators     = "(" | ")" | "<" | ">" | "@"
//	               | "," | ";" | ":" | "\" | <">
//	               | "/" | "[" | "]" | "?" | "="
//	               | "{" | "}" | SP | HT
const char http_token[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x10
    0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0,  // 0x20
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  // 0x30
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x40
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,  // 0x50
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x60
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,  // 0x70
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xa0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xb0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xc0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xd0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xe0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xf0
};

const uint8_t to_upper[256] = {
    0,   1,   2,   3,   4,   5,   6,    7,   8,    9,   10,  11,   12,  13,
    14,  15,  16,  17,  18,  19,  20,   21,  22,   23,  24,  25,   26,  27,
    28,  29,  30,  31,  ' ', '!', '\"', '#', '$',  '%', '&', '\'', '(', ')',
    '*', '+', ',', '-', '.', '/', '0',  '1', '2',  '3', '4', '5',  '6', '7',
    '8', '9', ':', ';', '<', '=', '>',  '?', '@',  'A', 'B', 'C',  'D', 'E',
    'F', 'G', 'H', 'I', 'J', 'K', 'L',  'M', 'N',  'O', 'P', 'Q',  'R', 'S',
    'T', 'U', 'V', 'W', 'X', 'Y', 'Z',  '[', '\\', ']', '^', '_',  '`', 'A',
    'B', 'C', 'D', 'E', 'F', 'G', 'H',  'I', 'J',  'K', 'L', 'M',  'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V',  'W', 'X',  'Y', 'Z', '{',  '|', '}',
    '~', 127, 128, 129, 130, 131, 132,  133, 134,  135, 136, 137,  138, 139,
    140, 141, 142, 143, 144, 145, 146,  147, 148,  149, 150, 151,  152, 153,
    154, 155, 156, 157, 158, 159, 160,  161, 162,  163, 164, 165,  166, 167,
    168, 169, 170, 171, 172, 173, 174,  175, 176,  177, 178, 179,  180, 181,
    182, 183, 184, 185, 186, 187, 188,  189, 190,  191, 192, 193,  194, 195,
    196, 197, 198, 199, 200, 201, 202,  203, 204,  205, 206, 207,  208, 209,
    210, 211, 212, 213, 214, 215, 216,  217, 218,  219, 220, 221,  222, 223,
    224, 225, 226, 227, 228, 229, 230,  231, 232,  233, 234, 235,  236, 237,
    238, 239, 240, 241, 242, 243, 244,  245, 246,  247, 248, 249,  250, 251,
    252, 253, 254, 255,
};

void bzero(void *p, size_t n) {
  memset(p, 0, n);
}

void init_http_message(struct HttpMessage *r, int type) {
  ASSERT(type == HttpRequest || type == HttpResponse);
  bzero(r, sizeof(*r));
  r->type = type;
}

/**
 * Returns small number for HTTP header, or -1 if not found.
 */
int get_http_header(const char *str, size_t len) {
  const struct HttpHeaderSlot *slot;
  if ((slot = lookup_http_header(str, len))) {
    return slot->code;
  }
  
  return -1;
}

int parse_http_message(struct HttpMessage *http_msg, String raw_http_msg, size_t processed_bytes) {
    size_t i;
    int http_header, character;
    if (raw_http_msg.length > processed_bytes) {
        ASSERT(0);
        return -1;
    }
    raw_http_msg.length = raw_http_msg.length > SHRT_MAX ? SHRT_MAX : raw_http_msg.length;
    processed_bytes = processed_bytes > SHRT_MAX ? SHRT_MAX : processed_bytes;
    for (; http_msg->cursor < raw_http_msg.length; ++http_msg->cursor) {
        character = raw_http_msg.data[http_msg->cursor] & 255;
        switch (http_msg->token) {
            case HttpStateStart: {
                if (character == '\r' || character == '\n') {
                    break; // RFC7230 § 3.5
                }
                if (!http_token[character]) {
                    ASSERT(0);
                    return -1;
                }
                if (http_msg->type == HttpRequest) {
                    http_msg->token = HttpStateMethod;
                    http_msg->method = to_upper[character];
                    http_msg->a = 8;
                } else {
                    http_msg->token = HttpStateVersion;
                    http_msg->a = http_msg->cursor;
                }
                break;
            }
            case HttpStateMethod: {
                for (;;) {
                    if (character == ' ') {
                        http_msg->a = http_msg->cursor + 1;
                        http_msg->token = HttpStateUri;
                        break;
                    } else if (http_msg->a == 64 || !http_token[character]) {
                        ASSERT(0);
                        return -1;
                    }
                    character = to_upper[character];
                    http_msg->method |= (uint64_t)character << http_msg->a;
                    http_msg->a += 8;
                    if (++http_msg->cursor == raw_http_msg.length) {
                        break;
                    }
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateUri: {
                for (;;) {
                    if (character == ' ' || character == '\r' || character == '\n') {
                        if (http_msg->cursor == http_msg->a) {
                            ASSERT(0);
                            return -1;
                        }
                        http_msg->uri.start_offset = http_msg->a;
                        http_msg->uri.end_offset = http_msg->cursor;

                        if (character == ' ') {
                            http_msg->a = http_msg->cursor + 1;
                            http_msg->token = HttpStateVersion;
                        } else {
                            http_msg->version = 9;
                            http_msg->token = character == '\r' ? HttpStateCr : HttpStateLf1;
                        }
                        break;
                    } else if (character < 0x20 || (0x7F <= character && character < 0xA0)) {
                        ASSERT(0);
                        return -1;
                    }
                    if (++http_msg->cursor == raw_http_msg.length) {
                        break;
                    }
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateVersion: {
                if (character == ' ' || character == '\r' || character == '\n') {
                    if (http_msg->cursor - http_msg->a == 8 && (READ64BE(raw_http_msg.data + http_msg->a) & 0xFFFFFFFFFF00FF00) == 0x485454502F002E00 && is_digit(raw_http_msg.data[http_msg->a + 5]) && is_digit(raw_http_msg.data[http_msg->a + 7])) {
                        http_msg->version = (raw_http_msg.data[http_msg->a + 5] - '0') * 10 + (raw_http_msg.data[http_msg->a + 7] - '0');
                        if (http_msg->type == HttpRequest) {
                            http_msg->token = character == '\r' ? HttpStateCr : HttpStateLf1;
                        } else {
                            http_msg->token = HttpStateStatus;
                        }
                    } else {
                        ASSERT(0);
                        return -1;
                    }
                }
                break;
            }
            case HttpStateStatus: {
                for (;;) {
                    if (character == ' ' || character == '\r' || character == '\n') {
                        if (http_msg->status < 100) {
                            ASSERT(0);
                            return -1;
                        }
                        if (character == ' ') {
                            http_msg->a = http_msg->cursor + 1;
                            http_msg->token = HttpStateMessage;
                        } else {
                            http_msg->token = character == '\r' ? HttpStateCr : HttpStateLf1;
                        }
                        break;
                    } else if ('0' <= character && character <= '9') {
                        http_msg->status *= 10;
                        http_msg->status += character - '0';
                        if (http_msg->status > 999) {
                            ASSERT(0);
                            return -1;
                        }
                    } else {
                        ASSERT(0);
                        return -1;
                    }
                    if (++http_msg->cursor == raw_http_msg.length) {
                        break;
                    }
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateMessage: {
                for (;;) {
                    if (character == '\r' || character == '\n') {
                        http_msg->message.start_offset = http_msg->a;
                        http_msg->message.end_offset = http_msg->cursor;
                        http_msg->token = character == '\r' ? HttpStateCr : HttpStateLf1;
                        break;
                    } else if (character < 0x20 || (0x7F <= character && character < 0xA0)) {
                        ASSERT(0);
                        return -1;
                    }
                    if (++http_msg->cursor == raw_http_msg.length) {
                        break;
                    }
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateCr: {
                if (character != '\n') {
                    ASSERT(0);
                    return -1;
                }
                http_msg->token = HttpStateLf1;
                break;
            }
            case HttpStateLf1: {
                if (character == '\r') {
                    http_msg->token = HttpStateLf2;
                    break;
                } else if (character == '\n') {
                    return ++http_msg->cursor;
                } else if (!http_token[character]) {
                    // 1. Forbid empty header name (RFC2616 §2.2)
                    // 2. Forbid line folding (RFC7230 §3.2.4)
                    ASSERT(0);
                    return -1;
                }
                http_msg->tmp_key.start_offset = http_msg->cursor;
                http_msg->token = HttpStateName;
                break;
            }
            case HttpStateName: {
                for (;;) {
                    if (character == ':') {
                        http_msg->tmp_key.end_offset = http_msg->cursor;
                        http_msg->token = HttpStateColon;
                        break;
                    } else if (!http_token[character]) {
                        ASSERT(0);
                        return -1;
                    }
                    if (++http_msg->cursor == raw_http_msg.length) {
                        break;
                    }
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateColon: {
                if (character == ' ' || character == '\t') {
                    break;
                }
                http_msg->a = http_msg->cursor;
                http_msg->token = HttpStateValue;
                // fallthrough
                __attribute__((fallthrough));
            }
            case HttpStateValue: {
                for (;;) {
                    if (character == '\r' || character == '\n') {
                        i = http_msg->cursor;
                        while (i > http_msg->a && (raw_http_msg.data[i - 1] == ' ' || raw_http_msg.data[i - 1] == '\t'))
                            --i;
                        if ((http_header = get_http_header(raw_http_msg.data + http_msg->tmp_key.start_offset, http_msg->tmp_key.end_offset - http_msg->tmp_key.start_offset)) != -1 && (!http_msg->headers[http_header].start_offset || !http_repeatable[http_header])) {
                            http_msg->headers[http_header].start_offset = http_msg->a;
                            http_msg->headers[http_header].end_offset = i;
                        } else {
                            if (http_msg->xheaders.n == http_msg->xheaders.c) {
                                unsigned c2;
                                struct HttpHeader *p1, *p2;
                                p1 = http_msg->xheaders.p;
                                c2 = http_msg->xheaders.c;
                                if (c2 == 0) {
                                    c2 = 1;
                                } else {
                                    c2 = c2 * 2;
                                }
                                if ((p2 = realloc(p1, c2 * sizeof(*p1)))) {
                                    http_msg->xheaders.p = p2;
                                    http_msg->xheaders.c = c2;
                                }
                            }
                            if (http_msg->xheaders.n < http_msg->xheaders.c) {
                                http_msg->xheaders.p[http_msg->xheaders.n].key = http_msg->tmp_key;
                                http_msg->xheaders.p[http_msg->xheaders.n].value.start_offset = http_msg->a;
                                http_msg->xheaders.p[http_msg->xheaders.n].value.end_offset = i;
                                http_msg->xheaders.p = http_msg->xheaders.p;
                                ++http_msg->xheaders.n;
                            }
                        }
                        http_msg->token = character == '\r' ? HttpStateCr : HttpStateLf1;
                        break;
                    } else if ((character < 0x20 && character != '\t') || (0x7F <= character && character < 0xA0)) {
                        ASSERT(0);
                        return -1;
                    }
                    if (++http_msg->cursor == raw_http_msg.length)
                        break;
                    character = raw_http_msg.data[http_msg->cursor] & 255;
                }
                break;
            }
            case HttpStateLf2: {
                if (character == '\n') {
                    return ++http_msg->cursor;
                }
                ASSERT(0);
                return -1;
            }
            default: {
                __builtin_unreachable();
            }
        }
    }
    if (http_msg->cursor < processed_bytes) {
        return 0;
    }
    ASSERT(0);
    return -1;
}