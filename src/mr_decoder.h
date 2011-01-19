#ifndef _MR_MSG_DECODER_H
#define _MR_MSG_DECODER_H

#include "mr_exports.h"
#include "mr_strutils.h"
#include "mr_files.h"
#include "mr_encoder.h"

namespace mrutils {
    namespace imap {
        class ContentIterator;
    }

class _API_ Decoder {
    public:
       /*********************
        * Main functions
        *********************/

        // URL decoding; see http://www.koders.com/cpp/fidB2A537A71CAA62B45600A401E58354F15272EBBC.aspx?s=mdef:md5
        static void decodeURL(const char * input, mrutils::stringstream& output, char * buffer, int size);

       /**
        * Note: this currently only supportse plain, base 64, and QP
        * returns a pointer to the END of the out string
        */
        static inline char * decodeStr(char * out, const char * in
            ,mrutils::encoding::Encoding encoding = encoding::EC_BASE64) {
            switch (encoding) {
                case encoding::EC_BASE64:
                    return decodeB64(out,in);
                case encoding::EC_QUOTED_PRINTABLE:
                    return decodeQP(out,in);
                default:
                    if (out == in) return (out + strlen(out));
                    else {
                        const size_t len = strlen(in);
                        memcpy(out,in,len+1);
                        return (out+len);
                    }
            }
        }

       /**
        * RFC2047 is a format used in MIME messages
        * example: =?iso-8859-1?Q?[ot]:_Strain_Gauges=3F?=
        * Can accomodate in-place decoding
        * Returns a pointer to the END of the string
        */
        static char * decodeRFC2047(char * out, const char * in);

       /**
        * For IMAP
        */
        #ifdef MR_GNUTLS
            static inline void decodeTo(std::string& encoding
                ,imap::ContentIterator& it, int fd, char * buffer, int size) {
                switch (encoding::encodingParse(encoding.c_str())) {
                    case encoding::EC_UUENCODE:
                        decodeUUE(buffer, size, it, fd,NULL);
                        break;
                    case encoding::EC_BASE64:
                        decodeB64(buffer, size, it, fd,NULL);
                        break;
                    case encoding::EC_QUOTED_PRINTABLE:
                        decodeQP(buffer, size, it, fd,NULL);
                        break;
                    default:
                        decodePlain(buffer, size, it, fd,NULL);
                        break;
                }
            }

            static inline void decodeTo(std::string& encoding
                ,imap::ContentIterator& it, mrutils::stringstream & ss
                ,char * buffer, int size) {
                switch (encoding::encodingParse(encoding.c_str())) {
                    case encoding::EC_UUENCODE:
                        decodeUUE(buffer, size, it, -1,&ss);
                        break;
                    case encoding::EC_BASE64:
                        decodeB64(buffer, size, it, -1,&ss);
                        break;
                    case encoding::EC_QUOTED_PRINTABLE:
                        decodeQP(buffer, size, it, -1,&ss);
                        break;
                    default:
                        decodePlain(buffer, size, it, -1,&ss);
                        break;
                }
            }

        #endif

    private:
        static char * decodeB64(char * out, const char * in);
        static char * decodeQP(char * out, const char * in);
        static void decodeB64(char * buffer, int size, char const* it, int fd, mrutils::stringstream * ss);
        static void decodeQP(char * buffer, int size, const char * it, int fd, mrutils::stringstream * ss);

        #ifdef MR_GNUTLS
            static void decodeB64(char * buffer, int size, imap::ContentIterator& it, int fd, mrutils::stringstream * ss);
            static void decodeQP(char * buffer, int size, imap::ContentIterator& it, int fd, mrutils::stringstream * ss);
            static void decodePlain(char * buffer, int size, imap::ContentIterator& it, int fd, mrutils::stringstream * ss);
            static void decodeUUE(char * buffer, int size, imap::ContentIterator& it, int fd, mrutils::stringstream * ss);
        #endif
};

}


#endif
