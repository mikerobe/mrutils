#ifndef _MR_TLSSERVER_H
#define _MR_TLSSERVER_H

#include "mr_exports.h"
#include "mr_tlsconnection.h"

namespace mrutils {

class _API_ TLSServer : public TLSConnection {
    public:
        enum protocol_t {
            PR_CUSTOM_SRP
        };

    public:
        TLSServer(protocol_t protocol = PR_CUSTOM_SRP)
        : init_(false), connected_(false)
         ,socket(NULL)
        { }

        ~TLSServer();

        bool init(const char * tpasswd, const char * tpasswdconf);

        bool use(mrutils::Socket* socket);

    public:
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
        void close();
        void closeSession();

    private:
        inline int recv(void* s_, char * buf, int size) {
            // Wait 10 seconds -- if no data, then no longer connected
            if (!socket->waitRead(10)) { 
                fprintf(stderr,"Timed out waiting for content.");
                connected_ = false; return -1; 
            }
            return gnutls_record_recv( (gnutls_session_t) s_, buf, size );
        }

    private:
        public: mrutils::Socket * socket; private:
        bool init_, connected_;

        gnutls_session_t session;
        gnutls_srp_server_credentials_t srp_cred;

};


}


#endif
