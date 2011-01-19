#include <ncurses.h>
#undef getch
#undef vline
#undef hline
//#undef LINES
//#undef COLS

#include "mr_guiimgscroll.h"
#include "mr_gui.h"
#include "mr_numerical.h"
#include "mr_files.h"
#include "mr_guifilesave.h"

#define CSI "\x1b""["
#define DOWN(n) fwrite(CSI #n "S",1,ctlen(CSI #n "S"),stdout)
#define UP(n)   fwrite(CSI #n "T",1,ctlen(CSI #n "T"),stdout)
#define MOVEDN(n) fwrite(CSI #n "B",1,ctlen(CSI #n "B"),stdout)
#define MOVEUP(n) fwrite(CSI #n "A",1,ctlen(CSI #n "B"),stdout)
#define WRITE(n) fwrite(n,1,ctlen(n),stdout)

const int  mrutils::GuiImgScroll::StringAllocator::bufSz = 32768;

namespace {
    static inline void scrollUp(int rows) { printf(CSI "%dT", rows); fflush(stdout); }
    static inline void scrollDown(int rows) { printf(CSI "%dS", rows); fflush(stdout); }

    static inline void moveUp(int rows) { printf(CSI "%dA", rows); }
    static inline void moveDn(int rows) { printf(CSI "%dB", rows); }
    static inline void moveLeft(int rows) { printf(CSI "%dD", rows); }
    static inline void moveRight(int rows) { printf(CSI "%dC", rows); }
    static inline void moveTo(int row)  { printf(CSI "%dH", row+1); }
    static inline void moveToCol(int col) { printf(CSI "%dG", col+1); }

    static inline void setBg(int color) { printf(CSI "%dm", 40+color); }
    static inline void setBg256(int color) { printf(CSI "48;5;%dm", color); }
    static inline void resetColor() { fputs(CSI "0m",stdout); }

    static inline void clearToBottom() { fputs(CSI "J",stdout); }

    static inline void printEndLine() {
        setBg(1); fputs(CSI "K",stdout); resetColor();
    }

    static inline void highlightStart() { WRITE(CSI "48;5;11m" CSI "30m"); }
    static inline void highlightEnd() { resetColor(); }

    static inline void printSearch(const char * p, const char * const search, const size_t searchLen) {
        for (const char * q = p; NULL != (q = mrutils::stristr(q,search));p = q = q + searchLen) {
            fwrite(p,1,q-p,stdout);
            highlightStart();
            fwrite(q,1,searchLen,stdout);
            highlightEnd();
        } fputs(p,stdout);
    }

    static const int C_PREV    = 16;
    static const int C_NEXT    = 14;
    static const int C_H       = 8; 
    static const int C_DEL     = 127; 
    static const int C_CLLINE  = 21;   // <c-u>
    static const int C_DELWRD  = -120; // <m-backspace>
    static const int M_D       = -28;  // <m-d>
    static const int C_A       = 1;
    static const int C_E       = 5;
    static const int C_K       = 11;
    static const int C_F       = 6;
    static const int C_B       = 2;
    static const int M_F       = -26; // forward word
    static const int M_B       = -30; // back word
    static const int C_D       = 4; // delete character
    static const int C_W       = 23; // delete word

    static const int scrollWheelAmt = 
    #ifdef __APPLE_
        1;
    #else
        3;
    #endif


}

void mrutils::GuiImgScroll::initDisplay() {
    int i = 1;
    if (bottomLine != lines.end()) { drawLine(bottomLine++); } else i = 0;
    for (;i < term.winRows; ++i) { 
        if (bottomLine != lines.end()) {
            fputs("\r\n",stdout); drawLine(bottomLine); ++bottomLine;
        } else {
            ++gutter; fputs("\r\n",stdout); printEndLine(); ++i;
            for (;i < term.winRows; ++i) fputs("\r\n",stdout);
        }
    }
    fflush(stdout);
}

