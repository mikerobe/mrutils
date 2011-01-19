#include "mr_curl.h"
#include "mr_numerical.h"
#include "mr_sockets.h"
#include "mr_set.h"
#include "mr_bufferedreader.h"

#ifdef OK_OUTSIDE_LIBS
#include "mr_gzipreader.h"
#endif

#define RECV_P(n) \
    for (int tot = n;;) {\
        if (data.stopFD != -1) FD_SET(data.stopFD,&data.fds); \
        if (0 >= select(data.maxFD,&data.fds,NULL,NULL,&waitTime)||!FD_ISSET(data.s_,&data.fds)) return -1;\
        const int r = ::recv(data.s_,p,tot,0);\
        if (r <= 0) return -1;\
        tot -= r; p += r; if (tot == 0) break;\
    }

namespace {
    struct getChunkData_t {
        // for reading from socket
        int maxFD;
        fd_set fds;
        int stopFD;
        int s_;

        // chunk details
        int left, length;
        bool done;

        char buffer[16];

        // this was read on the buffer but not parsed
        std::string stubLine;

        getChunkData_t(const mrutils::Socket& sock, const int stopFD)
        : maxFD((sock.s_>stopFD ? sock.s_:stopFD)+1), stopFD(stopFD), s_(sock.s_)
         ,left(0), length(0), done(false)
        {}

        inline void reset() {
            left = length = 0; done = false;
            stubLine.clear();
            FD_ZERO(&fds);
            FD_SET(s_,&fds);
        }

       /**
        * Takes what's currently on the buffer and parses it
        */
        void parseOnBuffer(mrutils::BufferedReader& reader) {
            char * p = reader.pos, *q = p, *d = p;

            for (;;) {
                // p points to the hex number
                bool good = false;
                for (q = p; q != reader.eob; ++q) if (*q == '\n') { good = true; ++q; break; }
                if (!good) { stubLine.assign(p,reader.eob - p); reader.eob = d; return; }

                int onBuffer = reader.eob - q;

                // translate the data back
                left = mrutils::atoiHex(p);

                if (left == 0) {
                    done = true;
                    memcpy(d,q,reader.eob - q);
                    reader.eob += d - q;
                    return;
                }

                length += left;
                if (onBuffer <= left) {
                    memcpy(d,q,onBuffer);
                    left -= onBuffer;
                    reader.eob = d + onBuffer;
                    return;
                } else {
                    memcpy(d,q,left);
                    d += left;

                    // first \r\n
                    if (onBuffer <= left + 2) { reader.eob = d; return; }

                    p = q + left + 2;
                    left = 0;
                }

            }
        }

       /**
        * Advances to the end of the chunked section
        */
        inline bool finish(mrutils::BufferedReader& reader) {
            if (done) return true;
            for (;;) {
                if (!reader.skip(left)) return false;
                if (!reader.skip(2) || !reader.nextLine()) return false;
                if (0 == (left = mrutils::atoiHex(reader.line))) break;
            } return (done = true);
        }
    };

    static int getChunk(void * data_, char * dest, int size) {
        getChunkData_t & data = *(getChunkData_t*)data_;

        if (data.done) return 0;

        TIMEVAL waitTime;
        waitTime.tv_sec = 5; waitTime.tv_usec = 0;
        char * p = data.buffer;

        if (data.left == 0) {
            // next read is either \r\n# or #
            p = data.buffer; RECV_P(3)
            int i;
            if (*data.buffer == '\r') {
                RECV_P(2)
                i = 4;
            } else i = 2;

            for (;i < (int)sizeof(data.buffer);i += 2) {
                if (data.buffer[i] == '\n') break;
                if (data.buffer[i] == '\r') { RECV_P(1) break; }
                RECV_P(2);
            }

            if (i == (int)sizeof data.buffer) {
                data.buffer[15] = '\0';
                std::cerr << "invalid chunked data size: " << data.buffer << std::endl;
                return 0;
            }

            data.left = mrutils::atoiHex(*data.buffer == '\r' ? (data.buffer + 2) : data.buffer );

            if (data.left == 0) { data.done = true; return 0; }
        } 

        p = dest;
        if (data.left < size) size = data.left;
        RECV_P(size)
        data.left -= size;
        return size;
    }

