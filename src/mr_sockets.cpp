#include "mr_sockets.h"
#include "mr_files.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define EISCONN WSAEISCONN
    #define EINPROGRESS WSAEINPROGRESS
    #define EWOULDBLOCK WSAEWOULDBLOCK
    #define EALREADY WSAEALREADY
#else
    #include <netinet/tcp.h>
#endif

// for secure sockets
#if defined(MR_GNUTLS)
    #include "mr_threads.h"
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        #define pid_t long
    #endif
    
    #include <gcrypt.h>
    #include <errno.h>
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#else
    #define Sleep(x) usleep(x*1000L)
#endif

namespace {
    static u_long one = 1; 
    static u_long zero = 0;
    static int flagOn = 1;
}

bool mrutils::Socket::setBlocking(bool tf) {
    if (!ioctl(s_,FIONBIO,(tf?&zero:&one))) {
        setToBlock = blocking = tf;
        return true;
    } else return false;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    static inline bool initWSA() {
        WSAData info; 
        int err = WSAStartup(MAKEWORD(2,0), &info);
        if (err != 0) {
            fprintf(stderr,"WSAStartup had erro code: %d\n",err);
            fflush(stderr);
        }
    } static bool initWSA_ = initWSA();
#else
    static inline void sigpipe(int) { ::signal(SIGPIPE, &sigpipe); }
#endif

void mrutils::Socket::create() {
    /* Start WSA on windows, ignore SIGPIPE on unix */
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #else
        // still getting sigpipes...
        //::signal(SIGPIPE, SIG_IGN);
        ::signal(SIGPIPE, &sigpipe);
    #endif

    memset(&addr, 0, sizeof(addr));
    memset(&addrUnix, 0, sizeof(addrUnix));

    if (s_ == -1) { // then create socket
        switch (type) {
            #ifdef _MR_IS_UNIX
                case SOCKET_UNIX: {
                    addrUnix.sun_family = AF_UNIX;
                    // serverIP is actually a file path
                    strcpy(addrUnix.sun_path, serverIP.c_str());
                    s_ = socket(AF_UNIX, SOCK_STREAM, 0);
                } break;
            #endif

            case SOCKET_STREAM: {
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = htonl(INADDR_ANY);
                addr.sin_port   = htons(port);
                s_ = socket(AF_INET, SOCK_STREAM, 0);
                
                // don't buffer sending messages -- send immediately
                if (setsockopt(s_, IPPROTO_TCP, TCP_NODELAY, (char*)&flagOn, sizeof(flagOn))
                    < 0) {
                    perror("setting TCP_NODELAY");
                }

            } break;

            case SOCKET_MULTICAST: {
                addr.sin_family      = AF_INET;
                addr.sin_addr.s_addr = htonl(INADDR_ANY);
                addr.sin_port        = htons(port);
                s_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

                // reuse address
                if (setsockopt(s_, SOL_SOCKET, SO_REUSEADDR, (char*)&flagOn, sizeof(flagOn))
                    < 0) {
                    perror("setting SO_REUSEADDR");
                }

                // reuse port if not windows
                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)                                                                  
                #else
                    #ifdef SO_REUSEPORT
                        if (setsockopt(s_, SOL_SOCKET, SO_REUSEPORT, (char*)&flagOn, sizeof(flagOn))
                            < 0) {
                            perror("setting SO_REUSEPORT");
                        }
                    #endif
                #endif

            } break;
        }
    }

    FD_ZERO(&fdsread_); FD_ZERO(&fdswrite_);
    if (s_ != -1) { selmax_ = s_+1; }
}

bool mrutils::Socket::initServer(bool reuse) {
    isServer = true;

    int ret = SOCKET_ERROR;

    switch (type) {
        case SOCKET_UNIX: {
            #if defined(_MR_IS_UNIX)
                unlink(addrUnix.sun_path);
                ret = bind(s_, (struct sockaddr*) &addrUnix, 
                1+strlen(addrUnix.sun_path) + sizeof(addrUnix.sun_family));
            #else 
                return (connected = false);
            #endif
        } break;

        case SOCKET_STREAM: {
            // reuse
            if (reuse) {
                if (0>setsockopt(s_, SOL_SOCKET, SO_REUSEADDR, (char*)&addr, sizeof(addr))) {
                    perror("socket stream server enable reuse");
                    return false;
                }

                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)                                                                  
                #else
                    #ifdef SO_REUSEPORT
                        if (setsockopt(s_, SOL_SOCKET, SO_REUSEPORT, (char*)&flagOn, sizeof(flagOn))
                            < 0) {
                            perror("setting SO_REUSEPORT");
                        }
                    #endif
                #endif
            }

            ret = bind(s_, (sockaddr*)&addr, sizeof(sockaddr_in));
        } break;

        case SOCKET_MULTICAST: {
            addr.sin_addr.s_addr=inet_addr(serverIP.c_str());

            // enable loop-back
            if (setsockopt(s_, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&flagOn, sizeof(flagOn))
                < 0) {
                perror("multicast error enabling loopback");
                return false;
            }

            // set TTL to 1
            if (setsockopt(s_, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&flagOn, sizeof(flagOn))
                < 0) {
                perror("multicast error setting IP_MULTICAST_TTL");
                return false;
            }

            // no listen for multicast socket server
            return (connected = true);
        } break;

        default:
            break;
    }

    if (ret == SOCKET_ERROR) {
        perror("Socket connection error");
        return (connected = false);
    } else {
        listen(s_, SOMAXCONN);
        return (connected = true);
    }
}