void mrutils::GuiImgScroll::display(const char * const name, const char * const saveDir, const char * const homeDir) {
    mrutils::gui::init();
    mrutils::gui::forceClearScreen();
    mrutils::mutexAcquire(gui::mutex);
        mousemask(BUTTON1_PRESSED, NULL);
        resetColor();
        moveTo(0);

        //printf("\e[?25l");

        topLine = lines.begin();
        gutter = 0;
        keyNum = 0;

        // find first line to draw -- first match
        if (!searchStr.empty()) {
            for (;topLine != lines.end();++topLine) if (topLine->matchesSearch(searchStr.c_str())) break;
            if (topLine == lines.end()) topLine = lines.begin();
        }
        bottomLine = topLine;
        initDisplay();

        mrutils::GuiFileSave fs(saveDir, homeDir);

        for (;;) {
            const char c = mrutils::SignalHandler::getch();
            if (c == 'q' || c == ';') break;
            ++keyNum;

            switch (c) {
                case 0x1b: // esc
                    if (0x5b == mrutils::SignalHandler::getch()
                        && 0x4d == mrutils::SignalHandler::getch()) {
                        const char c2 = mrutils::SignalHandler::getch();
                        const int x = mrutils::SignalHandler::getch() - 0x21;
                        const int y = mrutils::SignalHandler::getch() - 0x21;
                        switch (c2) {
                            case 0x61: // scroll down
                                scrollDown(scrollWheelAmt);
                                break;
                            case 0x60: // scroll up
                                scrollUp(scrollWheelAmt);
                                break;
                            case 0x20: // left down
                                break;
                            case 0x22: // right down
                                break;
                            case 0x23: // button up
                                break;
                        }
                    }
                    break;
                case 'j':
                    scrollDown(3);
                    break;
                case 'k':
                    scrollUp(3);
                    break;
                case 'n':
                case 'd':
                case ' ':
                    scrollDown(term.winRows);
                    break;
                case 'p':
                case 'u':
                    scrollUp(term.winRows);
                    break;

                case 'N':
                    scrollToNextSearch();
                    break;
                case 'P':
                    scrollToPrevSearch();
                    break;

                case ',':
                    if (!searchStr.empty()) { searchStr.clear(); updateSearch(); }
                    break;

                case '#':
                case '/': 
                    enterSearch();
                    break;
                    

                case 'g':
                case C_A:
                    scrollUp(topLine - lines.begin());
                    break;
                case 'G':
                case C_E:
                    if (bottomLine != lines.end()) {
                        scrollDown(lines.size() + 1 - term.winRows - (topLine - lines.begin()));
                    }
                    break;
                case 's':
                    mrutils::mutexRelease(gui::mutex);
                        if (fs.save(name)) {
                            if (FILE * fp = fopen(fs.path,"w")) {
                                for (std::deque<line_t>::iterator it = lines.begin(); it != lines.end(); ++it) {
                                    if (it->img != NULL && *it->prefix == '\0' && *it->suffix == '\0') continue;
                                    fputs(it->prefix,fp); fputs(it->suffix,fp); fputc('\n',fp);
                                }
                                fclose(fp);
                            }
                        }
                    mrutils::mutexAcquire(gui::mutex);

                    redraw();
                    break;
                default:
                {
                    mrutils::map<char,callFunc>::iterator it = callFns.find(c);
                    if (it != callFns.end()) { 
                        mrutils::mutexRelease(gui::mutex);
                        const bool exit = !(*it)->second();
                        mrutils::mutexAcquire(gui::mutex);
                        if (exit) goto done;
                    }
                } break;
                    
            }
        }
done:
    mrutils::mutexRelease(gui::mutex);
    mousemask(0, NULL);
}

void mrutils::GuiImgScroll::redraw() {
    moveTo(0); clearToBottom();
    std::deque<line_t>::iterator it = topLine;
    if (it != lines.end()) {
        drawLine(it); ++it;
        for (;it != bottomLine; ++it) { fputs("\r\n",stdout); drawLine(it); }
    }
    if (gutter > 0) { fputs("\r\n",stdout); printEndLine(); }
    fflush(stdout);
}

