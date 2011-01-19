#include "mr_imap.h"
#include "mr_files.h"
#include "hmac.h"
#include "mr_encoder.h"
#include <fstream>
#include "mr_map.h"
#include "mr_threads.h"

namespace {
    static const char * bin2hex (const void *bin, size_t bin_size) {
        static char printable[110];
        const unsigned char *_bin = (unsigned char *) bin;
        char *print;
        size_t i;

        if (bin_size > 50)
            bin_size = 50;

        print = printable;
        for (i = 0; i < bin_size; i++) {
            sprintf (print, "%.2x ", _bin[i]);
            print += 2;
        }

        return printable;
    }

    /*
    static gnutls_datum_t load_file (const char *file) {
        FILE *f;
        gnutls_datum_t loaded_file = { NULL, 0 };
        long filelen;
        void *ptr;

        if (!(f = fopen (file, "r"))
                || fseek (f, 0, SEEK_END) != 0
                || (filelen = ftell (f)) < 0
                || fseek (f, 0, SEEK_SET) != 0
                || !(ptr = malloc ((size_t) filelen))
                || fread (ptr, 1, (size_t) filelen, f) < (size_t) filelen)
        {
            return loaded_file;
        }

        loaded_file.data = (unsigned char*)ptr;
        loaded_file.size = (unsigned int) filelen;
        return loaded_file;
    }
    static void unload_file (gnutls_datum_t data) { free (data.data); }
    */

    struct xauth {
        std::string authStr;
        unsigned timestamp;
    };

    void makeXOAuth(const char * email, const char * xoauthKey
        ,const char * xoauthToken, const char * xoauthSecret, xauth* xauth) {

        const unsigned timestamp = time(NULL);
        mrutils::stringstream secret;
        secret << xoauthKey << '&' << xoauthSecret;

        srand(timestamp); rand();
        const uint64_t nonce = (1llu << 32) * rand() + rand();

        const size_t bufSz = 1024;
        char buffer[bufSz];
        char url[1024];

        mrutils::stringstream tokenEscaped;
        mrutils::Encoder::encodeStrTo(xoauthToken, tokenEscaped
           ,mrutils::encoding::EC_URL,buffer,bufSz);

        // now url encode
        mrutils::stringstream url1Escaped, url2Escaped, urlEscaped;
        sprintf(url, "https://mail.google.com/mail/b/%s/imap/", email);
        mrutils::Encoder::encodeStrTo(url, url1Escaped
           ,mrutils::encoding::EC_URL,buffer,bufSz);
        sprintf(url, "oauth_consumer_key=%s&oauth_nonce=%llu&oauth_signature_method=HMAC-SHA1&oauth_timestamp=%u&oauth_token=%s&oauth_version=1.0"
            ,xoauthKey
            ,nonce
            ,timestamp
            ,tokenEscaped.str().c_str()
            );
        mrutils::Encoder::encodeStrTo(url, url2Escaped
           ,mrutils::encoding::EC_URL,buffer,bufSz);
        urlEscaped << "GET&" << url1Escaped.str() << "&" << url2Escaped.str();

        //fprintf(stderr,"%s\n",urlEscaped.str().c_str());
        //fprintf(stderr,"secret: %s\n", secret.str().c_str());

        // now SHA1 to get the signature
        char sig[1024]; memset(sig,0,sizeof(sig));
        mrutils::encrypt::hmac_sha1(secret.str().c_str(),strlen(secret.str().c_str())
              ,urlEscaped.str().c_str(),strlen(urlEscaped.str().c_str()),sig);

        //for (char * s = sig; *s; ++s) 
        //  fprintf(stderr,"before: %d\n",(int)*s);

        // encode the sig in base 64
        mrutils::stringstream sigEncoded;
        mrutils::Encoder::encodeStrTo(sig, sigEncoded
           ,mrutils::encoding::EC_BASE64,buffer,bufSz,1024);

        //fprintf(stderr,"before escape: %s\n",sigEncoded.str().c_str());

        // strip the \r\n at the end
        sigEncoded.str()[sigEncoded.str().size()-2] = 0;

        // finally escape the sig
        mrutils::stringstream sigEscaped;
        mrutils::Encoder::encodeStrTo(sigEncoded.str().c_str(), sigEscaped
           ,mrutils::encoding::EC_URL,buffer,bufSz);

        //fprintf(stderr,"signature: %s\n",sigEscaped.str().c_str());

        // build the xoauth string...
        sprintf(url, "GET https://mail.google.com/mail/b/%s/imap/ oauth_consumer_key=\"%s\",oauth_nonce=\"%llu\",oauth_signature=\"%s\",oauth_signature_method=\"HMAC-SHA1\",oauth_timestamp=\"%u\",oauth_token=\"%s\",oauth_version=\"1.0\""
            ,email
            ,xoauthKey
            ,nonce
            ,sigEscaped.str().c_str()
            ,timestamp
            ,tokenEscaped.str().c_str()
            );

        //fprintf(stderr,"final string: %s\n",url);

        // finally encode the xoauth string in base64
        mrutils::stringstream authStr;
        mrutils::Encoder::encodeStrTo(url, authStr
           ,mrutils::encoding::EC_BASE64,buffer,sizeof(buffer),1024);

        // strip the \r\n
        authStr.str()[authStr.str().size()-2] = 0;

        xauth->authStr = authStr.str();
        xauth->timestamp = timestamp;
    }

