#include "mr_termline.h"
#ifdef _MR_CPPLIB_TERMLINE_H_INCLUDE

#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

using namespace mrutils;

class TermLine::CursesTerm : public mrutils::BufferedTerm {
    friend class TermLine;

    public:
        CursesTerm(WINDOW* win, int winCols)
        : win(win), winCols(winCols), maxCursor(winCols-1)
         ,cursor(0), cursorOld(0), y(0)
         ,frozen(false), hideCursor(false), bold(false)
        { }

    protected:
        inline void fputs(const char * str, _UNUSED FILE* ignored) {
            int n = maxCursor - cursor;
            if (bold) wattron(win,A_BOLD);
            waddnstr(win, str, n);
            cursor += std::min((int)strlen(str),n);
        }
        virtual inline void fflush(_UNUSED FILE* ignored) {
            mrutils::mutexAcquire(gui::mutex);

            mvwchgat(win, 0, cursorOld, 1, (bold?A_BOLD:0), gui::COL_INPUT, NULL);

            if (!hideCursor) 
                mvwchgat(win, 0, cursor, 1, (bold?A_BOLD:0), gui::COL_CURSOR, NULL);

            cursorOld = cursor;

            if (!frozen) wrefresh(win);
            mrutils::mutexRelease(gui::mutex);
        }
        virtual inline void fputc(char c, _UNUSED FILE* ignored) {
            if (c == '\n') {
                wmove(win, 0, 0);
                wclrtoeol(win);
                wchgat(win, winCols, 0, gui::COL_INPUT, NULL);
                cursor = 0; cursorOld = 0;
            } else {
                if (bold) wattron(win,A_BOLD);
                waddnstr(win, &c, 1);
                ++cursor;
            }
        }
        virtual inline void moveCursor(int amt) {
            cursor += amt;
            wmove(win, 0, cursor);
        }
        virtual inline void clearToEOL() {
            wclrtoeol(win);
            wchgat(win, winCols, 0, gui::COL_INPUT, NULL);
        }

        inline void setBold(bool tf) {
            this->bold = tf;
            //if (tf) { wattron(win,A_BOLD); }
            //else { wattroff(win,A_BOLD); }
        }
    private:
        WINDOW* win; int winCols, maxCursor;
        int cursor, cursorOld; int y;
        public: bool frozen; private:
        bool hideCursor, bold;
};

TermLine::TermLine(int y0, int x0, int rows, int cols,bool termOnTop) 
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : LINES + rows))
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,win((void*)newwin(1,winCols,(termOnTop?0:y0 + winRows - 1),x0))
    ,cursesTerm(new CursesTerm((WINDOW*)win, winCols))
    ,line(cursesTerm->line), eob(cursesTerm->eob)
    {
        cursesTerm->setMaxLineLength(winCols);
    }

TermLine::~TermLine() {
    delete cursesTerm;
    if (!init_) return;

    delwin((WINDOW*)win);
}

void TermLine::show() {
    if (init_) return;

    // hide cursor -- will emulate 
    curs_set(0);
    leaveok(stdscr,true);
    leaveok((WINDOW*)win,true);

    wmove((WINDOW*)win, 0,0); 
    wchgat((WINDOW*)win, winCols, (cursesTerm->bold?A_BOLD:0), gui::COL_INPUT, NULL);
    wattrset((WINDOW*)win, gui::ATR_INPUT);

    if (!cursesTerm->frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }

    init_ = true;
}

void TermLine::freeze() {
    cursesTerm->frozen = true;
}

void TermLine::thaw() {
    show();
    cursesTerm->frozen = false;

    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

void TermLine::setPrompt(const char * prompt) {
    cursesTerm->setPrompt(prompt);
}

void TermLine::assignFunction(char c, BufferedTerm::callFunc callFn) {
    cursesTerm->assignFunction(c, callFn);
}
void TermLine::appendText(const char * text) {
    cursesTerm->appendText(text);
}
void TermLine::assignText(const char * text) {
    cursesTerm->assignText(text);
}
void TermLine::assignSearch(char c, BufferedTerm::liveFunc liveFn
   ,BufferedTerm::liveEndFunc endFn, char endChar, bool skipChar,bool checkFns ) { 
    cursesTerm->assignSearch(c, liveFn, endFn, endChar, skipChar,checkFns);
}

void TermLine::setObtainMutex(bool tf) {
    cursesTerm->obtainMutex = tf;
}

void TermLine::setHideCursor(bool tf) {
    cursesTerm->hideCursor = tf;
}

void TermLine::setBold(bool tf) {
    cursesTerm->setBold(tf);
}


bool TermLine::nextLine(const char * passChars) {
    if (!init_) show();
    return cursesTerm->nextLine(passChars);
}

void TermLine::setStatus(const char * status) {
    cursesTerm->setStatus(status);
}
void TermLine::lockStatus(const char * status) {
    cursesTerm->lockStatus(status);
}
void TermLine::unlockStatus() {
    cursesTerm->unlockStatus();
}

#endif