void mrutils::GuiImgScroll::drawLineSearch(std::deque<line_t>::iterator line) {
    printSearch(line->prefix,searchStr.c_str(),searchStr.size());
    printSearch(line->suffix,searchStr.c_str(),searchStr.size());
}

void mrutils::GuiImgScroll::drawImgLine(std::deque<line_t>::iterator line) {
    imgdata_t & data = *line->img;
    data.keyNum = keyNum;

    fputs(line->prefix,stdout);
    moveRight(CEIL((double)data.img->width/term.pxCol));
    fputs(line->suffix,stdout);
    moveToCol(20);
    fflush(stdout);

    int w = data.img->width, h = data.img->height;
    const int maxWidth = term.pxWidth - term.paddingLeft - term.paddingRight - data.x;
    if (w > maxWidth) { double scale = (double)maxWidth / w; w *= scale; h *= scale; }

    term.showImage(*data.img, data.x, term.paddingTop + term.pxLine * (data.startLine - (topLine-lines.begin())),w,h);

    // fill the padding
    term.drawRectangle(0,0,0,term.pxWidth,term.paddingTop);
    term.drawRectangle(0,0,term.pxHeight - term.paddingBottom,term.pxWidth,term.paddingBottom);
    term.drawRectangle(0,term.pxWidth-term.paddingRight,0,term.paddingRight,term.pxHeight);
}

void mrutils::GuiImgScroll::addImg(int id, const char * prefix, const int prefixLen, const char * suffix, const int suffixLen) {
    if (!term.good) {
        // slow.. just allocate a string then reallocate in the buffer
        const int totalLength = prefixLen + suffixLen + ctlen(" [IMG] ");
        std::string str; str.reserve(totalLength);
        str.assign(prefix,prefixLen);
        str.append(" [IMG] ",ctlen(" [IMG] "));
        str.append(suffix,suffixLen);
        return addLine(str.c_str(), totalLength);
    }

    int w = images[id]->width, h = images[id]->height;
    const int maxWidth = term.pxWidth - term.paddingLeft - term.paddingRight - (term.paddingLeft + prefixLen * term.pxCol);

    if (maxWidth <= 0) {
        addLine(prefix,prefixLen);
    } else {
        int n = imgData.size(); imgData.resize(n+1);
        imgdata_t & data = imgData.back();
        data.img = images[id];
        data.startLine = lines.size();
        data.x = term.paddingLeft + prefixLen * term.pxCol;

        if (w > maxWidth) { double scale = (double)maxWidth / w; w *= scale; h *= scale; }

        const bool wrapSuffix = suffixLen * term.pxCol > maxWidth - w;
        char * prefixAlloc = (char*)memcpy(strings.next(prefixLen),prefix,prefixLen); prefixAlloc[prefixLen] = '\0';
        char * suffixAlloc = (char*)memcpy(strings.next(suffixLen),suffix,suffixLen); suffixAlloc[suffixLen] = '\0';

        if (wrapSuffix) lines.push_back(line_t(&data, prefixAlloc, ""));
        else lines.push_back(line_t(&data, prefixAlloc, suffixAlloc));

        line_t padding(&data);
        for (int i = 1, iE = CEIL((double)h / term.pxLine)
            ; i < iE; ++i) lines.push_back(padding);

        if (wrapSuffix) lines.push_back(line_t(suffixAlloc));
    }
}

mrutils::GuiImgScroll::~GuiImgScroll() {
    for (std::vector<mrutils::ImageDecode*>::iterator it = images.begin()
        ;it != images.end(); ++it) delete *it;
}

void mrutils::GuiImgScroll::clear(bool inclFns) {
    for (std::vector<mrutils::ImageDecode*>::iterator it = images.begin()
        ;it != images.end(); ++it) delete *it;
    images.clear();
    imgData.clear();
    strings.clear();
    lines.clear();
    keyNum = 0;
    if (inclFns) callFns.clear();
}

void mrutils::GuiImgScroll::scrollToNextSearch() {
    if (topLine == lines.end()) return;

    std::deque<line_t>::iterator line = topLine + 1;
    const char * const search = searchStr.c_str();
    for (;line != lines.end(); ++line) if (line->matchesSearch(search)) break;
    scrollDown(line - topLine);
}