    MUTEX loginMutex = mrutils::mutexCreate();
    mrutils::map<std::string,xauth> authKeys;
}

int mrutils::IMAP::login(char * buffer, _UNUSED int size) {
    // just to get rid of any bugs connecting simultaneously to the same server...
    mrutils::mutexScopeAcquire a(loginMutex);

    if (!connected) return DISCON;

    std::string key;

    if (xoauthKey.empty()) {
        sprintf(buffer,". login %s %s\r\n"
            ,username.c_str(),password.c_str());
    } else {
        key = email + xoauthKey;
        mrutils::map<std::string,xauth>::iterator it = authKeys.find(key);

        xauth auth;

        // timeout in 10 seconds
        if (it == authKeys.end() || time(NULL) > (time_t)((*it)->second.timestamp + 10)) {
            makeXOAuth(email.c_str(), xoauthKey.c_str()
                    ,xoauthToken.c_str(), xoauthSecret.c_str(), &auth);
            authKeys.insert(key,auth);
        } else { 
            fprintf(stderr,"%s\n","re-using auth key"); fflush(stderr);
            auth = (*it)->second; 
        }

        sprintf(buffer,". AUTHENTICATE XOAUTH %s\r\n",auth.authStr.c_str());
    }

    std::cerr << buffer << std::endl;
    if (!socket.write(buffer)) return DISCON;

    // wait for 10 seconds for a reply
    reader.setSocketWaitTime(10);
    while (reader.nextLine()) {
        std::cerr << "GOT: " << reader.line << std::endl;
        if (mrutils::startsWithI(reader.line,". OK")) {
            return OK;
        } else if (mrutils::startsWithI(reader.line,". BAD")) {
            if (!xoauthKey.empty()) authKeys.erase(authKeys.find(key));
            fprintf(stderr,"IMAP login error: bad syntax on \"%s\", %s\n",buffer, reader.line);
            return BAD;
        } else if (mrutils::startsWithI(reader.line,". NO")) {
            if (!xoauthKey.empty()) authKeys.erase(authKeys.find(key));
            fputs(buffer,stderr);
            fprintf(stderr,"Access denied for %s at %s; %s\n"
                ,username.c_str()
                ,socket.serverIP.c_str()
                ,xoauthKey.empty()?"password":"xoauth");
            fputs(reader.line,stderr);
            return NO;
        }
    }
    std::cerr << "DONE WITH RESPONSES" << std::endl;

    // wait 15 seconds for all future read operations
    reader.setSocketWaitTime(15);

    return DISCON;
}

bool mrutils::IMAP::addCert(const char * path) {
    std::ifstream in(path, std::ios::in);
    ca_list.push_back(static_cast<std::stringstream const*>(&(std::stringstream() << in.rdbuf()))->str());
    return true;

    /*
    gnutls_datum_t data;

    ca_list.resize(ca_list.size()+1);

    data = load_file(path);
    if (data.data == NULL) {
        fprintf (stderr, "*** Error reading cert file at %s\n", path);
        return false;
    }

    gnutls_x509_crt_init (&ca_list.back());

    int ret = gnutls_x509_crt_import (ca_list.back(), &data, GNUTLS_X509_FMT_PEM);
    if (ret < 0) {
        fprintf (stderr, "*** Error importing cert file at %s: %s\n"
            ,path,gnutls_strerror (ret));
        return false;
    }

    //printf("Imported cert\n");
    //printCertInfo(ca_list.back());

    unload_file (data);
    return true;
    */
}


