#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "../../app/shared.h"
 
#define READ32LE(P)                    \
  (__extension__({                     \
    uint32_t __x;                      \
    memcpy(&__x, P, 32 / 8);           \
    __builtin_bswap32(__x);            \
  }))

#define READ64LE(P)                    \
  (__extension__({                     \
    uint64_t __x;                      \
    memcpy(&__x, P, 64 / 8);           \
    __builtin_bswap32(__x);            \
  }))

#define READ64BE(P)                    \
  (__extension__({                     \
    uint64_t __x;                      \
    memcpy(&__x, P, 64 / 8);           \
    __builtin_bswap64(__x);            \
  }))


#define HttpRequest  0
#define HttpResponse 1

#define HttpGet     READ32LE("GET")
#define HttpHead    READ32LE("HEAD")
#define HttpPost    READ32LE("POST")
#define HttpPut     READ32LE("PUT")
#define HttpDelete  READ64LE("DELETE\0")
#define HttpOptions READ64LE("OPTIONS")
#define HttpConnect READ64LE("CONNECT")
#define HttpTrace   READ64LE("TRACE\0\0")

#define HttpStateStart   0
#define HttpStateMethod  1
#define HttpStateUri     2
#define HttpStateVersion 3
#define HttpStateStatus  4
#define HttpStateMessage 5
#define HttpStateName    6
#define HttpStateColon   7
#define HttpStateValue   8
#define HttpStateCr      9
#define HttpStateLf1     10
#define HttpStateLf2     11

#define HttpClientStateHeaders      0
#define HttpClientStateBody         1
#define HttpClientStateBodyChunked  2
#define HttpClientStateBodyLengthed 3

#define HttpStateChunkStart   0
#define HttpStateChunkSize    1
#define HttpStateChunkExt     2
#define HttpStateChunkLf1     3
#define HttpStateChunk        4
#define HttpStateChunkCr2     5
#define HttpStateChunkLf2     6
#define HttpStateTrailerStart 7
#define HttpStateTrailer      8
#define HttpStateTrailerLf1   9
#define HttpStateTrailerLf2   10

#define HttpHost                          0
#define HttpCacheControl                  1
#define HttpConnection                    2
#define HttpAccept                        3
#define HttpAcceptLanguage                4
#define HttpAcceptEncoding                5
#define HttpUserAgent                     6
#define HttpReferer                       7
#define HttpXForwardedFor                 8
#define HttpOrigin                        9
#define HttpUpgradeInsecureRequests       10
#define HttpPragma                        11
#define HttpCookie                        12
#define HttpDnt                           13
#define HttpSecGpc                        14
#define HttpFrom                          15
#define HttpIfModifiedSince               16
#define HttpXRequestedWith                17
#define HttpXForwardedHost                18
#define HttpXForwardedProto               19
#define HttpXCsrfToken                    20
#define HttpSaveData                      21
#define HttpRange                         22
#define HttpContentLength                 23
#define HttpContentType                   24
#define HttpVary                          25
#define HttpDate                          26
#define HttpServer                        27
#define HttpExpires                       28
#define HttpContentEncoding               29
#define HttpLastModified                  30
#define HttpEtag                          31
#define HttpAllow                         32
#define HttpContentRange                  33
#define HttpAcceptCharset                 34
#define HttpAccessControlAllowCredentials 35
#define HttpAccessControlAllowHeaders     36
#define HttpAccessControlAllowMethods     37
#define HttpAccessControlAllowOrigin      38
#define HttpAccessControlMaxAge           39
#define HttpAccessControlMethod           40
#define HttpAccessControlRequestHeaders   41
#define HttpAccessControlRequestMethod    42
#define HttpAccessControlRequestMethods   43
#define HttpAge                           44
#define HttpAuthorization                 45
#define HttpContentBase                   46
#define HttpContentDescription            47
#define HttpContentDisposition            48
#define HttpContentLanguage               49
#define HttpContentLocation               50
#define HttpContentMd5                    51
#define HttpExpect                        52
#define HttpIfMatch                       53
#define HttpIfNoneMatch                   54
#define HttpIfRange                       55
#define HttpIfUnmodifiedSince             56
#define HttpKeepAlive                     57
#define HttpLink                          58
#define HttpLocation                      59
#define HttpMaxForwards                   60
#define HttpProxyAuthenticate             61
#define HttpProxyAuthorization            62
#define HttpProxyConnection               63
#define HttpPublic                        64
#define HttpRetryAfter                    65
#define HttpTe                            66
#define HttpTrailer                       67
#define HttpTransferEncoding              68
#define HttpUpgrade                       69
#define HttpWarning                       70
#define HttpWwwAuthenticate               71
#define HttpVia                           72
#define HttpStrictTransportSecurity       73
#define HttpXFrameOptions                 74
#define HttpXContentTypeOptions           75
#define HttpAltSvc                        76
#define HttpReferrerPolicy                77
#define HttpXXssProtection                78
#define HttpAcceptRanges                  79
#define HttpSetCookie                     80
#define HttpSecChUa                       81
#define HttpSecChUaMobile                 82
#define HttpSecFetchSite                  83
#define HttpSecFetchMode                  84
#define HttpSecFetchUser                  85
#define HttpSecFetchDest                  86
#define HttpCfRay                         87
#define HttpCfVisitor                     88
#define HttpCfConnectingIp                89
#define HttpCfIpcountry                   90
#define HttpSecChUaPlatform               91
#define HttpCdnLoop                       92
#define HttpHeadersMax                    93

