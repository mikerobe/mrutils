#ifndef _MR_TLSCONNECTION_H
#define _MR_TLSCONNECTION_H

#include "mr_exports.h"
#include "mr_sockets.h"
#include "mr_bufferedreader.h"
#include "fastdelegate.h"

#define ssize_t long
#include <gnutls/gnutls.h>
#include <gnutls/extra.h>
#include <vector>

namespace mrutils {

class _API_ TLSConnection {
    public:
        virtual bool send(const char * msg, int len = 0) = 0;
        virtual bool nextLine() = 0;

    public:
        mrutils::BufferedReader reader;
};

}


#endif
