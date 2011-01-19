#ifndef _MR_CPPLIB_CURL_H
#define _MR_CPPLIB_CURL_H

#include "mr_exports.h"
#include <iostream>
#include "mr_strutils.h"
#include <set>

namespace mrutils {

    namespace curl {

        struct _API_ curlData_t {
            curlData_t()
            : buffSz(0)
             ,buffer(NULL)
             ,eob(NULL)
             ,errBuffer(NULL)
            {}

            int buffSz;
            char * buffer, * eob;
            char * errBuffer;
        };

        struct _API_ urlRequest_t {
            urlRequest_t(const char * url = NULL)
            : url(url)
             ,cookieFile(NULL)
             ,postFields(NULL)
             ,cookieData(NULL)
             ,referer("http://www.google.com")
             ,mobile(false)
             ,xmlOk(true)
            {}

            const char * url, * cookieFile, * postFields, * cookieData, * referer;
            bool mobile, xmlOk;
        };

        struct _API_ curlDataM_t {
            mrutils::stringstream content;
        };

        struct _API_ urlRequestM_t {
            friend int getMultiple(const std::set<urlRequestM_t>& requests, std::vector<curlDataM_t> * data, int stopFD);
            urlRequestM_t() : id(0) {}
            urlRequestM_t(const urlRequestM_t& other) : url(other.url), id(other.id) {}

            std::string url;

            inline bool operator<(const urlRequestM_t& other) const {
                return (0 > strcmp(url.c_str(),other.url.c_str()));
            }

            mutable int id;
        };

       /**
        * Pass the two required data structures, and optionally
        * a stopFD, which will halt the request when there is data
        * to be read in that FD
        *
        * The URL is received in one fell-swoop into the buffer until
        * there's no more data or the buffer is exhausted
        *
        * Returns a string for the URL that is ultimately retrieved
        */
        _API_ std::string getURL(const urlRequest_t& request, curlData_t& data, int stopFD = -1);

        _API_ std::string getURL(const urlRequest_t& request, curlDataM_t& data, int stopFD = -1);

        /**
         * Used for multiple plain-vanilla requests
         * to a single server. Uses HTTP/1.1 pipelining.
         * This does not accomodate redirects -- if there's a redirect,
         * it fails
         * Returns the number of successful (200) requests
         */
        _API_ int getMultiple(const std::set<urlRequestM_t>& requests, std::vector<curlDataM_t> * data, int stopFD = -1);


        /*
        struct _API_ recv_t {
            friend int recv(void * data, char * buffer, int sz);

            public:
                recv_t(const urlRequest_t& request, int stopFD = -1)
                : request(request)
                 ,blockRead(0), thisRead(0), running(0)
                 ,curl(NULL), multi_handle(NULL)
                 ,stopFD(stopFD)
                {}

                ~recv_t() {
                    if (multi_handle != NULL) curl_multi_cleanup(multi_handle);
                    if (curl != NULL) curl_easy_cleanup(curl);
                }

            public:
                const urlRequest_t& request;
                int blockRead, thisRead;
                
                // for writing... set by recv function
                char * buffer; int sz;

            private:
                bool init();

                recv_t(const recv_t& other);
                recv_t& operator=(const recv_t& other);

            private:
                int running;
                CURL * curl;
                CURLM *multi_handle;

                fd_set fdread; fd_set fdwrite; fd_set fdexcep;
                fd_set fdread_; fd_set fdwrite_; fd_set fdexcep_;

                int maxFD, stopFD;
        };

        _API_ int recv(void * data, char * buffer, int sz);
        */
    }
}


#endif
