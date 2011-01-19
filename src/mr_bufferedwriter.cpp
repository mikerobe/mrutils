#include "mr_bufferedwriter.h"


bool mrutils::BufferedWriter::send(const char * start, const char * const end) {
    if (start == end) return true;

    if (fd != -1) {
        const int size = end - start;
        return (size == MR_WRITE(fd,start,size));
    } else if (socket != NULL) {
        for (;;) {
            int ret = 0;
            if (waitSecs < 0) {
                FD_SET(socket->s_,&fdswrite_); if (interruptFD > 0) FD_SET(interruptFD,&fdsread_); 
                if (0 >= select(selmax_, &fdsread_, &fdswrite_, NULL, NULL)) return false;
                if (interruptFD >= 0 && FD_ISSET(interruptFD,&fdsread_)) return false;
                ret = socket->send(start, end - start);
            } else {
                waitTime.tv_sec = waitSecs; waitTime.tv_usec = 0;
                FD_SET(socket->s_,&fdswrite_); if (interruptFD > 0) FD_SET(interruptFD,&fdsread_); 
                if (0 >= select(selmax_, &fdsread_, &fdswrite_, NULL, &waitTime)) return false;
                if (interruptFD >= 0 && FD_ISSET(interruptFD,&fdsread_)) return false;
                ret = socket->send(start, end - start);
            }
            if (ret <= 0) return false;
            if (end == (start += ret)) return true;
        }
    } else return false;
}
