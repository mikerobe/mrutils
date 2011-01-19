#include "mr_sendmail.h"
#include "mr_strutils.h"

#define LINELEN 72

#define WRITE() \
    if (fp == NULL) {\
        if (!socket.write(ss.c_str())) return false;\
    } else fputs(ss.c_str(),fp)

#define WRITE_STR(str) \
    if (fp == NULL) {\
        if (!socket.write(str)) return false;\
    } else fputs(str,fp)

bool mrutils::Sendmail::connect() {
    if (connected && smtp250("RSET\r\n")) return true;
    close(); 
    const size_t bufSz = 256;
    char buffer[256];

    mrutils::stringstream ss;

    std::string userEncoded, passEncoded;

    if (!mrutils::Encoder::encodeStrTo(username.c_str(),ss,mrutils::encoding::EC_BASE64, buffer, bufSz)) {
        fprintf(stderr,"can't encode: %s\n", username.c_str());
        return false;
    }
    userEncoded = ss.str(); ss.str("");
    if (!mrutils::Encoder::encodeStrTo(password.c_str(),ss,mrutils::encoding::EC_BASE64, buffer, bufSz)) {
        fprintf(stderr,"can't encode: %s\n", password.c_str());
        return false;
    }
    passEncoded = ss.str(); ss.str("");

    if (!socket.initClient(10)) {
        fprintf(stderr,"Couldn't connect to SMTP server %s:%d in 10 seconds\n"
            ,socket.serverIP.c_str(),socket.port);
        return false;
    }

    if (!isSSL) {
        if (!reader.nextLine()) {
            fprintf(stderr,"Couldn't read greeting from plain text SMTP server %s:%d in 10 seconds\n"
                ,socket.serverIP.c_str(),socket.port);
            return false;
        }

        if (!smtp250("HELO domain\r\n")) return false;
        if (smtp250("STARTTLS\r\n","220")) {
            if (!socket.makeSecure()) {
                fprintf(stderr,"could not make smtp secure.\n");
                return false;
            }
        } else {
            fprintf(stderr,"warning: not a secure connection.\n");
            fflush(stderr);
        }
    } else {
        if (!socket.makeSecure()) {
            fprintf(stderr,"could not make smtp secure.\n");
            return false;
        }

        // should be a 220 line
        reader.nextLine();
    }

    if (!smtp250("HELO domain\r\n")) return false;

    // now log in
    if (!smtp250("AUTH LOGIN\r\n","334")) {
        fprintf(stderr,"SMTP got unexpected msg to AUTH LOGIN: %s\n",reader.line);
        return false;
    }

    // send username
    if (!smtp250(userEncoded.c_str(),"334")) {
        fprintf(stderr,"SMTP got unexpected msg to after username: %s\n",reader.line);
        return false;
    }

    // send password
    if (!smtp250(passEncoded.c_str(),"235")) {
        fprintf(stderr,"SMTP rejected login with username %s: %s\n"
            ,username.c_str(), reader.line);
        return false;
    }

    return (connected = true);
}

bool mrutils::Sendmail::send(const char * subject, const char * message, const char * to, FILE * fp) {
    //static const char bc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-_";

    if (to == NULL) to = fromEmail.c_str();

    if (fp == NULL) {
        if (!connect()) return false;

        oprintf(ss.clear(),"MAIL FROM: <%s>\r\n",fromEmail.c_str());
        if (!smtp250(ss.c_str())) return false;

        // TO
        for (const char * p = to
            ,* eob = p + strlen(to)
            ,* q = p
            ;*q && (NULL != (q = strchr(p, ',')) || (q = eob))
            ;p = q+1) {

            ss.clear() << "RCPT TO: <";
            ss.write(p,q-p);
            ss << ">\r\n";

            if (!smtp250(ss.c_str())) return false;
        }

        if (!smtp250("DATA\r\n","354")) {
            fprintf(stderr,"SMTP: got unexpected response to DATA: %s\n", reader.line);
            return false;
        }
    }

    oprintf(ss.clear(),"From: %s\r\n", fromEmail.c_str());
    WRITE();

    ss.clear() << "To: ";
    mrutils::replaceCopy(ss,to,",",",\r\n    ");
    ss << "\r\n";
    WRITE();

    oprintf(ss.clear(),"Subject: %s\r\n",subject); WRITE();
    WRITE_STR("MIME-Version: 1.0\r\n");

    WRITE_STR("Content-Type: text/plain; charset=iso-8859-1\r\n");
    oprintf(ss.clear(),"Content-Transfer-Encoding: %s\r\n\r\n"
        ,mrutils::encoding::encodingName[mrutils::encoding::EC_QUOTED_PRINTABLE]); WRITE();

    // Note: using the BufferedReader buffer for the encoded string
    // OK since no data on the buffer left to be read
    if (fp == NULL) 
        mrutils::Encoder::encodeStrTo(message, socket, mrutils::encoding::EC_QUOTED_PRINTABLE
            ,reader.buffer, reader.bufSize);
    else 
        mrutils::Encoder::encodeStrTo(message, fp, mrutils::encoding::EC_QUOTED_PRINTABLE
            ,reader.buffer, reader.bufSize);

    if (fp == NULL) {
        if (!smtp250("\r\n.\r\n")) return false;
    } else {
        fflush(fp);
    }

    return true;
}
