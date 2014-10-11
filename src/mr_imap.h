#ifndef _MR_IMAP_H
#define _MR_IMAP_H

#ifndef MR_GNUTLS
#error Imap requires MR_GNUTLS
#endif
#include "mr_exports.h"
#include "mr_sockets.h"
#include "mr_bufferedreader.h"
#include "fastdelegate.h"
#include <gnutls/x509.h>

namespace mrutils {

class _API_ IMAP {
    friend class CertChecker;

    public:
        typedef fastdelegate::FastDelegate2<mrutils::stringstream*, const char *, bool> certOKFunc;

    public:
        enum responseT {
            OK     = 0
           ,BAD    = 1
           ,NO     = 2
           ,DISCON = 3
        };

    public:
        IMAP(const char * server, short port
            ,const char * username
            ,const char * password
            ,const char * email = ""
            ,const char * xoauthKey = ""
            ,const char * xoauthToken = ""
            ,const char * xoauthSecret = ""
            )
        : socket(server,port)
         ,isSSL(mrutils::sockets::isSSL(port))
         ,connected(false)
         ,username(username), password(password)
         ,email(email)
         ,xoauthKey(xoauthKey)
         ,xoauthToken(xoauthToken)
         ,xoauthSecret(xoauthSecret)
        {
            reader.use(socket);
        }

    private:
        IMAP(const IMAP&);
        IMAP& operator=(const IMAP& o);

    public:
       /********************
        * Main methods
        ********************/

        inline bool connect(const int seconds = 10, const int stopFD = -1) {
            socket.makeUnsecure();
            connected = false;

            if (!socket.initClient(seconds,stopFD)) return false;
            reader.setSocketInterruptFD(stopFD);

            if (!isSSL) {
                // read greeting
                if (!reader.nextLine()) return false;

                // start TLS
                socket.write(". STARTTLS\r\n");
                if (!reader.nextLine()) return false;
                if (0!=strncmp(reader.line,". OK",4)) {
                    fprintf(stderr,"Unable to starttls on plain IMAP server: %s"
                        ,reader.line);
                    return false;
                }
            }

            if (!socket.makeSecure()) return false;
            return (connected = true);
        }

        inline void close() { socket.close(); connected = false; }

        int login(char * buffer, int size);

        /**
          * For runnning queries where there's no response 
          * expected other than OK.
          */
        inline int execSimple(char * buffer, int size, const char * query) {
            if (!connected) return false;

            size = sprintf(buffer,". %s\r\n",query);
            if (!socket.write(buffer)) { connected = false; return DISCON; }

            while (reader.nextLine()) {
                if (mrutils::startsWithI(reader.line,". OK")) {
                    return OK;
                } else if (mrutils::startsWithI(reader.line,". BAD")) {
                    fprintf(stderr,"IMAP: can't execute %s, got %s\n",buffer, reader.line);
                    return BAD;
                } else if (mrutils::startsWithI(reader.line,". NO")) {
                    fprintf(stderr,"IMAP: can't execute %s, got %s\n",buffer, reader.line);
                    return NO;
                }
            }

            connected = false;
            return DISCON;
        }

    public:
       /********************
        * Certificate verification
        ********************/

        /**
          * Certs are added as PEM plain text files into 
          * a std::string std::vector for simplicity
          */
        bool addCert(const char * path);
        bool verifyCerts(const char * dir, char * buffer, int size, certOKFunc certOKFn = NULL);

    private:
        static void printCertInfo(gnutls_x509_crt_t & cert, mrutils::stringstream * ss);

    public:
        mrutils::Socket socket;
        mrutils::BufferedReader reader;
        bool isSSL; bool connected;

    private:
        std::string username, password;
        std::string email, xoauthKey, xoauthToken, xoauthSecret;
        std::vector<std::string> ca_list;
};

namespace imap {
    class _API_ ContentIterator {
        public:
            ContentIterator(mrutils::IMAP & imap, std::string& boundary, int& bytes)
            : line(NULL), eob(NULL), imap(imap), boundary(boundary), bytes(bytes)
             ,good_(bytes > 0), firstLine_(true)
            {
                operator++();
            }

        public:
           /******************
            * Main functions
            ******************/

            inline char operator*() {
                if (!good_) return 0;
                return *line;
            }

            // prefix
            inline ContentIterator& operator++() {
                if (!good_) return *this;

                if (line == eob) {
                    if (bytes <= 0) { good_ = false; return *this; }
                    good_ = imap.reader.nextLine();

                    if (!good_) {
                        // server is gone
                        return *this;
                    } else {
                        int len = imap.reader.strlen;
                        bytes -= len + 1;
                        line = imap.reader.line;

                        if ((!boundary.empty()&&mrutils::startsWith(imap.reader.line, boundary.c_str()))) {
                            good_ = false;
                        } else {
                            eob = line + len;
                            *eob = '\n';
                        }
                    }
                } else {
                    ++line;
                }

                return *this;
            }

            /* alternate approach is nextLine() */
            inline int strlen() 
            { return (eob - line); }

            inline bool nextLine() {
                if (firstLine_) {
                    if (!good_) return false;
                    firstLine_ = false;
                    return true;
                }
                line = eob;
                operator++();
                return good_;
            }

            char * line, *eob;

        private:
            // postfix -- not usable
            ContentIterator& operator++(int); 
            
        private:
            mrutils::IMAP & imap;
            std::string & boundary;
            int& bytes;

            bool good_, firstLine_;
    };
}

}

#endif