void mrutils::Socket::reopenStream() {
    ::shutdown(s_,SHUT_RDWR); ::closesocket(s_);
    s_ = socket(AF_INET, SOCK_STREAM, 0);
    if (!setToBlock) ioctl(s_,FIONBIO,&one);
    FD_ZERO(&fdsread_); FD_ZERO(&fdswrite_);
    selmax_ = s_+1; blocking = setToBlock;
}

bool mrutils::Socket::initClient(const int seconds, const int stopFD) {
    switch (type) {
        case SOCKET_UNIX:
        #if defined(_MR_IS_UNIX)
            return (connected = ::connect(s_, (sockaddr*)&addrUnix, 
                1+strlen(addrUnix.sun_path) + sizeof(addrUnix.sun_family)) != -1);
        #else
            return false;
        #endif

        case SOCKET_STREAM: {
            if (connected) {
                connected = false;
                reopenStream();
            }

            // interruption fd set
            fd_set readSet; FD_ZERO(&readSet);
            waitTime.tv_sec = seconds; waitTime.tv_usec = 0;
            FD_SET(s_,&fdswrite_);
            int maxFD = std::max(selmax_,stopFD+1);

            // resolve host
            hostent *host = NULL;
            for (int i = 0; NULL == (host = gethostbyname(serverIP.c_str())); ++i) {
                std::cerr << "Can't resolve host " << serverIP << std::endl; 
                if (i >= seconds) return (connected = false); 

                // sleep 1 second
                if (stopFD < 0) Sleep(1000);
                else {
                    FD_SET(stopFD,&readSet);
                    waitTime.tv_sec = 1; waitTime.tv_usec = 0;
                    if (0 < select(maxFD,&readSet,NULL,NULL,&waitTime)) 
                    { reopenStream(); return (connected = false); }
                }
            } addr.sin_addr = *((in_addr*)host -> h_addr);

            // set nonblocking
            if (blocking) { ioctl(s_,FIONBIO,&one); setToBlock = true; blocking = false; }

            for (int i = -1; i < seconds; ++i) {
                if (0 > ::connect(s_, (sockaddr*)&addr, sizeof(sockaddr))) {
                    switch (
                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                        WSAGetLastError()
                    #else
                        errno
                    #endif
                    ) {
                        case EISCONN:
                            if (setToBlock) { ioctl(s_,FIONBIO,&zero); blocking = true; }
                            return (connected = true);

                        case ETIMEDOUT:
                        case ENETUNREACH:
                        case EINTR:
                        case ECONNREFUSED:
                        case EADDRNOTAVAIL:
                        case EADDRINUSE:
                        case ECONNRESET:
                        case EHOSTUNREACH:
                        case ENETDOWN:
                            // the socket is invalidated
                            reopenStream(); FD_SET(s_,&fdswrite_); 
                            if (stopFD < 0) {
                                maxFD = selmax_;
                                if (i >= 0) Sleep(1000);
                            } else {
                                maxFD = std::max(selmax_,stopFD+1);
                                if (i >= 0) {
                                    FD_SET(stopFD,&readSet);
                                    waitTime.tv_sec = 1; waitTime.tv_usec = 0;
                                    if (0 < select(maxFD,&readSet,NULL,NULL,&waitTime)) 
                                    { reopenStream(); return (connected = false); }
                                }
                            }
                            break;

                        case EINPROGRESS:
                        case EALREADY:
                            // socket still valid but not connected
                            if (stopFD < 0) {
                                if (i >= 0) {
                                    Sleep(1000);
                                    waitTime.tv_sec = seconds - i;
                                } else waitTime.tv_sec = seconds;
                                waitTime.tv_usec = 0;
                                if (0 >= select(maxFD,NULL,&fdswrite_,NULL,&waitTime)) 
                                { reopenStream(); return (connected = false); }
                            } else {
                                if (i >= 0) {
                                    FD_SET(stopFD,&readSet);
                                    waitTime.tv_sec = 1; waitTime.tv_usec = 0;
                                    if (0 < select(maxFD,&readSet,NULL,NULL,&waitTime)) 
                                    { reopenStream(); return (connected = false); }
                                    waitTime.tv_sec = seconds - i;
                                } else waitTime.tv_sec = seconds;
                                waitTime.tv_usec = 0; FD_SET(stopFD,&readSet);
                                if (0 >= select(maxFD,&readSet,&fdswrite_,NULL,&waitTime)
                                    ||FD_ISSET(stopFD,&readSet)) 
                                { reopenStream(); return (connected = false); }
                            }
                            break;

                        default:
                            // other error
                            reopenStream();
                            return (connected = false);
                    }
                } else {
                    if (setToBlock) { ioctl(s_,FIONBIO,&zero); blocking = true; }
                    return (connected = true);
                }
            }

            reopenStream();
            return (connected = false);
        } break;

        case SOCKET_MULTICAST: {
            /* bind address to socket */
            if ((bind(s_, (struct sockaddr *) &addr, sizeof(addr))) < 0) {
                perror("multicast bind() failed");
                return (connected = false);
            }

            /* construct an IGMP join request structure */
            struct ip_mreq mc_req; 
            mc_req.imr_multiaddr.s_addr = inet_addr(serverIP.c_str());
            mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

            /* send an ADD MEMBERSHIP message via setsockopt */
            if ((setsockopt(s_, IPPROTO_IP, IP_ADD_MEMBERSHIP
                ,(char*) &mc_req, sizeof(mc_req))) < 0) {
                perror("setsockopt() multicast ADD_MEMBERSHIP failed");
                return (connected =false);
            }

            // loopback data
            if ((setsockopt(s_, IPPROTO_IP, IP_MULTICAST_LOOP
                ,(char*) &mc_req, sizeof(mc_req))) < 0) {
                perror("setsockopt() multicast IP_MULTICAST_LOOP failed");
                return (connected = false);
            }

        } break;
    }

    return (connected = true);
}

