#ifndef _MR_MSG_ENCODER_H
#define _MR_MSG_ENCODER_H

#include "mr_exports.h"
#include "mr_strutils.h"
#include "mr_files.h"
#include "mr_sockets.h"
#include "mr_bufferedreader.h"

namespace mrutils {

namespace encoding {
    enum Encoding {
        EC_PLAIN              = 0
       ,EC_BASE64             = 1
       ,EC_QUOTED_PRINTABLE   = 2
       ,EC_UUENCODE           = 3
       ,EC_SEVEN_BIT          = 4
       ,EC_EIGHT_BIT          = 5
       ,EC_URL                = 6
    };

    static const char * encodingName[] = {
        "plain"
       ,"base64"
       ,"quoted-printable"
       ,"uuencode"
       ,"7bit"
       ,"8bit"
       ,"URL"
    };

    static _UNUSED Encoding encodingParse(const char * name) {
        return (
                0==mrutils::stricmp(name,"base64") ? EC_BASE64
               :0==mrutils::stricmp(name,"quoted-printable") ? EC_QUOTED_PRINTABLE
               :0==mrutils::stricmp(name,"uuencode") ? EC_UUENCODE
               :0==mrutils::stricmp(name,"7bit") ? EC_SEVEN_BIT
               :0==mrutils::stricmp(name,"8bit") ? EC_EIGHT_BIT
               :0==mrutils::stricmp(name,"URL") ? EC_URL
               :EC_PLAIN
               );
    }
}

class _API_ Encoder {
    public:
        static bool encodeStrTo(
            const char * str
           ,mrutils::stringstream & ss
           ,encoding::Encoding encoding
           ,char * buffer, int size
           ,unsigned lineLength = 72
           ) {

            switch (encoding) {
                case encoding::EC_BASE64:
                    if (!encodeB64(str, buffer, buffer + size, NULL, NULL, &ss, NULL, lineLength)) return false;
                    break;
                case encoding::EC_QUOTED_PRINTABLE:
                    if (!encodeQP(str, buffer, buffer+size, NULL, NULL, &ss, NULL, lineLength)) return false;
                    ss << "\r\n";
                    break;
                case encoding::EC_PLAIN:
                    if (!encodePlain(str, buffer, buffer+size, NULL, NULL, &ss, NULL, lineLength)) return false;
                    ss << "\r\n";
                    break;
                case encoding::EC_URL:
                    if (!encodeURL(str, buffer, buffer+size, NULL, NULL, &ss, NULL, lineLength)) return false;
                    //ss << "\r\n"; // <-- this is not done for URL encoding
                    break;
                default:
                    fprintf(stderr,"Can't encode string in format %s\n"
                        ,encoding::encodingName[encoding]);
                    return false;
            }

            return true;
        }

        static bool encodeStrTo(
            const char * str
           ,mrutils::Socket& socket
           ,encoding::Encoding encoding
           ,char * buffer, int size
           ,unsigned lineLength = 72
           ) {

            switch (encoding) {
                case encoding::EC_BASE64:
                    if (!encodeB64(str, buffer, buffer+size, NULL, NULL, NULL, &socket, lineLength)) return false;
                    break;
                case encoding::EC_QUOTED_PRINTABLE:
                    if (!encodeQP(str, buffer, buffer+size, NULL, NULL, NULL, &socket, lineLength)) return false;
                    if (!socket.write("\r\n")) return false;
                    break;
                case encoding::EC_PLAIN:
                    if (!socket.write(str)) return false;
                    if (!socket.write("\r\n")) return false;
                    break;
                default:
                    fprintf(stderr,"Can't encode string in format %s\n"
                        ,encoding::encodingName[encoding]);
                    return false;
            }

            return true;
        }

