#include "mr_curl.h"
#include "mr_numerical.h"
#include "mr_sockets.h"
#include "mr_set.h"
#include "mr_bufferedreader.h"
#ifdef HAVE_PANTHEIOS
	#include <pantheios/pantheios.hpp>
#endif

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
    /**
     * Returns string timestamp used for error logging
     * TODO: this could be done more efficiently with thread local
     * storage...
     */
    std::string getTime()
    {
        struct tm *timeinfo;
        time_t rawtime;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char buffer[16];
        strftime(buffer,sizeof(buffer),"%X - ", timeinfo);
        return buffer;
    }

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

            if (i == (int)sizeof(data.buffer)) {
                data.buffer[15] = '\0';
				#ifdef HAVE_PANTHEIOS
                pantheios::log(pantheios::error,
                    "invalid chunked data size: ",
                    data.buffer);
				#endif
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

    int readHeader(mrutils::BufferedReader& reader,
            const char * url, headerData_t * data)
    {
        char * p, * q;

        // skip any empty lines
        for (;;) {
            if (!reader.nextLine()) return -1;
            if (*reader.line != '\r') break;
        }

        if (0!=strncmp(reader.line,"HTTP",4)
            || NULL == (p = strchr(reader.line,' ')))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::error,
                "mrutils::curl %s:%d received invalid response from %s: %s",
                __FILE__,__LINE__,url,reader.line);
			#endif
            return -1;
        }

        //std::cout << reader.line << std::endl;

        bool good = false;

        switch (*++p) {
            case '1': // informational. no content
				#ifdef HAVE_PANTHEIOS
                pantheios::log(pantheios::notice,
                        "mrutils::curl pure information reply to ",
                        url);
				#endif
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
				#ifdef HAVE_PANTHEIOS
                pantheios::logprintf(pantheios::notice,
                    "mrutils::curl got error code %c%c%c from %s",
                    *p, *(p+1), *(p+2), url);
				#endif
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
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::notice,
                    "mrutils::curl got redirected with no location from ",
                    url);
			#endif
            return false;
        }

        return good;
    }

    int getURL(mrutils::curl::urlRequest_t* request,
        mrutils::curl::curlData_t * data,
        mrutils::curl::curlDataM_t* dataM,
        int stopFD = -1,
        int redirCount = 0)
    {
        using namespace mrutils::curl;
        char buffer[1024];

        if (data != NULL)
            data->eob = data->buffer;

        if (request->url.empty())
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::notice,
                    "mrutils::curl attempt to call getURL on empty URL");
			#endif
            return __LINE__;
        }

        if (!mrutils::startsWithI(request->url.c_str(),"http://"))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl can't handle non-http url: ",
                    request->url);
			#endif
            return __LINE__;
        }

        if (redirCount > 5)
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::error,
                    "mrutils::curl too many redirects (%d) for url: %s",
                    redirCount, request->url.c_str());
			#endif
            return __LINE__;
        }

        static const char * mobileAgent =
            "SonyEricssonT610/R501 Profile/MIDP-1.0 Configuration/CLDC-1.0";
            //"Mozilla/5.0 (BlackBerry; U; BlackBerry AAAA; en-US) AppleWebKit/534.11+ (KHTML, like Gecko) Version/X.X.X.X Mobile Safari/534.11+";
        static const char * nativeAgent =
            "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_4; en-us) "
            "AppleWebKit/534.11+ (KHTML, like Gecko) "
            "Version/5.0.2 Safari/533.18.5";
        int len;

        std::replace(request->url.begin(), request->url.end(), ' ', '+');
        if (request->url.size() > sizeof(buffer))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl URL is too long: ",
                    request->url);
			#endif
            return __LINE__;
        }
        strcpy(buffer,request->url.c_str());

		#ifdef HAVE_PANTHEIOS
        pantheios::log(pantheios::informational,
                "mrutils::curl request: ",
                request->url);
		#endif

        char *host = buffer + strlen("http://");
        char *file = strchr(host,'/');
        int port = 80;

        if (!file)
            file = (char*)"";
        else
            *file++ = '\0';

        // check for port
        if (char * portStr = strchr(host,':'))
        {
            port = atoi(portStr+1);
            *portStr = '\0';
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
        ,request->mobile ? mobileAgent : nativeAgent
        ,request->xmlOk ? "text/xml," : ""
        ,request->referer
    );

        mrutils::Socket sock(host,port);
        if (!sock.initClient(5,stopFD))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl can't connect to: ",
                    host);
			#endif
            return -1;
        }

        if ((int)ss.str().size() != sock.write(ss.c_str(), ss.str().size()))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl got disonnected sending request to:",
                    host);
			#endif
            return -1;
        }

        mrutils::BufferedReader reader("",data == NULL ? 32768 : data->buffSz-1,
            data == NULL ? NULL : data->buffer);
        reader.setSocketInterruptFD(stopFD);
        reader.setSocketWaitTime(5);
        reader.use(sock);

        headerData_t header;
        int ret = readHeader(reader,request->url.c_str(),&header);
        if (ret == -1)
            return -1;
        if (ret == 0 && !header.redirected)
            return -1;

        if (header.redirected)
        {
            // move host off the stack buffer onto the reader's buffer
            memcpy(reader.buffer, buffer, sizeof(buffer));
            host = reader.buffer + (host - buffer);

            if (header.location[0] == '/') // relative URL
            {
                snprintf(buffer, sizeof(buffer),
                    "http://%s:%d%s",host,port,header.location.c_str());
                request->url = buffer;
            } else // absolute URL
            {
                strncpy(buffer, header.location.c_str(), sizeof(buffer));
                buffer[sizeof(buffer)-1] = '\0';
                request->url = buffer;
            }

            return getURL(request,data,dataM,stopFD,++redirCount);
        }

        if (header.chunked)
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl sent HTTP/1.0 protocol, got chunked data back from host: ",
                    host);
			#endif
            return -1;
        }

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
                    if (r < 0)
                    {
						#ifdef HAVE_PANTHEIOS
                        pantheios::log(pantheios::error,
                                "mrutils::curl error gunzipping file from host: ",
                                host);
						#endif
                        return -1;
                    }
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
						#ifdef HAVE_PANTHEIOS
                        pantheios::logprintf(pantheios::error,
                                "mrutils::curl %s:%d error getting data for %s",
                                __FILE__,__LINE__,request->url.c_str());
						#endif
                        return -1;
                    } *(data->eob = data->buffer + len) = '\0';
                } else {
                    for (int l = reader.unreadOnBuffer();0 < (l = reader.read(l))
                        ;dataM->content.write(reader.line,l),l = reader.bufSize) ;
                }
            } else {
                if (data != NULL) {
                    // retrieve partial if too big
                    if (header.length > reader.bufSize)
                    {
						#ifdef HAVE_PANTHEIOS
                        pantheios::logprintf(pantheios::warning,
                                "mrutils::curl %s:%d page at %s is bigger than buffer (%d)",
                                __FILE__,__LINE__,request->url.c_str(),
                                reader.bufSize);
						#endif
                        header.length = reader.bufSize;
                    }

                    reader.read(0);
                    reader.switchBuffer(data->buffer);
                    if (header.length != reader.read(header.length))
                    {
						#ifdef HAVE_PANTHEIOS
                        pantheios::logprintf(pantheios::error,
                                "mrutils::curl %s:%d error getting data for %s",
                                __FILE__,__LINE__,request->url.c_str());
						#endif
                        return -1;
                    } *(data->eob = data->buffer + header.length) = '\0';
                } else {
                    for (int l = reader.unreadOnBuffer();header.length > 0;header.length -= l, l = reader.bufSize) {
                        if (header.length < l) l = header.length;
                        if (l != reader.read(l))
                        {
							#ifdef HAVE_PANTHEIOS
                            pantheios::logprintf(pantheios::error,
                                "mrutils::curl %s:%d got disconnected reading from host: %s",
                                __FILE__, __LINE__, host);
							#endif
                            return -1;
                        } dataM->content.write(reader.line,l);
                    }

                }
            }
        }
    
        return 0;
    }

    struct redirect_t { 
        const mrutils::curl::urlRequestM_t * request;
        std::string location;
    };

}