    struct headerData_t {
        bool redirected, gzipped, chunked;
        std::string location;
        int length;
        
        headerData_t() 
        : redirected(false)
         ,gzipped(false)
         ,chunked(false)
         ,length(-1)
        {}

        inline void reset() {
            redirected = gzipped = chunked = false;
            length = -1; location.clear();
        }
    };

    int readHeader(mrutils::BufferedReader& reader, const char * url, headerData_t * data) {
        char * p, * q;

        // skip any empty lines
        for (;;) {
            if (!reader.nextLine()) return -1;
            if (*reader.line != '\r') break;
        }

        if (0!=strncmp(reader.line,"HTTP",4)
            || NULL == (p = strchr(reader.line,' '))) {
            std::cerr << "mrutils::curl received invalid response from " << url << ": " << reader.line << std::endl;
            return -1;
        }

        //std::cout << reader.line << std::endl;

        bool good = false;

        switch (*++p) {
            case '1': // informational. no content
                std::cerr << "mrutils::curl pure information reply to " << url << std::endl;
                break;

            case '2': // success
                good = true;
                break;

            case '3': // redirected
                data->redirected = true;
                break;

            case '4': // client error
                if (NULL != (q = strchr(url, '#'))) {
                    // try another request without the #
                    data->location.assign(url,q-url);
                    data->redirected = true;
                    break;
                } 

            default: // 5 (server error)
                std::cerr << "mrutils::curl Got error code " << *p << *(p+1) << *(p+2) << " from " << url << std::endl;
                break;
        }

        // read the rest of the header
        for (;reader.nextLine() && 0!=strcmp(reader.line,"\r");) {
            //std::cout << reader.line << std::endl;
            if (mrutils::startsWithI(reader.line,"Content-Length:")) {
                p = reader.line + ctlen("Content-Length:");
                while (*p == ' ' || *p == '\t') ++p;
                data->length = atoi(p);
            } else if (mrutils::startsWithI(reader.line,"Content-Encoding:")) {
                p = reader.line + ctlen("Content-Encoding:");
                while (*p == ' ' || *p == '\t') ++p;
                data->gzipped = (0==mrutils::strnicmp(p,"gzip",4));
            } else if (mrutils::startsWithI(reader.line,"Transfer-Encoding:")) {
                p = reader.line + ctlen("Transfer-Encoding:");
                while (*p == ' ' || *p == '\t') ++p;
                data->chunked = (0==mrutils::strnicmp(p,"chunked",7));
            } else if (mrutils::startsWithI(reader.line,"Location:")) {
                p = reader.line + ctlen("Location:");
                while (*p == ' ' || *p == '\t') ++p;
                q = strpbrk(p," \t\r");
                if (q != NULL) data->location.assign(p,q-p);
                else data->location = p;
            }
        }
        //std::cout << std::endl;

        if (data->redirected && data->location.empty()) {
            std::cerr << "mrutils::curl got redirected with no location from " << url << std::endl;
            return false;
        }

        return good;
    }

