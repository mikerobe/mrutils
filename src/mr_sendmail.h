#ifndef _MR_MSG_SENDMAIL_H
#define _MR_MSG_SENDMAIL_H

#include "mr_exports.h"
#include "mr_threads.h"
#include "mr_encoder.h"
#include "mr_bufferedreader.h"

namespace mrutils {

/**
  * This is a simple utility intended to dispatch 
  * short, plain-text messages
  */
class _API_ Sendmail {
    public:
        Sendmail(const char * server, short port = 25)
        : socket(server,port)
         ,connected(false)
         ,isSSL(mrutils::sockets::isSSL(port))
        {
            reader.use(socket);
        }

        virtual ~Sendmail() {
            close();
        }

    public:
       /*********************
        * Main methods
        *********************/

        inline bool init(const char * username, const char * password, const char * email = NULL) {
            if (email == NULL) email = username;
            this->username = username; this->password = password; this->fromEmail = email;

            // close current connection
            close();

            // now see if we can connect.
            if (!connect()) return false;
            return true;
        }

        bool send(const char * subject, const char * message, const char * to = NULL, FILE * fp = NULL);

        inline void close() { 
            if (connected) {
                socket.write("QUIT\r\n");
                socket.makeUnsecure();
                connected = false;
            }
        }

    protected:
        bool connect();

        inline bool smtp250(const char * toSend, const char * prefix="250") {
            if (!socket.write(toSend)) return false;
            bool good_ = false;
            while (reader.nextLine()) {
                if (!mrutils::startsWith(reader.line,prefix)) {
                    fprintf(stderr,"SMTP error on command %s: %s\n"
                        ,toSend
                        ,reader.line);
                    return false;
                }
                if (reader.line[3] != '-') {
                    good_ = true;
                    break;
                }
            }
            return good_;
        }

    public:
        std::string username, password, fromEmail;

    protected:
        mrutils::Socket socket;
        mrutils::BufferedReader reader;
        bool connected; // when connection is secure
        const bool isSSL;
        mrutils::stringstream ss;
};

}


#endif