int mrutils::curl::getURL(urlRequest_t *request, curlDataM_t& dataM, int stopFD) {
    return ::getURL(request,NULL,&dataM,stopFD,0);
}

int mrutils::curl::getURL(urlRequest_t *request, curlData_t& data, int stopFD) {
    return ::getURL(request,&data,NULL,stopFD,0);
}

int mrutils::curl::getMultiple(std::set<urlRequestM_t> const &requests_,
        std::vector<curlDataM_t> *data, int stopFD)
{
    if (requests_.empty())
    {
		#ifdef HAVE_PANTHEIOS
        pantheios::log(pantheios::warning,
            __PRETTY_FUNCTION__, " call with empty request");
		#endif
        return 0;
    }

    int hits = 0;

    mrutils::BufferedReader reader; 
    reader.setSocketInterruptFD(stopFD);
    reader.setSocketWaitTime(10); 
    mrutils::stringstream ss;
    std::string scratchBuffer;

    std::vector<urlRequestM_t const*> requests;
    requests.resize(requests_.size());

    if (data->size() != requests_.size())
        data->resize(requests_.size());

    // set the id of each request
    {   int i = 0; 
        for (std::set<urlRequestM_t>::const_iterator it = requests_.begin();
                it != requests_.end(); ++it, ++i)
        {
            (*it).id = i;
            requests[i] = &*it;
        }
    }

    std::vector<redirect_t> redirects;
    redirects.reserve(requests.size());

    for (std::vector<urlRequestM_t const*>::iterator start = requests.begin(), end = start;
            start != requests.end(); start = end)
    {
        if (!mrutils::startsWithI((*start)->url.c_str(),"http://"))
        {
            ++end;
            continue;
        }

        char const *p, *q;

        std::string host;
        p = (*start)->url.c_str() + ctlen("http://");
        if (NULL == (q = strchr(p,'/')))
            host = p;
        else
            host.assign(p,q-p);


		#ifdef HAVE_PANTHEIOS
        pantheios::log(pantheios::informational,
                "mrutils::curl ",__PRETTY_FUNCTION__," request: ",
                host,"/...");
		#endif

        for (; end != requests.end(); ++end)
        {
            if (!mrutils::startsWithI((*end)->url.c_str(),"http://"))
                break;

            p = (*end)->url.c_str() + ctlen("http://");

            if (NULL == (q = strchr(p,'/')))
            {
                if (0 != strcmp(host.c_str(),p))
                    break;
            } else if ((int)host.size() != q-p || 0!=strncmp(host.c_str(),p,host.size()))
            {
                break;
            }
        }

        // build request
        ss.clear();
        for (std::vector<const urlRequestM_t*>::iterator it = start, last = end-1;
                it != end; ++it)
        {
            char const *file = (*it)->url.c_str() + ctlen("http://") + host.size();

            if (*file == '/')
                ++file;

            oprintf(ss,
                    "GET /%s HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "Accept: text/xml, text/html;q=0.9, image/jpg;q=0.5, image/jpeg;q=0.5, image/png;q=0.5, image/gif;q=0.5\r\n"
                    "Accept-Language: en, fr;q=0.50\r\n"
#               ifdef OK_OUTSIDE_LIBS
                    "Accept-Encoding: gzip,identity\r\n"
#               else
                    "Accept-Encoding: identity\r\n"
#               endif
                    "Accept-Charset: ISO-8859-1, utf-8;q=0.66, *;q=0.66\r\n"
                    "Connection: %s\r\n"
                    "\r\n",
                    file,
                    host.c_str(),
                    it == last ? "close" : "keepalive");
        }


        // connect to host & send requests
        mrutils::Socket sock(host.c_str(),80);

        if (!sock.initClient(5,stopFD))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::error,
                    "mrutils::curl ",__PRETTY_FUNCTION__,
                    " can't connect to host: ", host);
			#endif
            continue;
        }

        std::string const requestStr = ss.str();
        if (sock.write(requestStr.c_str(), requestStr.size())
                != (int)requestStr.size())
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::error,
                "mrutils::curl %s:%d got disconnected reading from host: %s",
                __FILE__, __LINE__, host.c_str());
			#endif
            continue;
        }

        reader.use(sock);

        // for chunked data
        headerData_t header;
        getChunkData_t chunkData(sock,stopFD);

        // parse each result
        for (std::vector<const urlRequestM_t*>::iterator it = start,
                    last = end-1; it != end; ++it)
        {
            header.reset();

            const int ret = readHeader(reader,(*it)->url.c_str(),&header);

            if (ret == -1)
            { // server didn't send response for this one...
                end = it; // try it again without grouping
                if (end == start)
                    ++end;
                break;
            }

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
						#ifdef HAVE_PANTHEIOS
                        pantheios::log(pantheios::error,
                            "mrutils::curl chunked data error: "
                            "line isn't 0 at ", reader.line, " url: ",
                            (*it)->url);
						#endif
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
						#ifdef HAVE_PANTHEIOS
                        pantheios::logprintf(pantheios::error,
                            "mrutils::curl %s:%d got disconnected reading from host: %s",
                            __FILE__, __LINE__, host.c_str());
						#endif
                        break;
                    }
                } else {
                    bool good = true;

                    for (int l = reader.unreadOnBuffer();header.length > 0;header.length -= l, l = reader.bufSize) {
                        if (header.length < l) l = header.length;
                        if (l != reader.read(l)) {
							#ifdef HAVE_PANTHEIOS
                            pantheios::logprintf(pantheios::error,
                                "mrutils::curl %s:%d got disconnected reading from host: %s",
                                __FILE__, __LINE__, host.c_str());
							#endif
                            good = false;
                            break;
                        }

                        data->at((*it)->id).content.write(reader.line,l);
                    }

                    if (!good)
                        break;
                }
            }

            if (!skipContent)
                ++hits;
        }
    }

    // handle redirects
    for (std::vector<redirect_t>::iterator it = redirects.begin();
            it != redirects.end(); ++it)
    {
        urlRequest_t newRequest(it->location);
        if (0 != ::getURL(&newRequest,
                NULL, &data->at((*it).request->id),stopFD,0))
            ++hits;
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
