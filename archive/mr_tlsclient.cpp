#include "mr_tlsclient.h"

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

#include "mr_numerical.h"
#include "mr_threads.h"

// required since some of the gnutls functions aren't thread safe
static MUTEX connectionMutex = mrutils::mutexCreate();

// if no threads, create a mutex & use for sending & receiving
#ifdef MR_TLS_NO_THREADS 
    static MUTEX windowsMutex = mrutils::mutexCreate();

    bool mrutils::TLSClient::send(const char * msg, int len) {
        if (!connected_) return false;
        if (len == 0) len = strlen(msg);

        mrutils::mutexAcquire(windowsMutex);
            int ret = gnutls_record_send (session, msg, len);
        mrutils::mutexRelease(windowsMutex);

        return (ret == len);
    }

    bool mrutils::TLSClient::nextLine() {
        bool ret;

        mrutils::mutexAcquire(windowsMutex);
            if (!reader.nextLine(false)) ret = (connected_ = false);
            else ret = true;
        mrutils::mutexRelease(windowsMutex);

        return ret;
    }
#endif

bool mrutils::TLSClient::receivePlainLine(char * response, int size) {
    char * p = response; char * eob = response + size;

    for(;eob > p;) {
        int r = ::recv(client->s_, p, eob - p, 0);
        if (r > 0) {
            p += r;
            if (*(p-1) == '\n' || *(p-1) == 0) {
                *p = 0; return true;
            }
        } else return false;
    }
    
    return false;
}

void mrutils::TLSClient::close() {
    if (closed_) return; closed_ = true;
    if (!goodClient_) return;

    mrutils::mutexAcquire(connectionMutex);
        gnutls_bye (session, GNUTLS_SHUT_RDWR);
        gnutls_deinit (session);

        switch (protocol) {
            case PR_CUSTOM_SRP:
                gnutls_srp_free_client_credentials (srp_cred);
                break;

            case PR_IMAP:
            case PR_SMTP:
                gnutls_certificate_free_credentials (xcred);
                break;
        }

        gnutls_global_deinit ();
    mrutils::mutexRelease(connectionMutex);
}

