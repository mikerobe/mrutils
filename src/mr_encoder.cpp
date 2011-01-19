#include "mr_encoder.h"

#define SOF -INT_MAX
#if (EOF == SOF)
    #error "eof is same as sof"
#endif

#define isEOF \
    (str == NULL?(t1 == SOF ? EOF == (t1 = fgetc(in)) : EOF == t1):!*str)

#define GETC() \
    (str == NULL?(t2=t1,t1=SOF,t2):*str++)

#define FLUSH \
    if (q != buffer) { \
        *q = 0;\
        q = buffer;\
        if (ss == NULL) {\
            if (socket == NULL) {\
                fputs(buffer,fp);\
            } else {\
                if (!socket->write(buffer)) return false;\
            }\
        } else {\
            *ss << buffer;\
        }\
    }

#define PUTCHAR(c) \
    if (q == eob) { FLUSH }\
    *q++ = c



/*************** QP *******************/
const unsigned char sm_hexDigits[] = "0123456789ABCDEF";
/*
#define QP_WRITE() \
    *q = 0; fputs(buffer, fp);\
    q = buffer
*/

#define QP_BREAK() \
    PUTCHAR('=');\
    PUTCHAR('\r');\
    PUTCHAR('\n');\
    curCol = 0

#define QP_ADD() \
    if (lineLen - curCol < 2) { QP_BREAK(); } \
    PUTCHAR(c); ++curCol
    
#define QP_ENCODE_HEX() \
    if (lineLen - curCol < 4) { QP_BREAK(); } \
    PUTCHAR('=');\
    PUTCHAR(sm_hexDigits[c >> 4]);\
    PUTCHAR(sm_hexDigits[c & 0xF]);\
	curCol += 3




bool mrutils::Encoder::encodeQP(
            const char * str
           ,char * buffer
           ,const char * eob
           ,FILE * in
           ,FILE * fp
           ,mrutils::stringstream* ss
           ,mrutils::Socket * socket
           ,unsigned lineLen
           ) {
    int t1 = SOF, t2;

    --eob;
    char * q = buffer;
    int curCol = 0;

    //char * q = buffer;
    //const char * eob = buffer + size;
    char nextChar = (isEOF?0:GETC());

    for(;nextChar > 0;) {
        // make sure enough space on buffer for any contingency
        //if (eob - q < 10) { QP_WRITE(); }

        char c = nextChar;
        nextChar = (isEOF?0:GETC());

		switch (c) {
            // no . at beginning of line
            case '.': 
            {
                if (curCol == 0) {
                    QP_ENCODE_HEX();
                } else {
                    QP_ADD();
                }
            } break;

            // no space at end of line
            case ' ': 
            {
                if ( nextChar == '\n' || nextChar == '\r' || nextChar == 0 ) {
                    QP_ENCODE_HEX();
                } else {
                    QP_ADD();
                }
            } break;

            case '\t':
            case '=':
            case '?':
            {
                QP_ENCODE_HEX();
            } break;

            case '\r': // don't write \r
                break;

            case '\n':
            {
                // print \r\n
                PUTCHAR('\r');
                PUTCHAR('\n');
                curCol = 0;

                //QP_ENCODE_HEX();
            } break;

            default:
            {
                if (c >= 33 && c <= 126) {
                    QP_ADD();
                } else {
                    QP_ENCODE_HEX();
                }
            } break;

		}
	}

    //if (q != buffer) { QP_WRITE(); }
    FLUSH
    return true;
}

/*************** B64 *******************/

namespace {
    inline void encodeblock( unsigned char in[3], unsigned char out[4], int len ) {
        static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        out[0] = cb64[ in[0] >> 2 ];
        out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
        out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
        out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
    }
}