void mrutils::IMAP::printCertInfo(gnutls_x509_crt_t & cert, mrutils::stringstream * ss) {
    static char serial[40];
    static char dn[128];
    size_t size;
    unsigned int algo, bits;
    time_t expiration_time, activation_time;

    expiration_time = gnutls_x509_crt_get_expiration_time (cert);
    activation_time = gnutls_x509_crt_get_activation_time (cert);

    if (ss != NULL) {
        *ss << "Certificate is valid since: " << ctime (&activation_time);
        *ss << "Certificate expires: " << ctime (&expiration_time);
    } else {
        fprintf (stderr,"\tCertificate is valid since: %s", ctime (&activation_time));
        fprintf (stderr,"\tCertificate expires: %s", ctime (&expiration_time));
    }

    /* Print the serial number of the certificate.  */
    size = sizeof (serial);
    gnutls_x509_crt_get_serial (cert, serial, &size);

    if (ss != NULL) {
        *ss << "Certificate serial number: " << bin2hex (serial, size) << "\n";
    } else {
        fprintf (stderr,"\tCertificate serial number: %s\n", bin2hex (serial, size));
    }

    /* Extract some of the public key algorithm's parameters */
    algo = gnutls_x509_crt_get_pk_algorithm (cert, &bits);

    if (ss != NULL) {
        *ss << "Certificate public key: %s" 
            << gnutls_pk_algorithm_get_name ((gnutls_pk_algorithm_t)algo);
    } else {
        fprintf (stderr,"\tCertificate public key: %s",
                gnutls_pk_algorithm_get_name ((gnutls_pk_algorithm_t)algo));
    }

    /* Print the version of the X.509 certificate.  */
    if (ss != NULL) {
        *ss << "Certificate version: #" << gnutls_x509_crt_get_version (cert) << "\n";
    } else {
        fprintf (stderr,"\tCertificate version: #%d\n",
                gnutls_x509_crt_get_version (cert));
    }

    size = sizeof (dn);
    gnutls_x509_crt_get_dn (cert, dn, &size);
    if (ss != NULL) {
        *ss << "DN: " << dn << "\n";
    } else {
        fprintf (stderr,"\tDN: %s\n", dn);
    }

    size = sizeof (dn);
    gnutls_x509_crt_get_issuer_dn (cert, dn, &size);
    if (ss != NULL) { 
        *ss << "Issuer's DN: " << dn << "\n";
    } else {
        fprintf (stderr,"\tIssuer's DN: %s\n", dn);
    }
}

