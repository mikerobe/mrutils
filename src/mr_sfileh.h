#ifndef _MR_SOCKETFILEHANDLER_H
#define _MR_SOCKETFILEHANDLER_H

#include "mr_exports.h"
#include "mr_sockets.h"
#include "mr_bufferedwriter.h"
#include "mr_bufferedreader.h"

namespace mrutils {

class _API_ SocketFileHandler {
    public:
        /**
          * Responds to a filehandle message
          * Reader is assumed to already have a line to read on it. 
          */
        bool handleMessage(mrutils::BufferedReader& reader, FILE * verboseOut = NULL);


    private:
        mrutils::BufferedWriter writer;
        char buffer[1024];
};

}


#endif