bool mrutils::Encoder::encodeB64(
            const char * str
           ,char * buffer
           ,const char * eob
           ,FILE * in
           ,FILE * fp
           ,mrutils::stringstream * ss
           ,mrutils::Socket * socket
           ,unsigned lineLen
           ) {
    int t1 = SOF,t2;

    --eob;
    char * q = buffer;
    unsigned char inB[4], outB[4];
    int i, len, blocksout = 0;

    inB[3] = 0;

    while( !isEOF ) {
        len = 0;
        for( i = 0; i < 3; i++ ) {
            if( !isEOF ) {
                inB[i] = (unsigned char) GETC();
                len++;
            } else {
                inB[i] = 0;
            }
        }
        if( len ) {
            encodeblock( inB, outB, len );
            for( i = 0; i < 4; i++ ) {
                PUTCHAR(outB[i]);
            }
            
            blocksout++;
        }
        if( blocksout >= ((int)lineLen/4) || isEOF ) {
            if( blocksout ) {
                PUTCHAR('\r');
                PUTCHAR('\n');
            }
            blocksout = 0;
        }
    }

    FLUSH
    return true;
}

void mrutils::Encoder::encodeB64To(mrutils::stringstream& ss, mrutils::BufferedReader& reader, const int linelen) {
    static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const int blocksPerLine = linelen / 4;

    for (int block = 0;;++block) {
        if (block == blocksPerLine) { ss.write("\r\n",2); block = 0; }

        const int read = reader.read(3);
        uint8_t * in = (uint8_t*)reader.line;

        switch (read) {
            case 3:
                ss << b64[ in[0] >> 2 ]
                   << b64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ]
                   << b64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ]
                   << b64[ in[2] & 0x3f];
                continue;
            case 2:
                ss << b64[ in[0] >> 2 ] 
                   << b64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ]
                   << b64[ ((in[1] & 0x0f) << 2) ]
                   << '=';
                return;
            case 1:
                ss << b64[ in[0] >> 2 ]
                   << b64[ ((in[0] & 0x03) << 4) ]
                   << "==";
                return;
            case 0:
                return;
        }
    }

}

/*************** URL *******************/

bool mrutils::Encoder::encodeURL(
            const char * str
           ,char * buffer
           ,const char * eob
           ,FILE * in
           ,FILE * fp
           ,mrutils::stringstream* ss
           ,mrutils::Socket * socket
           ,_UNUSED unsigned lineLen
           ) {
    int t1 = SOF,t2;

    static const char *digits = "0123456789ABCDEF";

    --eob;
    char * q = buffer;

    for (char ch = (isEOF?0:GETC());ch > 0; ch = (isEOF?0:GETC())) {
        if (ch == ' ') { PUTCHAR('+'); }
        else if (isalnum(ch) || strchr("-_.!~*'()", ch)) { PUTCHAR(ch); }
        else {
            PUTCHAR('%');
            PUTCHAR(digits[(ch >> 4) & 0x0F]);
            PUTCHAR(digits[       ch & 0x0F]);
        }  
    }

    FLUSH
    return true;
}

/*************** PLAIN *******************/

bool mrutils::Encoder::encodePlain(
            const char * str
           ,char * buffer
           ,const char * eob
           ,FILE * in
           ,FILE * fp
           ,mrutils::stringstream* ss
           ,mrutils::Socket * socket
           ,_UNUSED unsigned lineLen
           ) {
    int t1 = SOF,t2;

    --eob;
    char * q = buffer;
    int curCol = 0;

    char c = (isEOF?0:GETC());

    for(;c > 0; c = (isEOF?0:GETC())) {
		switch (c) {
            // no . at beginning of line -- replace with ..
            case '.': 
            {
                if (curCol == 0) {
                    PUTCHAR('.'); PUTCHAR('.');
                } else {
                    PUTCHAR(c); 
                }
            } break;

            case '\r': // don't write \r
                break;

            case '\n':
            {
                PUTCHAR('\r');
                PUTCHAR('\n');
            } break;

            default:
            {
                PUTCHAR(c);
            } break;

		}
	}

    FLUSH
    return true;
}