        static bool encodeStrTo(
            const char * str
           ,FILE * fp
           ,encoding::Encoding encoding
           ,char * buffer, int size
           ,unsigned lineLength = 72
           ) {

            switch (encoding) {
                case encoding::EC_BASE64:
                    if (!encodeB64(str, buffer, buffer+size, NULL, fp, NULL, NULL, lineLength)) return false;
                    break;
                case encoding::EC_QUOTED_PRINTABLE:
                    if (!encodeQP(str, buffer, buffer+size, NULL, fp, NULL, NULL, lineLength)) return false;
                    fputs("\r\n",fp);
                    break;
                case encoding::EC_PLAIN:
                    if (!encodePlain(str, buffer, buffer+size, NULL, fp, NULL, NULL, lineLength)) return false;
                    fputs("\r\n",fp);
                    break;
                case encoding::EC_URL:
                    if (!encodeURL(str, buffer, buffer+size, NULL, fp, NULL, NULL, lineLength)) return false;
                    //fputs("\r\n",fp); // <-- not for URL encoding
                    break;
                default:
                    fprintf(stderr,"Can't encode string in format %s\n"
                        ,encoding::encodingName[encoding]);
                    return false;
            }

            return true;
        }

        static inline bool encodeFileTo(
            const char * file
           ,FILE * fp
           ,encoding::Encoding encoding
           ,char * buffer, int size
           ,unsigned lineLength = 72
           ) {

            FILE * in = fopen(file,"r");

            if (!in) return false;

            switch (encoding) {
                case encoding::EC_BASE64:
                    if (!encodeB64  (NULL, buffer, buffer+size, in, fp, NULL, NULL, lineLength)) return false;
                    break;
                case encoding::EC_QUOTED_PRINTABLE:
                    if (!encodeQP   (NULL, buffer, buffer+size, in, fp, NULL, NULL, lineLength)) return false;
                    fputs("\r\n",fp);
                    break;
                case encoding::EC_PLAIN:
                    if (!encodePlain(NULL, buffer, buffer+size, in, fp, NULL, NULL, lineLength)) return false;
                    fputs("\r\n",fp);
                    break;
                default:
                    fclose(in);
                    fprintf(stderr,"Can't encode file in format %s\n"
                        ,encoding::encodingName[encoding]);
                    return false;
            }

            fclose(in);
            return true;
        }

        static inline bool encodeFileTo(
            const char * file
           ,mrutils::Socket& socket
           ,encoding::Encoding encoding
           ,char * buffer, int size
           ,unsigned lineLength = 72
           ) {

            FILE * in = fopen(file,"r");

            if (!in) return false;

            switch (encoding) {
                case encoding::EC_BASE64:
                    if (!encodeB64(NULL, buffer, buffer+size, in, NULL, NULL, &socket, lineLength)) return false;
                    break;
                case encoding::EC_QUOTED_PRINTABLE:
                    if (!encodeQP(NULL, buffer, buffer+size, in, NULL, NULL, &socket, lineLength)) return false;
                    if (!socket.write("\r\n")) return false;
                    break;
                case encoding::EC_PLAIN:
                    if (!encodePlain(NULL, buffer, buffer+size, in, NULL, NULL, &socket, lineLength)) return false;
                    if (!socket.write("\r\n")) return false;
                    break;
                default:
                    fclose(in);
                    fprintf(stderr,"Can't encode file in format %s\n"
                        ,encoding::encodingName[encoding]);
                    return false;
            }

            fclose(in);
            return true;
        }

        static void encodeB64To(mrutils::stringstream& ss, mrutils::BufferedReader& reader, const int linelen = 72);

    private:
        static bool encodeQP(const char * str, char * buffer, const char * eob, FILE * in, FILE * fp, mrutils::stringstream * ss = NULL, mrutils::Socket * socket = NULL, unsigned lineLen=72);
        static bool encodeB64(const char * str, char * buffer, const char * eob, FILE * in, FILE * fp, mrutils::stringstream * ss = NULL, mrutils::Socket * socket = NULL, unsigned lineLen=72);
        static bool encodePlain(const char * str, char * buffer, const char * eob, FILE * in, FILE * fp, mrutils::stringstream * ss = NULL, mrutils::Socket * socket = NULL, unsigned lineLen=72);
        static bool encodeURL(const char * str, char * buffer, const char * eob, FILE * in, FILE * fp, mrutils::stringstream * ss = NULL, mrutils::Socket * socket = NULL, unsigned lineLen=72);
};

}

#endif
