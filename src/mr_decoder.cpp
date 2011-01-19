#include "mr_decoder.h"
#include "mr_strutils.h"

#ifdef MR_GNUTLS
    #include "mr_imap.h"
#endif

#define DEC(c)	(((c) - ' ') & 077)

#define WRITE(fd, s, size) \
    if (ss != NULL) \
        (*ss).write((char*)s,size);\
    else if (0==MR_WRITE(fd,s,size)) return;

#define FLUSH \
    if (q != buffer) { \
        *q = 0;\
        q = buffer;\
        ss << buffer;\
    }

#define PUTCHAR(c) \
    if (q == eob) { FLUSH }\
    *q++ = c


namespace {
    inline void b64decodeblock( unsigned char in[4], unsigned char out[3] ) {   
        out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
        out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
        out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
    }
}

#ifdef MR_GNUTLS
void mrutils::Decoder::decodeUUE(_UNUSED char * buffer, _UNUSED int size, imap::ContentIterator& it, int fd
    ,mrutils::stringstream * ss) {
	char *bp; int n, i, expected;

	for (;;) {
		/* for each input line */
        if (!it.nextLine()) {
            fprintf(stderr,"uuencoded: short file\n");
            return;
        }

        // replace \r\n with \n0
        *(it.eob-1) = '\n';
        *it.eob = 0;

		n = DEC(it.line[0]);
		if ((n <= 0) || (it.line[0] == '\n'))
			break;

		/* Calculate expected # of chars and pad if necessary */
		expected = ((n+2)/3)<<2;
		for (i = it.eob-it.line-1; i <= expected; i++) it.line[i] = ' ';

		bp = it.line + 1;
		while (n > 0) {
            int c1, c2, c3;

            c1 = DEC(*bp) << 2 | DEC(bp[1]) >> 4;
            c2 = DEC(bp[1]) << 4 | DEC(bp[2]) >> 2;
            c3 = DEC(bp[2]) << 6 | DEC(bp[3]);
            if (n >= 1) { WRITE(fd, &c1, 1); }
            if (n >= 2) { WRITE(fd, &c2, 1); }
            if (n >= 3) { WRITE(fd, &c3, 1); }

			bp += 4;
			n -= 3;
		}
	}
}
#endif

namespace {
    static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";
    static const unsigned char hexDecode[256] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
     0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    };

}

#define DECODE64_ALGO\
    unsigned char in[4], out[3], v;                                                                              \
    int i, len;                                                                                                  \
                                                                                                                 \
    while( *it ) {                                                                                               \
        for( len = 0, i = 0; i < 4 && *it; i++ ) {                                                               \
            v = 0;                                                                                               \
            while( *it && v == 0 ) {                                                                             \
                v = (unsigned char) *it; ++it;                                                                   \
                v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);                                  \
                if( v ) {                                                                                        \
                    v = (unsigned char) ((v == '$') ? 0 : v - 61);                                               \
                }                                                                                                \
            }                                                                                                    \
            if( *it ) {                                                                                          \
                len++;                                                                                           \
                if( v ) {                                                                                        \
                    in[ i ] = (unsigned char) (v - 1);                                                           \
                }                                                                                                \
            }                                                                                                    \
            else {                                                                                               \
                in[i] = 0;                                                                                       \
            }                                                                                                    \
        }                                                                                                        \
        if( len ) {                                                                                              \
            b64decodeblock( in, out );                                                                           \
            WRITE(fd, out, len-1);                                                                               \
        }                                                                                                        \
    }

#ifdef MR_GNUTLS
void mrutils::Decoder::decodeB64(_UNUSED char * buffer, _UNUSED int size, imap::ContentIterator& it, int fd
    ,mrutils::stringstream * ss) { DECODE64_ALGO }
#endif
void mrutils::Decoder::decodeB64(_UNUSED char * buffer, _UNUSED int size, char const * it, int fd
    ,mrutils::stringstream * ss) { DECODE64_ALGO }