bool mrutils::TLSClient::initSRP(const char * server, int port, const char * user, const char * pass, int seconds) {
    const char priorityEncryption[] = "NORMAL:+SRP";
    const int  priorityProtocols[]  = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };
    
    // create the client connection
    if (client != NULL) { close(); }
    if (client == NULL) 
        client = new mrutils::SocketClient(server, port);
    closed_ = false;
    if (!client->init(seconds)) {
        fprintf(stderr,"Unable to connect to server %s:%d\n", server,port);
        return false;
    }

    goodClient_ = true; int ret; const char * err;
    mrutils::mutexAcquire(connectionMutex);
    #ifndef MR_TLS_NO_THREADS
        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    #endif

    gnutls_global_init();
    gnutls_global_init_extra(); // for srp
    gnutls_srp_allocate_client_credentials (&srp_cred);
    gnutls_srp_set_client_credentials (srp_cred, user, pass);
    gnutls_init (&session, GNUTLS_CLIENT);

    /* Encryption priorities */
    ret = gnutls_priority_set_direct (session, priorityEncryption, &err);
    if (ret < 0) {
        if (ret == GNUTLS_E_INVALID_REQUEST) {
            fprintf (stderr, "Syntax error at: %s\n", err);
        }
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    /* Protocol priorities */ 
    ret = gnutls_protocol_set_priority(session, priorityProtocols);
    if (ret < 0) {
        printf("unable to set proto priority\n");
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    gnutls_credentials_set (session, GNUTLS_CRD_SRP, srp_cred);
    gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) client->s_);

    /* Perform the TLS handshake */
    ret = gnutls_handshake (session);
    if (ret < 0) {
        fprintf (stderr, "*** Handshake failed\n");
        gnutls_perror (ret);
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    if (!reader.open( 
        fastdelegate::MakeDelegate(this
            ,&TLSClient::recv), (void*) session )) {
        printf("Unable to open mrutils::BufferedReader");
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    mrutils::mutexRelease(connectionMutex);

    return (connected_ = true);
}

bool mrutils::TLSClient::init(const char * server, int port, int seconds) {
    const char priorityEncryption[] = "PERFORMANCE:!ARCFOUR-128";
    const int  priorityProtocols[]  = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };

    static const int sslPorts[] = {
        261
       ,443
       ,448
       ,465
       ,563
       ,614
       ,636
       ,989
       ,990
       ,992
       ,993
       ,994
       ,995
    };

    // create the client connection
    if (client != NULL) { close(); }
    if (client == NULL) 
        client = new mrutils::SocketClient(server, port);
    closed_ = false;
    if (!client->init(seconds)) {
        fprintf(stderr,"Unable to connect to server %s:%d\n", server,port);
        return false;
    }

    int * sslPort = mrutils::lower_bound((int*)&sslPorts, sizeof(sslPorts)/sizeof(sslPorts[0])
        ,port);
    bool notSSL = (sslPort == sslPorts + sizeof(sslPorts) || port != *sslPort);

    if ( notSSL ) {
        // then clear text send STARTTLS
        char response[1024]; response[0] = 0;

        // read hello message
        if (!receivePlainLine(response, sizeof(response))) {
            fprintf(stderr,"Unable to starttls on plain server (no hello) %s"
                ,response);
            return false;
        } response[0] = 0;

        switch (protocol) {
            case PR_IMAP:
            {
                client->write(". STARTTLS\r\n");
                if (!receivePlainLine(response,sizeof(response))
                    || 0!=strncmp(response,". OK", 4) 
                    ) {
                    fprintf(stderr,"Unable to starttls on plain IMAP server %s"
                        ,response);
                    return false;
                }
            } break;

            case PR_SMTP:
            {
                client->write("HELO domain\r\n");
                if (!receivePlainLine(response,sizeof(response))
                    || 0!=strncmp(response,"250", 3) 
                    ) {
                    fprintf(stderr,"Unable to starttls on plain SMTP server %s"
                        ,response);
                    return false;
                }
                client->write("STARTTLS\r\n");
                if (!receivePlainLine(response,sizeof(response))
                    || 0!=strncmp(response,"220", 3) 
                    ) {
                    fprintf(stderr,"Unable to starttls on plain SMTP server %s"
                        ,response);
                    return false;
                }
            } break;

            default:
                fprintf(stderr,"Unknown protocol for TLSClient init() call. (srp?)\n");
                return false;
                break;
        }
    }

    goodClient_ = true; int ret; const char * err;
    mrutils::mutexAcquire(connectionMutex);
    #ifndef MR_TLS_NO_THREADS
        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    #endif

    gnutls_global_init ();

    /* X509 stuff */
    gnutls_certificate_allocate_credentials (&xcred);

    /* sets the trusted cas file 
     * Not used -- only if using gnutls_certificate_verify_peers2 to verify certs
     */
    //gnutls_certificate_set_x509_trust_file (xcred, "cas file", GNUTLS_X509_FMT_PEM);

    /* Initialize TLS session */
    gnutls_init (&session, GNUTLS_CLIENT);

    /* Encryption priorities */
    ret = gnutls_priority_set_direct (session, priorityEncryption, &err);
    if (ret < 0) {
        if (ret == GNUTLS_E_INVALID_REQUEST) {
            fprintf (stderr, "Syntax error at: %s\n", err);
        }
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    /* Protocol priorities */ 
    ret = gnutls_protocol_set_priority(session, priorityProtocols);
    if (ret < 0) {
        printf("unable to set proto priority\n");
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    /* put the x509 credentials to the current session */
    gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, xcred);

    gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) client->s_);

    /* Perform the TLS handshake */
    ret = gnutls_handshake (session);
    if (ret < 0) {
        fprintf (stderr, "*** Handshake failed\n");
        gnutls_perror (ret);
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    if (!reader.open( 
        fastdelegate::MakeDelegate(this
            ,&TLSClient::recv), (void*) session )) {
        printf("Unable to open mrutils::BufferedReader");
        mrutils::mutexRelease(connectionMutex);
        return false;
    }

    mrutils::mutexRelease(connectionMutex);

    // receive welcome message
    if (!notSSL) if (!reader.nextLine(false)) return false;

    return (connected_ = true);
}