bool mrutils::IMAP::verifyCerts(const char * dir, char * buffer, int size, certOKFunc certOKFn) {
    int ret;

    if (gnutls_certificate_type_get (socket.session) != GNUTLS_CRT_X509) {
        fprintf(stderr,"NOT x509 certificate: %d\n",gnutls_certificate_type_get (socket.session));
        return false; 
    }

    unsigned certCount = 0;
    const gnutls_datum_t * cert_list = gnutls_certificate_get_peers(socket.session, &certCount);

    if (certCount == 0) {
        fprintf(stderr,"\tNO certs\n");
        return false;
    }

    gnutls_x509_crt_t cert;
    if (gnutls_x509_crt_init(&cert) < 0) {
        fprintf(stderr,"\terror in initialization x509 cert\n");
        return false;
    }

    // import first (self-signed) cert
    if (gnutls_x509_crt_import (cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0) {
        fprintf(stderr,"\terror importing last cert\n");
        return false;
    }

    bool good = true;
    mrutils::stringstream ss;

    //ret = gnutls_certificate_verify_peers(session);
    /* this isn't used; I just save certificates & compare bits
    gnutls_x509_crt_verify (cert, &ca_list[0], ca_list.size(),
			  GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT, &ret);

    if (ret & GNUTLS_CERT_INSECURE_ALGORITHM) {
        fprintf(stderr,"The certificate was signed using an insecure algorithm such as MD2 or MD5. These algorithms have been broken and should not be trusted.\n");
        good = false;
    }

    if (ret & GNUTLS_CERT_INVALID) {
        fprintf(stderr,"The certificate is not signed by one of the known authorities, or the signature is invalid.\n");
        good = false;
    }

    if (ret & GNUTLS_CERT_SIGNER_NOT_FOUND) {
        fprintf(stderr,"The certificate's issuer is not known. This is the case when the issuer is not in the trusted certificates list.\n");
        good = false;
    }

    if (ret & GNUTLS_CERT_SIGNER_NOT_CA) {
        fprintf(stderr,"The certificate's signer was not a CA. This may happen if this was a version 1 certificate, which is common with some CAs, or a version 3 certificate without the basic constrains extension.\n");
        good = false;
    }

    if (good) {
        gnutls_x509_crt_deinit(cert);
        return true;
    }
    */

    good = false;

    // write cert to buffer as PEM
    char * p = buffer; size_t pemSize = size;
    if ((ret = gnutls_x509_crt_export( cert, GNUTLS_X509_FMT_PEM, buffer, &pemSize )) < 0) {
        fprintf(stderr,"Unable to write cert as PEM: %s. size %u vs buffer %d\n"
            ,gnutls_strerror (ret)
            ,(unsigned)pemSize, size
            );
        return false;
    } p += pemSize;

    // see if this cert is identical to one of the certs we trust
    // if so, accept it
    for (std::vector<std::string>::iterator it = ca_list.begin()
        , itE = ca_list.end(); it != itE; ++it) {
        /*
        // write this cert to buffer at p
        size_t size_ = (buffer+size)-p;

        if (gnutls_x509_crt_export( cert, GNUTLS_X509_FMT_PEM, p, &size_ ) < 0) {
            fprintf(stderr,"Unable to write cert as PEM.\n");
            return false;
        }
        */

        // do a byte-by-byte comparison
        bool same = true;
        for (const char * p1 = buffer, * p2 = (*it).c_str(); p1 != p;) {
            if (*p1++ != *p2++) {
                same = false; break;
            }
        }
        if (same) return true;
    }

    // check existence of dir
    mrutils::mkdirP(dir);

    good = (certOKFn != NULL);

    int j = 0;
    for (unsigned i = 0; i < certCount; ++i) {
        gnutls_x509_crt_deinit(cert);
        gnutls_x509_crt_init(&cert);

        gnutls_x509_crt_fmt_t format = GNUTLS_X509_FMT_DER;
        ret = gnutls_x509_crt_import (cert, &cert_list[i], format);
        if (ret < 0)  {
            ret = gnutls_x509_crt_import (cert, &cert_list[i], format = GNUTLS_X509_FMT_PEM);
            if (ret < 0) {
                printf("unable to import cert");
                continue;
            }
        }

        mrutils::stringstream path;
        path << dir << "/" << socket.serverIP.c_str() << "_" << i+j << ".cer";
        while (mrutils::fileExists(path.str().c_str())) {
            path.str(""); ++j;
            path << dir << "/" << socket.serverIP.c_str() << "_" << i+j << ".cer";
        }

        if (certOKFn == NULL) 
            fprintf(stderr,"Saving below cert to %s\n", path.str().c_str());
        ss.str("");
        printCertInfo(cert, (certOKFn == NULL?NULL:&ss));

        size_t size_ = size;
        if (0 != gnutls_x509_crt_export( cert, GNUTLS_X509_FMT_PEM, buffer, &size_ )) {
            if ((int)size_ > size) {
                fprintf(stderr,"Unable to save cert - buffer %d too small for %d\n"
                    ,size, (int)size_);
            } else {
                fprintf(stderr,"Unable to save cert.\n");
            }
        } else {
            int outFile;

            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                outFile = MR_OPEN(path.str().c_str(),O_CREAT | O_WRONLY | O_TRUNC, _S_IREAD | _S_IWRITE);
            #else
                outFile = MR_OPEN(path.str().c_str(),O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            #endif

            if (outFile < 0) {
                fprintf(stderr,"Unable to open file.\n");
                continue;
            }

            if (size_ != (size_t)MR_WRITE(outFile, buffer, size_)) {
                fprintf(stderr,"mrutils::IMAP error writing cert to %s\n",path.str().c_str());
            }

            MR_CLOSE(outFile);
        }

        if (certOKFn != NULL) {
            good = good && certOKFn(&ss, path.str().c_str());
        }

    }

    gnutls_x509_crt_deinit(cert);

    return good;
}