struct HttpSlice {
  short start_offset;
  short end_offset;
};

struct HttpHeader {
  struct HttpSlice key;
  struct HttpSlice value;
};

struct HttpHeaders {
  unsigned n, c;
  struct HttpHeader *p;
};

struct HttpMessage {
  size_t cursor;
  size_t a;
  int status;
  unsigned char token;
  unsigned char type;
  unsigned char version;
  uint64_t method;
  struct HttpSlice tmp_key;
  struct HttpSlice uri;
  struct HttpSlice scratch;
  struct HttpSlice message;
  struct HttpSlice headers[HttpHeadersMax];
  struct HttpHeaders xheaders;
};

struct HttpUnchunker {
  int t;
  size_t i;
  size_t j;
  ssize_t m;
};

int parse_http_message(struct HttpMessage *http_msg, String raw_http_msg, size_t processed_bytes);

// const char *GetHttpReason(int);
// const char *GetHttpHeaderName(int);
// int GetHttpHeader(const char *, size_t);
void init_http_message(struct HttpMessage *, int); 
// void DestroyHttpMessage(struct HttpMessage *);
// void ResetHttpMessage(struct HttpMessage *, int);
// boolean HeaderHas(struct HttpMessage *, const char *, int, const char *,
//                size_t);
// int64_t ParseContentLength(const char *, size_t);
// char *FormatHttpDateTime(char[hasatleast 30], struct tm *);
// boolean ParseHttpRange(const char *, size_t, long, long *, long *);
// int64_t ParseHttpDateTime(const char *, size_t);
// uint64_t ParseHttpMethod(const char *, size_t);
// boolean IsValidHttpToken(const char *, size_t);
// boolean IsValidCookieValue(const char *, size_t);
// boolean IsAcceptablePath(const char *, size_t);
// boolean IsAcceptableHost(const char *, size_t);
// boolean IsAcceptablePort(const char *, size_t);
// boolean IsReasonablePath(const char *, size_t);
// int ParseForwarded(const char *, size_t, uint32_t *, uint16_t *);
// boolean IsMimeType(const char *, size_t, const char *);
// ssize_t Unchunk(struct HttpUnchunker *, char *, size_t, size_t *);
// const char *FindContentType(const char *, size_t);
// boolean IsNoCompressExt(const char *, size_t);
// char *FoldHeader(struct HttpMessage *, const char *, int, size_t *);

#endif /* HTTP_H */