#include "mr_tlsserver.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define MR_TLS_NO_THREADS

    #define pid_t long
    #include <gcrypt.h>
    #include <errno.h>
#else
    #undef MR_TLS_NO_THREADS

    #include <gcrypt.h>
    #include <errno.h>

    #include <pthread.h>
    GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif
#include "mr_threads.h"

// required since some of the gnutls functions aren't thread safe
static MUTEX connectionMutex = mrutils::mutexCreate();

// if no threads, create a mutex & use for sending & receiving
#ifdef MR_TLS_NO_THREADS 
    static MUTEX windowsMutex = mrutils::mutexCreate();

    bool mrutils::TLSServer::send(const char * msg, int len) {
        if (!connected_) return false;
        if (len == 0) len = strlen(msg);

        mrutils::mutexAcquire(windowsMutex);
            int ret = gnutls_record_send (session, msg, len);
        mrutils::mutexRelease(windowsMutex);

        return (ret == len);
    }

    bool mrutils::TLSServer::nextLine() {
        bool ret;

        mrutils::mutexAcquire(windowsMutex);
            if (!reader.nextLine(false)) ret = (connected_ = false);
            else ret = true;
        mrutils::mutexRelease(windowsMutex);

        return ret;
    }
#endif

mrutils::TLSServer::~TLSServer() {
    if (!init_) return;

    closeSession();

    mrutils::mutexAcquire(connectionMutex);
        gnutls_srp_free_server_credentials (srp_cred);
        gnutls_global_deinit ();
    mrutils::mutexRelease(connectionMutex);

}

void mrutils::TLSServer::closeSession() {
    if (socket == NULL) return;

    connected_ = false;

    mrutils::mutexAcquire(connectionMutex);
        gnutls_bye (session, GNUTLS_SHUT_RDWR);
        gnutls_deinit (session);
        delete socket;
    mrutils::mutexRelease(connectionMutex);
}

bool mrutils::TLSServer::init(const char * tpasswd, const char * tpasswdconf) {
    mrutils::mutexAcquire(connectionMutex);
        #ifndef MR_TLS_NO_THREADS
            std::cout << "gcry_control" << std::endl;
            gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
        #endif

        std::cout << "gnutls_global_init" << std::endl;
        gnutls_global_init();
        std::cout << "gnutls_global_init_extra" << std::endl;
        gnutls_global_init_extra(); // for srp

        std::cout << "gnutls_srp_allocate_server_credentials" << std::endl;
        gnutls_srp_allocate_server_credentials (&srp_cred);
        std::cout << "gnutls_srp_set_server_credentials_file" << std::endl;
        gnutls_srp_set_server_credentials_file (srp_cred, tpasswd, tpasswdconf);
    mrutils::mutexRelease(connectionMutex);

    return (init_ = true);
}

bool mrutils::TLSServer::use(mrutils::Socket* socket) {
    const char priorityEncryption[] = "NORMAL:+SRP";
    const int  priorityProtocols[]  = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };

    int ret; const char * err;

    //closeSession(); this->socket = socket;

    reader.use(*socket);

    mrutils::mutexAcquire(connectionMutex);

        std::cout << "gnutls_init" << std::endl;
        gnutls_init (&session, GNUTLS_SERVER);

        /* Encryption priorities */
        std::cout << "gnutls_priority_set_direct" << std::endl;
        ret = gnutls_priority_set_direct (session, priorityEncryption, &err);
        if (ret < 0) {
            if (ret == GNUTLS_E_INVALID_REQUEST) {
                fprintf (stderr, "Syntax error at: %s\n", err);
            }
            mrutils::mutexRelease(connectionMutex);
            return false;
        }

        /* Protocol priorities */ 
        std::cout << "gnutls_protocol_set_priority" << std::endl;
        ret = gnutls_protocol_set_priority(session, priorityProtocols);
        if (ret < 0) {
            printf("unable to set proto priority\n");
            mrutils::mutexRelease(connectionMutex);
            return false;
        }

        std::cout << "gnutls_credentials_set" << std::endl;
        gnutls_credentials_set (session, GNUTLS_CRD_SRP, srp_cred);
        std::cout << "gnutls_certificate_server_set_request" << std::endl;
        gnutls_certificate_server_set_request (session, GNUTLS_CERT_IGNORE);

        std::cout << "gnutls_transport_set_ptr " << socket->s_ << std::endl;
        gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) socket->s_);

        init_ = true;

        /* Perform the TLS handshake */
        std::cout << "gnutls_handshake" << std::endl;
        ret = gnutls_handshake (session);
        if (ret < 0) {
            fprintf (stderr, "*** Handshake failed\n");
            gnutls_perror (ret);
            mrutils::mutexRelease(connectionMutex);
            return false;
        }

        if (!reader.open( 
            fastdelegate::MakeDelegate(this
                ,&TLSServer::recv), (void*) session )) {
            printf("Unable to open mrutils::BufferedReader");
            mrutils::mutexRelease(connectionMutex);
            return false;
        }

    mrutils::mutexRelease(connectionMutex);

    return (connected_ = true);
}