// special treatment for RFC-2047... case - is missing
#define DECODEQP_ALGO\
    char * q = buffer;                                                                                           \
    static const char * eob = buffer + size;                                                                     \
                                                                                                                 \
    while (*it) {                                                                                                \
        if (q == eob) {                                                                                          \
            WRITE(fd, buffer, q - buffer);                                                                       \
            q = buffer;                                                                                          \
        }                                                                                                        \
                                                                                                                 \
        switch (*it) {                                                                                           \
            case '=': {                                                                                          \
                switch (*++it) {                                                                                 \
                    case '\r':                                                                                   \
                        ++it;                                                                                    \
                    case '\n':                                                                                   \
                        ++it;                                                                                    \
                        break;                                                                                   \
                    default: {                                                                                   \
                        const int c = (int)*it; ++it;                                                            \
                        const unsigned char value                                                                \
                            = hexDecode[c] * 16                                                                  \
                            + hexDecode[(int)*it];                                                               \
                        *q++ = value;                                                                            \
                        ++it;                                                                                    \
                    }                                                                                            \
                }                                                                                                \
            } break;                                                                                             \
                                                                                                                 \
            case '\r':                                                                                           \
                ++it;                                                                                            \
            case '\n':                                                                                           \
                ++it;                                                                                            \
                *q++ = '\n';                                                                                     \
                break;                                                                                           \
                                                                                                                 \
            default:                                                                                             \
                *q++ = *it; ++it;                                                                                \
        }                                                                                                        \
    }                                                                                                            \
                                                                                                                 \
    if (q != buffer) { WRITE(fd, buffer, q - buffer); }                                                          \

#ifdef MR_GNUTLS
void mrutils::Decoder::decodeQP(char * buffer, int size, imap::ContentIterator& it, int fd
    ,mrutils::stringstream * ss) { DECODEQP_ALGO }
#endif
void mrutils::Decoder::decodeQP(char * buffer, int size, const char * it, int fd
    ,mrutils::stringstream * ss) { DECODEQP_ALGO }

#ifdef MR_GNUTLS
void mrutils::Decoder::decodePlain(char * buffer, int size, imap::ContentIterator& it, int fd
    ,mrutils::stringstream * ss) {
    char * q = buffer;
    static const char * eob = buffer + size;

    while (*it) {
        if (q == eob) {
            WRITE(fd, buffer, q - buffer);
            q = buffer;
        }

        switch (*it) {
            case '\r':
                ++it;
            case '\n':
                ++it;
                *q++ = '\n';
                break;
            default:
                *q++ = *it; ++it;
        }
    }

    if (q != buffer) { WRITE(fd, buffer, q - buffer); }
}
#endif

namespace {
static int HexPairValue(const char * code) {
  int value = 0;
  const char * pch = code;
  for (;;) {
    int digit = *pch++;
    if (digit >= '0' && digit <= '9') {
      value += digit - '0';
    }
    else if (digit >= 'A' && digit <= 'F') {
      value += digit - 'A' + 10;
    }
    else if (digit >= 'a' && digit <= 'f') {
      value += digit - 'a' + 10;
    }
    else {
      return -1;
    }
    if (pch == code + 2)
      return value;
    value <<= 4;
  }
}
}

void mrutils::Decoder::decodeURL(const char * input, mrutils::stringstream& ss, char * buffer, int size) {
    char * eob = buffer + size-1;
    char * q = buffer;

    for (;*input;++input) {
        switch (*input) {
            case '+':
                PUTCHAR(' ');
                break;
            case '%':
                if (input[1] && input[2]) {
                    int value = HexPairValue(input + 1);
                    if (value >= 0) {
                        PUTCHAR(value);
                        input += 2;
                    }
                    else {
                        PUTCHAR('?');
                    }
                }
                else {
                    PUTCHAR('?');
                }
                break;
            default:
                PUTCHAR(*input);
                break;
        }
    }
    FLUSH
}

