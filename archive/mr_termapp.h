#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_TERMAPP_H
#endif

#ifndef _MR_CPPLIB_TERMAPP_H
#define _MR_CPPLIB_TERMAPP_H
#define _MR_CPPLIB_TERMAPP_H_INCLUDE

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
#include "mr_numerical.h"
#include "mr_strutils.h"
#include <set>

namespace mrutils {

/**
 * The usage is as follows:
 * call next(), pipe in the string of data for the choice 
 * call next(), more data
 * then call get() to get the line number chosen
 * */
class Chooser {
    static const int COL_BLANK;
    static const int COL_SELECTED;
    static const int COL_INPUT;
    static const int COL_TARGETED;

    static const int ATR_BLANK;
    static const int ATR_SELECTED;
    static const int ATR_INPUT;
    static const int ATR_TARGETED;

    public:
        Chooser(int startIndex = 0)
        : building(-1), done_(false), selectNumber(0)
          ,init_(false),createdWin(false)
          ,enteringSearch(false), applySearch(false)
          ,head(-1),tail(-1),headMatches(0),selMatches(0)
          ,inputP(userInput), startIndex(startIndex)
        { userInput[0] = '\0'; lastCommand[0] = '\0'; }

        ~Chooser() { done(true); }

    public:

        inline void redraw() { select(selected()); }
        void clear();
        void* getWindow() { return chooserWin; }
        inline int height() { return lines; }
        inline int width() { return cols; }

        inline void setName(const char * name) {
            this->name = name;
            if (init_) resetPrompt();
        }

        inline std::set<int>& getTargets() { return targets; }

        inline int selected() {
            if ( selMatches < 0 ) return -1;
            if ( selMatches >= searchMatches.size() ) return -1;
            return searchMatches[selMatches] + startIndex;
        }

        inline void resetLine(int line, const char * str) {
            choiceBuilder.width(3);
            choiceBuilder.setf( std::ios::left );
            choiceBuilder << line+startIndex << ":  ";
            choiceBuilder << str;
            choices[line] = choiceBuilder.str();
            choiceBuilder.str("");
        }

        inline Chooser& next() {
            if (building >= 0) build();
            building = choices.size();
            choiceBuilder.width(3);
            choiceBuilder.setf( std::ios::left );
            choiceBuilder << (building+startIndex) << ":  ";
            return *this;
        }

        template <class T>
        inline Chooser& operator << (T& d) {
            choiceBuilder << d;
            return *this;
        }

        int get(bool clear = true);
        char userInput[64];

        inline void select(int hid) {
            if (!init_ && !(init_ = init())) {
                userInput[0] = '\0';
                return;
            }
            
            if (building >= 0) build();
            highlight(hid - startIndex);
        }


    private:
        bool init();
        void done(bool destroy = false);

        inline void build() {
            choices.push_back(choiceBuilder.str());
            targeted.push_back(false);
            choiceBuilder.str("");
            building = -1;
        }

        inline int numVisible() {
            return MAX(0u,MIN(lines-1,(unsigned)searchMatches.size() - headMatches));
        }
        int mouse();

        inline void pageUp() {
            highlight( 0 );
        }
        inline void pageDn() {
            highlight( MAX(0u,(unsigned)choices.size() - 1) );
        }
        inline void highlightPrev(bool refreshInput=true) {
            if (selMatches == -1) selMatches = headMatches;
            if (selMatches == 0) { resetPrompt(); return; }

            highlight(searchMatches[--selMatches],refreshInput);
        }
        inline void highlightNext(bool refreshInput = true) {
            if (selMatches == -1) selMatches = headMatches;

            if (selMatches+1  >= searchMatches.size()) {
                if (head == -1) { resetPrompt(); return; }
                if (searchMatches[selMatches]+1 == choices.size()) { resetPrompt(); return; }
                if (searchMatches.size() - headMatches < lines-1) { resetPrompt(); return; }
                highlight(searchMatches[selMatches]+1,refreshInput);
            } else {
                highlight(searchMatches[++selMatches],refreshInput);
            }
        }

        inline void resetSearch() {
            head = tail = -1; searchMatches.clear();
            headMatches = selMatches = 0;
            highlight( 0 );
        }

        void highlight(int hid, bool refreshInput = true, bool selectFirst = true);
        void target(int hid);

        inline void resetPrompt() { printPrompt(name.c_str()); }
        void printPrompt(const char * prompt, const char * prefix = NULL);


    private:
        int startIndex;

        void * chooserWin;
        bool createdWin;

        int building;
        mrutils::stringstream choiceBuilder;
        char lastCommand[64];

        std::vector<std::string> choices;
        char *inputP;
        bool done_, init_;

        std::string name;
        unsigned lines, cols;

        bool enteringSearch, applySearch;
        int head, tail; // entry ids for first visible & last visible
        std::vector<int> searchMatches; // built as you scroll the list
        int headMatches, selMatches;

        int selectNumber;

        std::vector<bool> targeted;
        std::set<int> targets;
};
}

#endif