    std::string getURL(const mrutils::curl::urlRequest_t& request, mrutils::curl::curlData_t * data, mrutils::curl::curlDataM_t* dataM, int stopFD = -1, int redirCount = 0) {
        using namespace mrutils::curl; 
        if (data != NULL) data->eob = data->buffer;

        if (request.url == NULL || *request.url == 0) return (std::cerr << "attempt to call getURL on empty URL" << std::endl, "");
        if (!mrutils::startsWithI(request.url,"http://")) return "";
        if (redirCount > 5) return (std::cerr << "Too many redirects for url: " << request.url << std::endl,"");
    
        static char empty = '\0';
        static const char * mobileAgent = "SonyEricssonT610/R501 Profile/MIDP-1.0 Configuration/CLDC-1.0";
        static const char * nativeAgent = "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_4; en-us) AppleWebKit/534.11+ (KHTML, like Gecko) Version/5.0.2 Safari/533.18.5";
        int len;


        // replace ' ' with '+' in url
        std::string url = request.url;
        for (std::string::iterator it = url.begin(); it != url.end(); ++it) if (*it == ' ') *it = '+';

        char * host = const_cast<char*>(url.c_str()) + 7;
        char * file = strchr(host,'/');
        int port = 80;
    
        if (!file) file = &empty;
        else *file++ = '\0';

        // check for port
        if (char * portStr = strchr(host,':')) {
            port = atoi(portStr+1); *portStr = '\0';
        }

        mrutils::stringstream ss;
        oprintf(ss,"\
GET /%s HTTP/1.0\r\n\
Host: %s\r\n\
User-Agent: %s\r\n\
Accept: %stext/html;q=0.9, image/jpg;q=0.5, image/jpeg;q=0.5, image/png;q=0.5, image/gif;q=0.5\r\n\
Accept-Language: en, fr;q=0.50\r\n"
#ifdef OK_OUTSIDE_LIBS
"Accept-Encoding: gzip,identity\r\n"
#else
"Accept-Encoding: identity\r\n"
#endif
"Accept-Charset: ISO-8859-1, utf-8;q=0.66, *;q=0.66\r\n\
Referer: %s\r\n\
Connection: close\r\n\
\r\n"
        ,file
        ,host
        ,request.mobile ? mobileAgent : nativeAgent
        ,request.xmlOk ? "text/xml," : ""
        ,request.referer
    );
        //std::cout << ss << std::endl;
    
        mrutils::Socket sock(host,port);
        if (!sock.initClient(5,stopFD)) return (std::cerr << "Can't connect to " << host << std::endl,"");
        if ((int)ss.str().size() != sock.write(ss.c_str(), ss.str().size())) return (std::cerr << "mrutils::curl got disconnected sending request to " << host << std::endl,"");

        mrutils::BufferedReader reader("",data == NULL ? 32768 : data->buffSz-1, data == NULL ? NULL : data->buffer);
        reader.setSocketInterruptFD(stopFD); reader.setSocketWaitTime(5); 
        reader.use(sock);
    
        headerData_t header;
        int ret = readHeader(reader,request.url,&header);
        if (ret == -1) return "";
        if (ret == 0 && !header.redirected) return "";
    
        if (header.redirected) {
            urlRequest_t newRequest(request);
            newRequest.url = header.location.c_str();
            return getURL(newRequest,data,dataM,stopFD,++redirCount);
        }

        if (header.chunked) return (std::cerr << "mrutils::curl sent HTTP/1.0 protocol, got chunked data back: " << request.url << std::endl, "");

    #ifdef OK_OUTSIDE_LIBS
        if (header.gzipped) {
            // need to move reader to a new buffer if currently writing directly to data buffer
            std::string readBuf_; readBuf_.resize(reader.bufSize);
            char * readBuf = const_cast<char *>(readBuf_.c_str());

            if (data != NULL) {
                reader.switchBuffer(readBuf);
                data->eob = data->buffer;

                mrutils::GZipReader gz(reader);
                // copy to buffer
                for (int r, size = data->buffSz; size > 0 && 0 != (r = gz.read(data->eob,size)); data->eob += r, size -= r)
                    if (r < 0) return (std::cerr << "mrutils::curl error gunzipping file at " << request.url << std::endl, "");
                *data->eob = '\0';
            } else {
                mrutils::GZipReader gz(reader);
                for (int r;0 < (r = gz.read(readBuf,reader.bufSize)); dataM->content.write(readBuf,r)) ;
            }
        } else 
    #endif
        {
            if (header.length < 0) {
                if (data != NULL) {
                    len = reader.read(0); reader.switchBuffer(data->buffer);
                    if (len < 0 || 0 > (len = reader.read(reader.bufSize))) {
                        std::cerr << "mrutils::curl error getting data for " << request.url << std::endl;
                        return "";
                    } *(data->eob = data->buffer + len) = '\0';
                } else {
                    for (int l = reader.unreadOnBuffer();0 < (l = reader.read(l))
                        ;dataM->content.write(reader.line,l),l = reader.bufSize) ;
                }
            } else {
                if (data != NULL) {
                    // retrieve partial if too big
                    if (header.length > reader.bufSize) {
                        std::cerr << "warning page at " << request.url << " is bigger than buffer (" << reader.bufSize << ")" << std::endl;
                        header.length = reader.bufSize;
                    }

                    reader.read(0);
                    reader.switchBuffer(data->buffer);
                    if (header.length != reader.read(header.length)) {
                        std::cerr << "mrutils::curl error getting data for " << request.url << std::endl;
                        return "";
                    } *(data->eob = data->buffer + header.length) = '\0';
                } else {
                    for (int l = reader.unreadOnBuffer();header.length > 0;header.length -= l, l = reader.bufSize) {
                        if (header.length < l) l = header.length;
                        if (l != reader.read(l)) {
                            std::cerr << "mrutils::curl got disconnected reading from " << host << std::endl;
                            return "";
                        } dataM->content.write(reader.line,l);
                    }

                }
            }
        }
    
        return request.url;
    }

