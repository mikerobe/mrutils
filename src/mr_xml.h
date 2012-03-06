#ifndef _MR_CPPLIB_XMLPARSER_H
#define _MR_CPPLIB_XMLPARSER_H

#include "mr_exports.h"
#include <stdio.h>
#include <vector>
#include "fastdelegate.h"
#include "mr_strutils.h"
#include "mr_threads.h"
#include "mr_curl.h"

namespace mrutils {

class _API_ XML {
    public:
        XML()
        : buffer(new char[buffSz])
        { tagPtr = eob = sob = buffer; }

        ~XML() { delete[] buffer; }

    public:
        int get(mrutils::curl::urlRequest_t *request,
                int stopFD = -1, bool checkRedir = true);

        /**
         * Reads a local file as if it were coming from the web
         */
        void readFile(char const *path);

       /**
        * Replace &apos; with ', etc. returns a pointer to the end
        */
        static char * unescapeHTML(char * start, char * end);

        int tagCount(const char * tagName) {
            std::string str = "<"; str.append(tagName);
            int count = 0;
            for (char * ptr = buffer; NULL != (ptr = mrutils::stristr(ptr,str.c_str())); ++ptr) {
                const char c = ptr[str.size()];
                if (c == ' ' || c == '>' || c == '\t' || c == ';' || c == '\0') ++count;
            }
            return count;
        }

        // strategy: search backward for first <xxx (not </xxx) that doesn't end in />
        // set tagPtr to <xxx and return xxx
        char* prevTag() {
            tagText[0] = 0; if (tagPtr == eob) --tagPtr;

            if (*tagPtr == '<') {
                if (tagPtr == buffer) return tagText;
                --tagPtr;
            } 

            for (;;--tagPtr) {
                tagPtr = mrutils::strchrRev(tagPtr,'<',buffer);
                if (tagPtr == NULL) { tagPtr = buffer; return tagText; }
                if (*(tagPtr+1) == '/') continue;
                char * p = strchr(tagPtr,'>');
                if (p == NULL) return tagText;
                if (*(p-1) == '/') continue;
                *strncpy(tagText,tagPtr+1,std::min((size_t)127,strcspn(tagPtr+1,"> "))) = 0;
                return tagText;
            }
        }

        char * nextTag();

        bool next(const char * tagName);

        bool prev(const char * tagName) {
            if (tagPtr == buffer) return false;

            char start[128];
            *start = '<'; *mrutils::strncpy(start+1,tagName,126) = 0;

            for (;;--tagPtr) {
                tagPtr = mrutils::stristrRev(tagPtr,start,buffer);
                if (tagPtr == NULL) { tagPtr = buffer; return false; }
                if (*(tagPtr+1) == '/') continue;
                char * p = strchr(tagPtr,'>');
                if (p == NULL) return false;
                if (*(p-1) == '/') continue;
                return true;
            }
        }

       /**
        * Returns a pointer to the END of the field
        */
        const char * tag(char * dest, int size) {
            return tagInternal(dest,size,NULL);
        }

        void tag(std::string& str) {
            tagInternal(NULL,0,&str);
        }

        int size() { return (eob - buffer); }

    public:
        char *tagPtr, *eob, *sob;
        std::string m_url;

        static const int buffSz = 1024*1024; // 1MB

    private:
        const char * tagInternal(char * dest, int size, std::string* str);

    private:
        char tagText[128];
        char * buffer;
};

}

#endif
