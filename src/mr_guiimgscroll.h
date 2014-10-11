#ifndef _MR_CPPLIB_GUI_IMG_SCROLL_H
#define _MR_CPPLIB_GUI_IMG_SCROLL_H

#include "mr_guiimgterm.h"
#include <algorithm>
#include <deque>
#include "mr_map.h"

namespace mrutils {

    class GuiImgScroll {
	private:
		struct imgdata_t {
			mrutils::ImageDecode* img;
			int keyNum, x, startLine;
			imgdata_t() : keyNum(-1) { }
		};
		
		struct line_t {
			const char * const prefix, * const suffix;
			imgdata_t * const img;
			
			inline line_t(imgdata_t * img)
			: prefix(""), suffix(""), img(img)
			{}
			
			inline line_t(const char * const line)
			: prefix(line), suffix(""), img(NULL)
			{}
			
			inline line_t(imgdata_t* img, const char * const prefix, const char * const suffix)
			: prefix(prefix), suffix(suffix), img(img)
			{}
			
			inline bool matchesSearch(const char * const search) {
				return (mrutils::stristr(prefix,search) || mrutils::stristr(suffix,search));
			}
		};

        public:
            GuiImgScroll()
            : wrapLines(false)
            { searchStr.reserve(128); }

            ~GuiImgScroll();

        public:
            inline void setWrapLines(bool tf = true) { this->wrapLines = tf; }

        public:
            inline void addLine(const char * line, int strlen);
            void addImg(const int imgId, const char * prefix = "", const int prefixLen = 0, const char * suffix = "", const int suffixLen = 0);
            inline int storeImg(mrutils::BufferedReader& reader);

            void display(const char * const name = "",
                char const * const saveDir = "/Users/mikerobe/Desktop/",
                char const * const homeDir = "/Users/mikerobe/"
                );
            void clear(bool inclFns = true);

           /**
            * Letter function calls
            */
            typedef fastdelegate::FastDelegate0<bool> callFunc;
            inline void assignFunction(char c, callFunc callFn) { callFns.insert(c,callFn); }

            /**
             * Extended attributes are added to a file when saved on the
             * mac.
             */
            inline void addXAttribute(char const * name, char const * value)
            { xattributes.push_back(std::pair<std::string, std::string>(name, value)); }

            inline void clearXAttributes()
            { xattributes.clear(); }


        public:
            std::string searchStr;

        private:
            inline void drawLine(std::deque<line_t>::iterator line);
            void drawImgLine(std::deque<line_t>::iterator line);
            void drawLineSearch(std::deque<line_t>::iterator line);
            void scrollDown(int n, bool force = false);
            void scrollUp(int n);
            void redraw();
            void scrollToNextSearch();
            void scrollToPrevSearch();

            void initDisplay();
            void updateSearch();
            void enterSearch();
            void redrawSearch();

        private:
            mrutils::ImgTerm term;
            int keyNum;
            bool wrapLines;

            mrutils::map<char, callFunc> callFns;


            std::list<imgdata_t> imgData;
            std::vector<mrutils::ImageDecode*> images;

            typedef std::vector<std::pair<std::string, std::string> > xattributes_T;
            xattributes_T xattributes;
            

            std::deque<line_t> lines;
            std::deque<line_t>::iterator topLine, bottomLine;
            int gutter;

            struct StringAllocator {
                StringAllocator() 
                : curBuf((char*)malloc(bufSz))
                 ,p(curBuf + sizeof(char*)), eob(curBuf + bufSz)
                { *(char**)curBuf = NULL; }

                ~StringAllocator() {
                    for (;curBuf != NULL;) {
                        char * p = curBuf;
                        curBuf = *(char**)curBuf;
                        free(p);
                    }
                }

                inline void clear() {
                    // clear all but current buffer
                    for (char * buffer = *(char**)curBuf; buffer != NULL;) {
                        char * p = buffer;
                        buffer = *(char**)buffer;
                        free(p);
                    }
                    *(char**)curBuf = NULL;
                    p = curBuf + sizeof(char*);
                    eob = curBuf + bufSz;
                }


                inline char * next(int strlen) {
                    if (++strlen >= eob - p) {
                        char * next = (char*)malloc(std::max(bufSz,strlen+(int)sizeof(char*)));
                        eob = next + bufSz;
                        p = next + sizeof(char*);
                        *(char**)next = curBuf;
                        curBuf = next;
                    }
                    char * ret = p; p += strlen; return ret;
                }

                private:
                    char * curBuf, * p, * eob;
                    static const int bufSz;

            } strings;
    };

}

inline int mrutils::GuiImgScroll::storeImg(mrutils::BufferedReader& reader) {
    mrutils::ImageDecode * img = new mrutils::ImageDecode();
    if (!img->decode(reader)) { delete img; return -1; }
    int n = images.size(); images.push_back(img); return n;
}

inline void mrutils::GuiImgScroll::drawLine(std::deque<line_t>::iterator line) {
    if (line->img == NULL || line->img->keyNum == keyNum) {
        if (!searchStr.empty()) return drawLineSearch(line);
        fputs(line->prefix,stdout);
        fputs(line->suffix,stdout);
    } else return drawImgLine(line);
}

inline void mrutils::GuiImgScroll::addLine(const char * str, int strlen) {
    if (wrapLines && strlen > term.winCols) {
        const int iE = strlen/term.winCols;
        for (int i = 0; i < iE; ++i, str += term.winCols) {
            line_t line(strings.next(term.winCols));
            memcpy(const_cast<char*>(line.prefix),str,term.winCols);
            const_cast<char*>(line.prefix)[term.winCols] = '\0';
            lines.push_back(line);
        }
        if (0 == (strlen -= iE * term.winCols)) return;
    } else if (strlen > term.winCols) strlen = term.winCols;

    line_t line(strings.next(strlen));
    memcpy(const_cast<char*>(line.prefix),str,strlen);
    const_cast<char*>(line.prefix)[strlen] = '\0';
    lines.push_back(line);
}

#endif
