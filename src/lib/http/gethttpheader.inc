/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf /home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert  */
/* Computed positions: -k'5,10-11,22,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"

#include <string.h>
#include "./http.h"
#line 11 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
struct __attribute__((__packed__)) HttpHeaderSlot { char *name; char code; };

#define TOTAL_KEYWORDS 93
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 32
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 181
/* maximum key range = 179, duplicates = 0 */

#ifndef GPERF_DOWNCASE
#define GPERF_DOWNCASE 1
static unsigned char gperf_downcase[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
     30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
    165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
    180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
    225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
    255
  };
#endif

#ifndef GPERF_CASE_STRNCMP
#define GPERF_CASE_STRNCMP 1
static int
gperf_case_strncmp (register const char *s1, register const char *s2, register size_t n)
{
  for (; n > 0;)
    {
      unsigned char c1 = gperf_downcase[(unsigned char)*s1++];
      unsigned char c2 = gperf_downcase[(unsigned char)*s2++];
      if (c1 != 0 && c1 == c2)
        {
          n--;
          continue;
        }
      return (int)c1 - (int)c2;
    }
  return 0;
}
#endif

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182,  35, 182, 182, 182, 182,
      182, 182, 182,   5, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182,  15, 182,  30,  60,  10,
        5,  40,   0,  40, 182,  80,  40,  90,   0,  45,
        5,   0,  45,   0,   0,   0, 182,   0, 182,  45,
      182, 182, 182, 182, 182, 182, 182,  15, 182,  30,
       60,  10,   5,  40,   0,  40, 182,  80,  40,  90,
        0,  45,   5,   0,  45,   0,   0,   0, 182,   0,
      182,  45, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
      182, 182, 182, 182, 182, 182
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[21]];
      /*FALLTHROUGH*/
      case 21:
      case 20:
      case 19:
      case 18:
      case 17:
      case 16:
      case 15:
      case 14:
      case 13:
      case 12:
      case 11:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
      case 2:
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

