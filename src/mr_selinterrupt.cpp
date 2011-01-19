#include "mr_selinterrupt.h"
#include "mr_threads.h"

static ATOMIC_INT usePort = 8500;
static const int lastPort = 9500;

bool mrutils::SelectInterrupt::init() {
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        // reason is that windows can't select on anything but a real socket
        int port = ATOMIC_INCREMENT(usePort);

        for (;; port = ATOMIC_INCREMENT(usePort)) {
            if (port >= lastPort) { delete server; return false; }
            if (server != NULL) delete server;

            server = new mrutils::Socket(port);
            if (server->initServer()) break;
        }
        
        // now have to accept on a separate thread...
        mrutils::threadRun(fastdelegate::MakeDelegate(this
            ,&SelectInterrupt::serverAccept));

        client = new mrutils::Socket(port);
        if (!client->initClient(10)) {
            delete server; delete client; return false;
        }

        readFD = client->s_;
        maxFD = readFD+1;
        while (!setWrite) mrutils::sleep(10);
    #else
        int fds[2];
        if (0 != MR_PIPE(fds)) return false;

        readFD = fds[0];
        maxFD = readFD+1;
        writeFD = fds[1];
    #endif

    return (good_ = true);
}
