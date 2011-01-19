#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_COL_CHOOSER_H
#endif

#ifndef _MR_CPPLIB_COL_CHOOSER_H
#define _MR_CPPLIB_COL_CHOOSER_H
#define _MR_CPPLIB_COL_CHOOSER_H_INCLUDE

#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include "fastdelegate.h"
#include "mr_threads.h"
#include "mr_numerical.h"

namespace mrutils {

class ColChooser {
    typedef fastdelegate::FastDelegate2<ColChooser&,void*> optionsFunc;
    typedef fastdelegate::FastDelegate2<ColChooser&,void*> changeFunc;
    typedef fastdelegate::FastDelegate2<ColChooser&,void*> openFunc;
    typedef fastdelegate::FastDelegate3<ColChooser&,int,bool> onTargetFunc;

    private:
        class Column {
            public:
                Column(ColChooser& cc)
                : cc(cc), chooserWin(cc.chooserWin), colWidth(cc.cols)
                 ,head(-1), tail(-1), headMatches(0), selMatches(0)
                 ,colNumber(0), applySearch(false)
                {}

                Column& operator=(const Column& other) {
                    // only called once, on instantiation of the vector
                }
                inline int numVisible() {
                    return MAX(0u,MIN(cc.lines-1,(unsigned)searchMatches.size() - headMatches));
                }

                void highlight(int hid, bool refreshInput = true, bool selectFirst = true, bool refreshWindow=true);
                inline void pageUp() {
                    highlight( 0 );
                }
                inline void pageDn() {
                    highlight( MAX(0u,(unsigned)choices.size() - 1) );
                }
                inline void highlightPrev(bool refreshInput=true) {
                    if (selMatches == -1) selMatches = headMatches;
                    if (selMatches == 0) { cc.resetPrompt(); return; }

                    highlight(searchMatches[--selMatches],refreshInput);
                }
                inline void highlightNext(bool refreshInput = true) {
                    if (selMatches == -1) selMatches = headMatches;

                    if (selMatches+1  >= searchMatches.size()) {
                        if (head == -1) { cc.resetPrompt(); return; }
                        if (searchMatches[selMatches]+1 == choices.size()) { cc.resetPrompt(); return; }
                        if (searchMatches.size() - headMatches < cc.lines-1) { cc.resetPrompt(); return; }
                        highlight(searchMatches[selMatches]+1,refreshInput);
                    } else {
                        highlight(searchMatches[++selMatches],refreshInput);
                    }
                }

                inline void resetSearch() {
                    head = tail = -1; searchMatches.clear();
                    headMatches = selMatches = 0;
                }

                int mouse(void* mouseEvent);

                inline void clear() {
                    resetSearch();
                    applySearch = false;
                    colWidth = cc.cols;
                    choices.clear();
                }

            public:
                int colWidth, colNumber;
                std::vector<std::string> choices;

                bool applySearch;
                char search[128];

                int head, tail; // entry ids for first visible & last visible
            private:
                ColChooser& cc;
                void * chooserWin;

                std::vector<int> searchMatches; // built as you scroll the list
                int headMatches, selMatches;
        };

    public:
        ColChooser(int columns = 1
            ,int y0 = 0, int x0 = 0, int rows = 0, int cols = 0);

    public:
       /********************
        * Action functions & init
        ********************/

        inline void setStartIndex(int startIndex) 
        { this->startIndex = startIndex; }

        inline void setOpenFunction(openFunc openFn) 
        { this->openFn = openFn; }

        void setOnTarget(onTargetFunc onTargetFn) 
        { this->onTargetFn = onTargetFn; }

        void setOptionsFunction(optionsFunc optionsFn, void * optionData = NULL)
        { this->optionsFn = optionsFn; this->optionsData = optionsData; }

    public:
       /********************
        * Functions to assign
        ********************/

        void liveSearch(const char * search);
        void liveSearchEnd(const char * search);
        void next();
        void prev();

    public:
       /********************
        * Window, construction, formatting
        ********************/

        int get();
        void clear();
        inline void redrawNextCol() { data[active].highlight(depth[active]); }
        void redraw();

        /**
         * Highlights the given hid in the active column.
         * */
        inline void select(int hid) {
            data[active].highlight(hid);
        }

