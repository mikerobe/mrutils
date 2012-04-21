#include "mr_fastfileserver.h"
#include <string>

using namespace mrutils;

std::map<std::string,FastFileServer::FileData*> FastFileServer::files_;
SocketServer* FastFileServer::server_ = NULL;
bool FastFileServer::init__ = false;

std::map<std::string,FastFileServer::FileData*>& FastFileServer::files() { return files_; }
SocketServer*& FastFileServer::server() { return server_; }
bool& FastFileServer::init_() { return init__; }

bool FastFileServer::open(const char * path) {
    if (!init__) return false;

    FILE *fp = fopen(path,"r");
    if (fp == NULL) return false;

    std::cout << "Reading " << path << "." << std::endl;

    // full path
    FileData* fd = new FileData();
    files_.insert(std::pair<std::string,FileData*>(path, fd));

    FileData::Data &data = fd->data;
    FileData::DataSize &dataSize = fd->dataSize;

    unsigned total = 0;
    int read = 0;
    while (
            (data.push_back( (unsigned char*) malloc(SIZE) ),
             data.back() != NULL)
            && (read = in(fp, data.back())) > 0
          ) { 
        dataSize.push_back(read); 
        total += read;
        std::cout << ".";
    }

    std::cout << std::endl << "Read " << total << " bytes" << std::endl;

    if (data.back() == NULL) {
        std::cerr << "FastFileServer unable to allocate memory" << std::endl;
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

unsigned int FastFileServer::in(FILE* fp, unsigned char *next) {
    int ret; unsigned len = 0; 

    do {
        ret = 16384;
        if ((unsigned)ret > SIZE - len)
            ret = (int)(SIZE - len);
        ret = (int)fread(next,1,ret,fp);
        if (ret == -1) {
            len = 0;
            break;
        }
        next += ret;
        len += ret;
    } while (ret != 0 && len < SIZE);
    return len;
}

/* int main(int argc, const char * argv[]) {
    FastFileServer s((argc<2?0:atoi(argv[1])));

    char path[1024];
    for(;;) {
        std::cout << "> ";

        if (std::cin.getline(path,sizeof(path)).eof()) break;
        if (strstr(path,"quit") || strstr(path,"stop") || strstr(path,"exit")) {
            break;
        }
        if (strlen(path) == 0 || strlen(path) == 1) continue;
        if (!s.open(path)) {
            std::cerr << "Unable to open " << path << std::endl;
        }
    }
}
*/