    struct redirect_t { 
        const mrutils::curl::urlRequestM_t * request;
        std::string location;
    };

}

std::string mrutils::curl::getURL(const urlRequest_t& request, curlDataM_t& dataM, int stopFD) {
    return ::getURL(request,NULL,&dataM,stopFD,0);
}

std::string mrutils::curl::getURL(const urlRequest_t& request, curlData_t& data, int stopFD) {
    return ::getURL(request,&data,NULL,stopFD,0);
}

int mrutils::curl::getMultiple(const std::set<urlRequestM_t>& requests_, std::vector<curlDataM_t> * data, int stopFD) {
    if (requests_.empty()) {
        std::cerr << "call to " << __PRETTY_FUNCTION__ << " with empty request" << std::endl;
        return 0;
    }

    int hits = 0;

    mrutils::BufferedReader reader; 
    reader.setSocketInterruptFD(stopFD); reader.setSocketWaitTime(5); 
    mrutils::stringstream ss;
    std::string scratchBuffer;

    std::vector<const urlRequestM_t*> requests; requests.resize(requests_.size());
    if (data->size() != requests_.size()) data->resize(requests_.size());

    // set the id of each request
    { int i = 0; 
      for (std::set<urlRequestM_t>::const_iterator it = requests_.begin(); it != requests_.end(); ++it, ++i) {
          (*it).id = i; requests[i] = &*it;
      }
    }

    std::vector<redirect_t> redirects; redirects.reserve(requests.size());

    for (std::vector<const urlRequestM_t*>::iterator start = requests.begin(), end = start
        ;start != requests.end(); start = end) {
        if (!mrutils::startsWithI((*start)->url.c_str(),"http://")) { ++end; continue; }

        const char * p, *q;

        std::string host; p = (*start)->url.c_str() + ctlen("http://");
        if (NULL == (q = strchr(p,'/'))) host = p; else host.assign(p,q-p);

        for (; end != requests.end(); ++end) {
            if (!mrutils::startsWithI((*end)->url.c_str(),"http://")) break;
            p = (*end)->url.c_str() + ctlen("http://");
            if (NULL == (q = strchr(p,'/'))) {
                if (0 != strcmp(host.c_str(),p)) break;
            } else if ((int)host.size() != q-p || 0!=strncmp(host.c_str(),p,host.size())) break;
        }

        // build request
        ss.clear();
        for (std::vector<const urlRequestM_t*>::iterator it = start, last = end-1; it != end; ++it) {
            const char * file = (*it)->url.c_str() + ctlen("http://") + host.size();
            if (*file == '/') ++file;
            oprintf(ss,"\
GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
Accept: text/xml, text/html;q=0.9, image/jpg;q=0.5, image/jpeg;q=0.5, image/png;q=0.5, image/gif;q=0.5\r\n\
Accept-Language: en, fr;q=0.50\r\n"
#ifdef OK_OUTSIDE_LIBS
"Accept-Encoding: gzip,identity\r\n"
#else
"Accept-Encoding: identity\r\n"
#endif
"Accept-Charset: ISO-8859-1, utf-8;q=0.66, *;q=0.66\r\n\
Connection: %s\r\n\
\r\n",file
    ,host.c_str()
    ,it == last ? "close" : "keepalive");
        }

        //std::cerr << ss << std::endl;

        // connect to host & send requests
        mrutils::Socket sock(host.c_str(),80);
        if (!sock.initClient(5,stopFD)) {
            std::cerr << "Can't connect to " << host << std::endl;
            continue;
        }
        if (sock.write(ss.c_str(),ss.str().size()) != (int)ss.str().size()) {
            std::cerr << "mrutils::curl got disconnected sending request to " << host << std::endl;
            continue;
        }

        reader.use(sock);

        // for chunked data
        headerData_t header;
        getChunkData_t chunkData(sock,stopFD);

        // parse each result
        for (std::vector<const urlRequestM_t*>::iterator it = start, last = end-1; it != end; ++it) {
            header.reset();
            const int ret = readHeader(reader,(*it)->url.c_str(),&header);
            if (ret == -1) { end = it; if (end == start) ++end; break; }

            if (header.redirected) {
                int i = (int) redirects.size(); redirects.resize(i+1);
                redirects[i].request = *it;
                redirects[i].location = header.location;
            }

            bool skipContent = header.redirected || ret == 0;

            if (header.length < 0) {
                if (header.chunked) {
                    chunkData.reset();
                    reader.open(&getChunk,(void*)&chunkData,false);
                    chunkData.parseOnBuffer(reader);

                    if (header.gzipped) {
                        mrutils::GZipReader gz(reader);
                        scratchBuffer.resize(32768);
                        char * p = const_cast<char *>(scratchBuffer.c_str());
                        if (skipContent) for (int r;0 < (r = gz.read(p,32768));) ;
                        else {
                            for (int r, id = (*it)->id;0 < (r = gz.read(p,32768)); data->at(id).content.write(p,r)) ;
                        }
                    } else {
                        if (chunkData.done) {
                            if (skipContent) {
                                reader.skip(chunkData.length);
                            } else {
                                reader.read(chunkData.length);
                                data->at((*it)->id).content.write(reader.line,chunkData.length);
                            }
                        } else {
                            if (skipContent) {
                                // go until appear to be out of data
                                for (int l = reader.unreadOnBuffer();0 < (l = reader.read(l))
                                    ;l = reader.bufSize) ;
                            } else {
                                for (int l = reader.unreadOnBuffer(), id = (*it)->id;0 < (l = reader.read(l))
                                    ;data->at(id).content.write(reader.line,l),l = reader.bufSize) ;
                            }
                        }
                    }

                    reader.use(sock,false);
                    if (!chunkData.finish(reader)) {
                        std::cerr << "mrutils::curl chunked data error: line isn't 0 at " << reader.line << " " << (*it)->url << std::endl;
                        break;
                    }
                } else {
                    // then HTTP/1.0
                    if (!skipContent) {
                        for (int l = reader.unreadOnBuffer(), id = (*it)->id;0 < (l = reader.read(l))
                            ;data->at(id).content.write(reader.line,l),l = reader.bufSize) ;
                    }
                    end = it + 1;
                }
            } else {
                if (skipContent) {
                    if (!reader.skip(header.length)) {
                        std::cerr << "mrutils::curl got disconnected reading from " << host << std::endl;
                        break;
                    }
                } else {
                    bool good = true;

                    for (int l = reader.unreadOnBuffer();header.length > 0;header.length -= l, l = reader.bufSize) {
                        if (header.length < l) l = header.length;
                        if (l != reader.read(l)) {
                            std::cerr << "mrutils::curl got disconnected reading from " << host << std::endl;
                            good = false;
                            break;
                        }

                        data->at((*it)->id).content.write(reader.line,l);
                    }

                    if (!good) break;
                }
            }
            if (!skipContent) ++hits;
        }
    }

    // handle redirects
    for (std::vector<redirect_t>::iterator it = redirects.begin(); it != redirects.end(); ++it) {
        if (!::getURL(urlRequest_t((*it).location.c_str()), NULL, &data->at((*it).request->id),stopFD,0).empty()) ++hits;
    }

    return hits;
}

