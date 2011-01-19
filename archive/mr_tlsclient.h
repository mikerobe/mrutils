#ifndef _MR_TLSCLIENT_H
#define _MR_TLSCLIENT_H

#include "mr_exports.h"
#include "mr_tlsconnection.h"

namespace mrutils {

/**
  * NOTE: under windows, this does not support multithreading.
  */
class _API_ TLSClient : public TLSConnection {
    friend class IMAP;

    public:
        enum protocol_t {
            PR_IMAP
           ,PR_SMTP
           ,PR_CUSTOM_SRP
        };

    public:
        TLSClient(protocol_t protocol = PR_IMAP)
        : client(NULL), goodClient_(false), connected_(false), closed_(false)
         ,protocol(protocol)
        { }

        ~TLSClient() {
            close();
            if (client != NULL) delete client;
            client = NULL;
        }

        bool init(const char * server, int port, int seconds = 10);
        bool initSRP(const char * server, int port, const char * user, const char * pass, int seconds = 10);

        void close();

    public:
       /****************
        * Main methods
        ****************/

        inline bool connected() {
            return connected_;
        }

        inline bool hasData() {
            return client->waitAccept(0);
        }

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            // no threads on windows, so will have to lock 
            bool send(const char * msg, int len = 0);
            bool nextLine();
        #else
            inline bool send(const char * msg, int len = 0) {
                if (!connected_) return false;
                if (len == 0) len = strlen(msg);
                return (gnutls_record_send (session, msg, len) == len);
            }

            inline bool nextLine() {
                if (!reader.nextLine(false)) 
                    return (connected_ = false);
                else return true;
            }
        #endif

    private:
        inline int recv(void* s_, char * buf, int size) {
            // Wait 10 seconds -- if no data, then no longer connected
            if (!client->waitAccept(10)) { 
                fprintf(stderr,"Timed out waiting for content.");
                connected_ = false; return -1; 
            }
            return gnutls_record_recv( (gnutls_session_t) s_, buf, size );
        }

        bool receivePlainLine(char * buffer, int size);

    private:
        protocol_t protocol;
        bool goodClient_, connected_, closed_;
        public: mrutils::SocketClient * client; private:

        gnutls_session_t session;
        gnutls_certificate_credentials_t xcred;
        gnutls_srp_client_credentials_t srp_cred;

};

}

#endif