namespace {
    static const char * b64map = ">aaa?456789:;<=aaa""\x0""aaa""\x0""""\x1""""\x2""""\x3""""\x4""""\x5""""\x6""""\x7""""\x8""""\x9""""\xa""""\xb""""\xc""""\xd""""\xe""""\xf""""\x10""""\x11""""\x12""""\x13""""\x14""""\x15""""\x16""""\x17""""\x18""""\x19""aaaaaa""\x1a""""\x1b""""\x1c""""\x1d""""\x1e""""\x1f"" !\"#$%&'()*+,-./0123aaaa";
}

char * mrutils::Decoder::decodeRFC2047(char * out, const char * str) {
    for (const char *p = str;;) {
        p = strstr(p,"=?");
        if (p == NULL) { 
            const size_t len = strlen(str);
            if (out != str) memcpy(out,str,len+1);
            return (out+len);
        } else p += 2;

        // skip encoding (but require ios or utf)
        if (!mrutils::startsWithI(p,"iso")
            && !mrutils::startsWithI(p,"utf")) continue;
        else p += 3;
        for (int i = 0; i < 29 && *p && *p != '?'; ++i, ++p) 
        if (*p != '?') continue;

        // format char b or q 
        const char format = ::tolower(*++p);
        if (format != 'q' && format != 'b') continue;
        if (*++p != '?') continue; ++p;

        // now the actual content
        const char * cstr = p;
        for (int i = 0; i < 128 && *p; ++i, ++p) {
            if (*p == '?' && *(p+1) == '=') break;
        } if (*p != '?' || *(p+1) != '=') continue;

        if (format == 'q') {
            // QP decoding
            for (;cstr != p; ++cstr) {
                switch (*cstr) {
                    case '=':
                        switch (*++cstr) {
                            case '\r':
                                ++cstr;
                                continue;
                            case '\n':
                                continue;
                            default: {
                                const int c = *cstr; ++cstr;
                                const unsigned char value
                                    = hexDecode[c] * 16
                                    + hexDecode[(int)*cstr];
                                *out++ = value;
                            }
                        } break;
                    case '_':
                        *out++ = ' ';
                        break;
                    default:
                        *out++ = *cstr;
                        break;
                }
            }
        } else {
            // base 64
            for (;cstr != p; ++out, cstr += 4) {
                char c1 = b64map[cstr[1]-43], c2 = b64map[cstr[2]-43];
                *out   = (unsigned char) (b64map[cstr[0]-43] << 2 | c1 >> 4);
                *++out = (unsigned char) (c1 << 4 | c2 >> 2);
                *++out = (unsigned char) (((c2 << 6) & 0xc0) | b64map[cstr[3]-43]);
            }
        }

        p += 2; str = p;
    } return out;
}

char * mrutils::Decoder::decodeB64(char * out, const char * in) {
    for (;*in; ++out, in += 4) {
        char c1 = b64map[in[1]-43], c2 = b64map[in[2]-43];
        *out   = (unsigned char) (b64map[in[0]-43] << 2 | c1 >> 4);
        *++out = (unsigned char) (c1 << 4 | c2 >> 2);
        *++out = (unsigned char) (((c2 << 6) & 0xc0) | b64map[in[3]-43]);
    } *out = '\0';
    return out;
}

char * mrutils::Decoder::decodeQP(char * out, const char * in) {
    for (const char * q = in;;in = ++q) {
        q = strchr(q,'=');
        if (q == NULL) {
            if (out != in) {
                const size_t len = strlen(in);
                memcpy(out,in,len+1);
                return (out+len);
            } else return (out+strlen(out)+1);
        }
        memcpy(out,in,q-in);
        out += q - in;
        switch (*++q) {
            case '\r':
                ++q;
            case '\n':
                continue;
            default: {
                const int c = *q; ++q;
                const unsigned char value
                    = hexDecode[c] * 16
                    + hexDecode[(int)*q];
                *out++ = value;
            }
        }
    } return out;
}