/*
namespace {
    inline int writerRecv(char * content, size_t size, size_t nmemb, mrutils::curl::recv_t * data) {
        content += data->blockRead;

        int copied = size * nmemb - data->blockRead;
        if (copied > data->sz) {
            int sz = data->sz;
            data->sz = 0; 
            data->thisRead += sz;
            data->blockRead += sz;
            memcpy(data->buffer, content, sz);
            data->buffer += sz; 
            return CURL_WRITEFUNC_PAUSE;
        } else {
            data->blockRead = 0;
            data->thisRead += copied;
            memcpy(data->buffer, content, copied);
            data->sz -= copied; data->buffer += copied;
            return (size * nmemb);
        }
    }
}

bool mrutils::curl::recv_t::init() {
    if (request.url == NULL || *request.url == 0) {
        fprintf(stderr,"attempt to call mrutils::curl::recv on empty URL\n");
        return false;
    }

    if (NULL == (curl = curl_easy_init())) {
        fprintf(stderr,"Unable to init curl");
        return false;
    }

    if (NULL == (multi_handle = curl_multi_init())) {
        curl_easy_cleanup(curl); curl = NULL;
        fprintf(stderr,"Unable to init multi curl");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, request.url);  
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, NULL);  
    curl_easy_setopt(curl, CURLOPT_ENCODING, "");  
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);  
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writerRecv);  
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);  

    if (request.cookieFile != NULL && *request.cookieFile != 0) 
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, request.cookieFile);

    if (request.cookieData != NULL && *request.cookieData != 0) 
        curl_easy_setopt(curl, CURLOPT_COOKIE, request.cookieData);

    if (request.postFields != NULL && *request.postFields != 0)
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.postFields); 

    if (request.referer != NULL && *request.referer != 0) 
        curl_easy_setopt(curl, CURLOPT_REFERER, request.referer);

    if (request.mobile)
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SonyEricssonT610/R501 Profile/MIDP-1.0 Configuration/CLDC-1.0");  

    curl_multi_add_handle(multi_handle, curl);

    while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &running)){}

    if (!running) {
        curl_multi_cleanup(multi_handle); curl_easy_cleanup(curl);
        curl = NULL; multi_handle = NULL;
        fprintf(stderr,"CURL: can't get url %s in %s: %d (%d: %s)"
            ,request.url,__FILE__,__LINE__,errno, strerror(errno));
        return false;
    }

    FD_ZERO(&fdread); FD_ZERO(&fdwrite); FD_ZERO(&fdexcep);
    if (CURLM_OK != curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxFD)) {
        curl_multi_cleanup(multi_handle); curl_easy_cleanup(curl);
        multi_handle = NULL; curl = NULL;
        fprintf(stderr,"CURL: error on curl_multi_fdset at %s: %d (%d: %s)"
            ,__FILE__,__LINE__,errno, strerror(errno));
        return false;
    }

    if (stopFD != -1) {
        FD_SET(stopFD,&fdread);
        maxFD = MAX_(maxFD,stopFD)+1;
    } else ++maxFD;

    fdread_ = fdread; fdwrite_ = fdwrite; fdexcep_ = fdexcep;

    return true;
}

int mrutils::curl::recv(void * data_, char * buffer, int sz) {
    recv_t & data = *(recv_t*)data_;
    if (data.curl == NULL && !data.init()) return 0;

    data.thisRead = 0;
    data.buffer = buffer; data.sz = sz;

    while(data.running && data.thisRead == 0) {
        struct timeval timeout;
        timeout.tv_sec = 3; timeout.tv_usec = 0;

        if (select(data.maxFD, &data.fdread, &data.fdwrite, &data.fdexcep, &timeout) <= 0) return 0;
        if (data.stopFD != -1 && FD_ISSET(data.stopFD,&data.fdread)) return 0;

        while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(data.multi_handle, &data.running)){}

        if (data.blockRead != 0)
            curl_easy_pause(data.curl,CURLPAUSE_CONT);
        
        data.fdread = data.fdread_; data.fdwrite = data.fdwrite_; data.fdexcep = data.fdexcep_;

    }

    return data.thisRead;
}
*/