#ifdef MR_GNUTLS
    bool mrutils::Socket::makeSecure(const char * a, const char * b) {
        if (!connected) return false;

        isSRP = (a!=NULL);
        const int  priorityProtocols[]  = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };
        int ret; const char * err;

        // check that GNUTLS is initialized
        if (!gcry_control (GCRYCTL_INITIALIZATION_FINISHED_P)) {
            std::cerr << "GnuTLS must be initialized first by the application." << std::endl;
            return false;
        }


        if (isSRP) { // srp 
            if (isServer) {
                gnutls_srp_allocate_server_credentials (&srp_server_cred);
                gnutls_srp_set_server_credentials_file (srp_server_cred, a, b);
            } else {
                gnutls_srp_allocate_client_credentials (&srp_client_cred);
                gnutls_srp_set_client_credentials (srp_client_cred, a, b);
            }
        } else {
            gnutls_certificate_allocate_credentials (&xcred);
            //gnutls_certificate_set_x509_trust_file (xcred, "cas file", GNUTLS_X509_FMT_PEM);
        }

        gnutls_init (&session, (isServer?GNUTLS_SERVER:GNUTLS_CLIENT));

        /* Encryption priorities */
        ret = gnutls_priority_set_direct (session, (isSRP?"NORMAL:+SRP":"PERFORMANCE:!ARCFOUR-128"), &err);
        if (ret < 0) {
            if (ret == GNUTLS_E_INVALID_REQUEST) {
                fprintf (stderr, "Syntax error at: %s\n", err);
            }
            return false;
        }

        /* Protocol priorities */ 
        ret = gnutls_protocol_set_priority(session, priorityProtocols);
        if (ret < 0) {
            printf("unable to set proto priority\n");
            return false;
        }

        if (isSRP) {
            if (isServer) {
                gnutls_credentials_set (session, GNUTLS_CRD_SRP, srp_server_cred);
                gnutls_certificate_server_set_request (session, GNUTLS_CERT_IGNORE);
            } else {
                gnutls_credentials_set (session, GNUTLS_CRD_SRP, srp_client_cred);
            }
        } else {
            /* put the x509 credentials to the current session */
            gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, xcred);
        }

        gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) s_);

        /* Perform the TLS handshake */
        ret = gnutls_handshake (session);
        if (ret < 0) {
            fprintf (stderr, "*** Handshake failed\n");
            gnutls_perror (ret);
            return false;
        }

        return (isSecure=true);
    }
    void mrutils::Socket::makeUnsecure() {
        if (!isSecure) return;

        gnutls_bye (session, GNUTLS_SHUT_RDWR);
        gnutls_deinit (session);

        if (isServer) {
            if (isSRP) {
                gnutls_srp_free_server_credentials (srp_server_cred);
            } else {
                gnutls_certificate_free_credentials (xcred);
            }
        } else {
            if (isSRP) {
                gnutls_srp_free_client_credentials (srp_client_cred);
            } else {
                gnutls_certificate_free_credentials (xcred);
            }
        }

        isSecure = false;
    }
#endif