const struct __attribute__((__packed__)) HttpHeaderSlot *
lookup_http_header (register const char *str, register size_t len)
{
  static const struct __attribute__((__packed__)) HttpHeaderSlot wordlist[] =
    {
      {""}, {""}, {""},
#line 26 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"DNT",                              HttpDnt},
#line 13 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Host",                             HttpHost},
#line 45 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Allow",                            HttpAllow},
      {""}, {""},
#line 72 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Location",                         HttpLocation},
      {""}, {""},
#line 16 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Accept",                           HttpAccept},
#line 79 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"TE",                               HttpTe},
#line 57 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Age",                              HttpAge},
#line 39 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Date",                             HttpDate},
      {""}, {""}, {""},
#line 85 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Via",                              HttpVia},
      {""},
#line 15 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Connection",                       HttpConnection},
      {""}, {""},
#line 66 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"If-Match",                         HttpIfMatch},
      {""},
#line 35 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Range",                            HttpRange},
#line 91 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-XSS-Protection",                 HttpXXssProtection},
      {""},
#line 49 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Allow-Headers",     HttpAccessControlAllowHeaders},
#line 54 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-RequestHeaders",    HttpAccessControlRequestHeaders},
#line 56 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Request-Methods",   HttpAccessControlRequestMethods},
#line 52 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-MaxAge",            HttpAccessControlMaxAge},
#line 82 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Upgrade",                          HttpUpgrade},
#line 68 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"If-Range",                         HttpIfRange},
#line 36 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Length",                   HttpContentLength},
#line 87 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Frame-Options",                  HttpXFrameOptions},
#line 65 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Expect",                           HttpExpect},
#line 89 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Alt-Svc",                          HttpAltSvc},
      {""},
#line 60 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Description",              HttpContentDescription},
      {""},
#line 84 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"WWW-Authenticate",                 HttpWwwAuthenticate},
      {""}, {""},
#line 44 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"ETag",                             HttpEtag},
#line 19 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"User-Agent",                       HttpUserAgent},
#line 22 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Origin",                           HttpOrigin},
#line 59 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Base",                     HttpContentBase},
#line 46 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Range",                    HttpContentRange},
#line 38 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Vary",                             HttpVary},
#line 23 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Upgrade-Insecure-Requests",        HttpUpgradeInsecureRequests},
#line 62 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Language",                 HttpContentLanguage},
#line 41 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Expires",                          HttpExpires},
#line 105 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"CDN-Loop",                         HttpCdnLoop},
#line 94 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-CH-UA",                        HttpSecChUa},
      {""},
#line 25 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Cookie",                           HttpCookie},
#line 88 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Content-Type-Options",           HttpXContentTypeOptions},
#line 92 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Accept-Ranges",                    HttpAcceptRanges},
#line 34 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Save-Data",                        HttpSaveData},
#line 93 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Set-Cookie",                       HttpSetCookie},
#line 40 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Server",                           HttpServer},
#line 48 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Allow-Credentials", HttpAccessControlAllowCredentials},
      {""},
#line 97 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-Fetch-Site",                   HttpSecFetchSite},
#line 70 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Keep-Alive",                       HttpKeepAlive},
#line 101 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"CF-RAY",                           HttpCfRay},
#line 81 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Transfer-Encoding",                HttpTransferEncoding},
      {""},
#line 61 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Disposition",              HttpContentDisposition},
#line 17 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Accept-Language",                  HttpAcceptLanguage},
#line 76 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Proxy-Connection",                 HttpProxyConnection},
#line 51 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Allow-Origin",      HttpAccessControlAllowOrigin},
#line 67 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"If-None-Match",                    HttpIfNoneMatch},
#line 69 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"If-Unmodified-Since",              HttpIfUnmodifiedSince},
      {""},
#line 77 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Public",                           HttpPublic},
#line 27 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-GPC",                          HttpSecGpc},
      {""},
#line 47 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Accept-Charset",                   HttpAcceptCharset},
      {""},
#line 53 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Method",            HttpAccessControlMethod},
#line 37 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Type",                     HttpContentType},
#line 74 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Proxy-Authenticate",               HttpProxyAuthenticate},
#line 71 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Link",                             HttpLink},
      {""},
#line 30 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Requested-With",                 HttpXRequestedWith},
#line 83 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Warning",                          HttpWarning},
      {""},
#line 55 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Request-Method",    HttpAccessControlRequestMethod},
      {""},
#line 64 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-MD5",                      HttpContentMd5},
#line 80 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Trailer",                          HttpTrailer},
      {""},
#line 28 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"From",                             HttpFrom},
      {""},
#line 42 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Encoding",                 HttpContentEncoding},
#line 20 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Referer",                          HttpReferer},
#line 58 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Authorization",                    HttpAuthorization},
#line 99 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-Fetch-User",                   HttpSecFetchUser},
      {""},
#line 63 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Content-Location",                 HttpContentLocation},
      {""}, {""}, {""}, {""},
#line 103 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"CF-Connecting-IP",                 HttpCfConnectingIp},
#line 104 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"CF-IPCountry",                     HttpCfIpcountry},
#line 14 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Cache-Control",                    HttpCacheControl},
#line 75 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Proxy-Authorization",              HttpProxyAuthorization},
      {""},
#line 24 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Pragma",                           HttpPragma},
      {""}, {""},
#line 100 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-Fetch-Dest",                   HttpSecFetchDest},
      {""}, {""}, {""},
#line 50 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Access-Control-Allow-Methods",     HttpAccessControlAllowMethods},
      {""}, {""}, {""},
#line 73 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Max-Forwards",                     HttpMaxForwards},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 31 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Forwarded-Host",                 HttpXForwardedHost},
      {""}, {""}, {""},
#line 18 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Accept-Encoding",                  HttpAcceptEncoding},
      {""}, {""}, {""}, {""},
#line 102 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"CF-Visitor",                       HttpCfVisitor},
      {""},
#line 29 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"If-Modified-Since",                HttpIfModifiedSince},
      {""}, {""}, {""}, {""},
#line 33 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-CSRF-Token",                     HttpXCsrfToken},
      {""}, {""}, {""}, {""}, {""},
#line 43 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Last-Modified",                    HttpLastModified},
#line 98 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-Fetch-Mode",                   HttpSecFetchMode},
#line 90 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Referrer-Policy",                  HttpReferrerPolicy},
#line 78 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Retry-After",                      HttpRetryAfter},
      {""}, {""}, {""},
#line 86 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Strict-Transport-Security",        HttpStrictTransportSecurity},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""},
#line 21 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Forwarded-For",                  HttpXForwardedFor},
      {""},
#line 32 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"X-Forwarded-Proto",                HttpXForwardedProto},
#line 96 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-CH-UA-Platform",               HttpSecChUaPlatform},
      {""}, {""},
#line 95 "/home/roy/repositories/littlechef/src/app/http/gethttpheader.gpert"
      {"Sec-CH-UA-Mobile",                 HttpSecChUaMobile}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if ((((unsigned char)*str ^ (unsigned char)*s) & ~32) == 0 && !gperf_case_strncmp (str, s, len) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