void mrutils::GuiImgScroll::enterSearch() {
    // get first char
    char c = mrutils::SignalHandler::getch();
    if (c == '/' || c == 10 || c == '\0' || c == 13) return;
    searchStr.assign(1,c);
    updateSearch();

    for (;;) {
        c = mrutils::SignalHandler::getch();
        switch (c) {
            case '/':
            case 10:
            case 13:
            case '\0':
                if (topLine == lines.end()) scrollUp(lines.size());
                return;
            case C_H:
                if (searchStr.empty()) continue;
                searchStr.resize(searchStr.size()-1);
                break;
            default:
                if (c < ' ' || c > '~') continue;
                searchStr.append(1,c);
                break;
        }
        updateSearch();
    }
}

void mrutils::GuiImgScroll::updateSearch() {
    if (searchStr.empty()) { 
        if (topLine == lines.end()) scrollUp(lines.size());
        else redraw(); 
        return; 
    }

    if (topLine == lines.end()) scrollUp(lines.size());

    const char * search = searchStr.c_str();

    std::deque<line_t>::iterator line = topLine;
    for (;line != lines.end();++line) if (line->matchesSearch(search)) break;
    if (line == lines.end() || (bottomLine != lines.end() && line - bottomLine > 0)) {
        scrollDown(line - topLine,true); return;
    }

    const int down = line - topLine;
    const int bottomDown = lines.end() - bottomLine;

    if (down > bottomDown) {
        bottomLine = lines.end();
        gutter += down - bottomDown;
    } else {
        bottomLine += down;
    }

    topLine = line;
    redraw();
}

void mrutils::GuiImgScroll::scrollToPrevSearch() {
    if (topLine == lines.begin()) return;

    std::deque<line_t>::iterator line = topLine - 1;
    const char * const search = searchStr.c_str();
    for (;line != lines.begin(); --line) if (line->matchesSearch(search)) break;
    scrollUp(topLine - line);
}

void mrutils::GuiImgScroll::scrollDown(int n, bool force) {
    if (bottomLine != lines.end()) {
        ::scrollDown(n);
        topLine += n;
        moveTo(term.winRows - n);
        fflush(stdout);
        mrutils::sleep(1); // wait for text to render on the screen
        drawLine(bottomLine++); 

        for (int i = 1; i < n; ++i) { 
            if (bottomLine != lines.end()) {
                fputs("\r\n",stdout); drawLine(bottomLine); ++bottomLine;
            } else {
                gutter = term.winRows - (bottomLine - topLine);
                if (gutter > 0) { fputs("\r\n",stdout); printEndLine(); ++i; }
                for (; i < n; ++i) fputs("\r\n",stdout);
                break;
            }
        }

        fflush(stdout);
    } else {
        int maxDown = term.winRows - gutter;
        if (maxDown > 0) {
            if (!force) { 
                if (gutter > 0) return;
                n = 1;
            } else if (n > maxDown) n = maxDown;

            ::scrollDown(n);
            gutter += n; topLine += n;
            moveTo(term.winRows-gutter); fflush(stdout);
            printEndLine();
            fflush(stdout);
        }
    }
}

void mrutils::GuiImgScroll::scrollUp(int n) {
    if (topLine != lines.begin()) {
        int maxLines = std::min((ptrdiff_t)n,topLine - lines.begin());
        ::scrollUp(maxLines); topLine -= maxLines;

        if (gutter < maxLines) {
            bottomLine -= maxLines - gutter;
            gutter = 0;
        } else gutter -= maxLines;
        moveTo(0); fflush(stdout);
        mrutils::sleep(1); // wait for text to render on the screen

        if (maxLines > term.winRows) maxLines = term.winRows;

        std::deque<line_t>::iterator it = topLine;
        drawLine(it); ++it;
        for (int i = 1; i < maxLines; ++i, ++it) { fputs("\r\n",stdout); drawLine(it); }
        fflush(stdout);
    } 
}