    public:
       /********************
        * Adding data
        ********************/

        inline void resetLine(int line, const char *str) {
            if (hideNumbers) {
                data[active].choices[line] = choiceBuilder.str();
            } else {
                choiceBuilder << std::left << std::setw(3) << line+startIndex << ":  ";
                choiceBuilder << str;
                data[active].choices[line] = choiceBuilder.str();
                choiceBuilder.str("");
            }

            redraw();
        }

    public:
       /********************
        * Getting data
        ********************/





        void toggleTarget(int hid);
        void setTarget(int hid, bool tf=true);
        void clearTargets();

        inline int activeCol() { return active; }
        void* getWindow() { return chooserWin; }
        inline int height() { return lines; }
        inline int width() { return cols; }
        inline int maxHeight() { return maxLines; }
        inline std::set<int>& getTargets() { return targets; }
        inline int selected() { return depth[active]; }
        inline std::vector<int>& history() { return depth; }

        inline char * input() { return data[active].search; }

        inline void onChange(changeFunc changeFn, void * changeData) {
            this->changeFn = changeFn; 
            this->changeData = changeData;
        }

        inline void persist() { acceptReturn = false; }
        inline void setNoMouse() { enableMouse = false; }
        inline void setHideNumbers(bool tf=true) { hideNumbers = tf; }
        inline void setVisible(bool tf) { 
            visible = tf; 
        }
        inline bool getVisible() { return visible; }
        inline void setName(const char * name) {
            this->name = name;
            if (mrutils::mutexTryAcquire(updateMutex)) {
                if (init_ && !enteringSearch 
                        && !enteringText && visible) resetPrompt();
                mrutils::mutexRelease(updateMutex);
            }
        }

        void nextColumn(bool highlight=true);
        void prevColumn(int colsBack = 1);

        inline ColChooser& next() {
            if (building >= 0) build();
            building = data[active+1].choices.size();
            if (!hideNumbers) 
                choiceBuilder << std::left << std::setw(3) << (building+startIndex) << ":  ";
            return *this;
        }

        template <class T>
        inline ColChooser& operator << (T& d) {
            choiceBuilder << d;
            return *this;
        }


    private:
        void done(bool destroy=false);
        bool init();
        int mouse();

        inline void toggleTargetInternal(int hid) {
            toggleTarget(hid);
            if (onTargetFn != NULL) {
                onTargetFn(*this,hid, targeted[hid]);
            }
        }

        inline void build() {
            data[active+1].choices.push_back(choiceBuilder.str());

            if (data.size() > active+2) {
                if (data[active+1].choices.size() == 1)
                    data[active+1].colWidth = choiceBuilder.str().size();
                else 
                    data[active+1].colWidth = MIN((cols-colStart-(data.size()-active-2))/(data.size()-active-1),
                        MAX(data[active+1].colWidth, choiceBuilder.str().size()));
            } else if (active+2 == data.size()) { // last column
                if (data.size() == 1) data[active+1].colWidth = cols - colStart;
                else data[active+1].colWidth = cols - 1 - colStart - data[active].colWidth;

                if (data[active+1].choices.size() > targeted.size())
                    targeted.push_back(false);
            }

            choiceBuilder.str("");
            building = -1;
        }

        inline void resetPrompt() { printPrompt(name.c_str()); }
        void printPrompt(const char * prompt, const char * prefix = NULL);


    private:
        std::string name;

        bool createdScreen; 
        public: int winRows, winCols; private:
        bool init_; 
        public: void * win; private:

        MUTEX updateMutex;

        bool createdWin;
        void * chooserWin;

        bool init_, done_, acceptReturn;

        bool hideNumbers;
        int building, startIndex;
        std::stringstream choiceBuilder;

        int active, colStart, selectNumber;
        char lastCommand[128];

        bool enteringSearch, enteringText; 
        volatile bool visible; char *inputP;
        std::vector<Column> data;
        std::vector<int> depth; // list of selected ids for each column
        optionsFunc optionsFn; void * optionData;
        changeFunc changeFn; void * changeData;

        openFunc openFn; void * openData;
        onTargetFunc onTargetFn;

        std::vector<bool> targeted;
        std::set<int> targets;
};

}

#endif

