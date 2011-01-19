#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_COL_CHOOSER_H
#endif

#ifndef _MR_CPPLIB_COL_CHOOSER_H
#define _MR_CPPLIB_COL_CHOOSER_H
#define _MR_CPPLIB_COL_CHOOSER_H_INCLUDE

#include <iostream>
#include <vector>
#include "fastdelegate.h"
#include <set>
#include <algorithm>
#include "mr_numerical.h"
#include "mr_gui.h"

/**
 * TODO: add a last search id so that matches knows how far it has searched
 * in elements. -- will make move operations faster in some cases
 * */

namespace mrutils {

class ColChooser {
    typedef fastdelegate::FastDelegate2<ColChooser&,void*> optionsFunc;
    typedef fastdelegate::FastDelegate2<ColChooser&,void*> selectionFunc;
    typedef fastdelegate::FastDelegate4<ColChooser&,int,bool,void*> targetFunc;
    typedef fastdelegate::FastDelegate4<ColChooser&,int,const char *,void*,bool> searchFunc;

    class Column {
        friend class ColChooser; 

        public:
            Column(ColChooser& cc)
            : cc(cc), colNumber(0), direction(1), colWidth(0)
             ,colStart(0)
             ,startNum(0), headId(0), tailId(-1)
             ,headNum(0), matchNum(-1)
             ,showNumbers(false)
             ,searchTerm(0), searchFnTerm(0)
             ,searchTargeted(false)
            {}

            // only called once, on instantiation of the vector
            Column& operator=(_UNUSED const Column& other) { return *this; }

        public:
           /*********************
            * Main functions
            *********************/

            void redraw(int id, bool clrSpaces = false);
            void highlight(int id);
            void nextRow();
            void prevRow();
            void nextPage();
            void prevPage(bool highlightTail = false);
            void search(const char * str = NULL, bool fn = false);

            inline void top() {
                if (matches.empty()) return;
                highlight(match(0));
            }
            void bottom();

            void rebuild();
            inline void clear(bool inclEntries = true) {
                cc.selected[colNumber] = -1;
                searchTerm = 0;
                searchTerms.clear();
                searchFnTerms.clear();
                headId = 0; headNum = 0;
                matchNum = -1; tailId = -1;
                startNum = matches.size();
                searchTargeted = false;

                if (inclEntries) {
                    entries.clear(); matches.clear();
                    formatATR.clear(); formatCOL.clear();
                    colWidth = 0; startNum = 0;
                }

            }

            inline void addEntry(std::string& str) {
                matches.push_back(entries.size());
                entries.push_back(str);
                colWidth = std::max(colWidth, (int)str.size() + (showNumbers?5:0));
                formatATR.resize(entries.size(), gui::ATR_BLANK);
                formatCOL.resize(entries.size(), gui::COL_BLANK);
                if (cc.targeted.size() >= entries.size() && cc.targeted[entries.size()-1]) {
                    formatATR[entries.size()-1] = gui::ATR_TARGETED;
                    formatCOL[entries.size()-1] = gui::COL_TARGETED;
                }
            }

            int appendEntry(const char * str);

        private:
            void highlightTail();
            void highlightHead();
            void deselect(int i = -1);
            void select(int i = -1);

            inline void printEntry(int row, int id, bool clrSpaces=false);

            inline int match(int matchNum) {
                if (direction > 0) return matches[matchNum];
                else return matches[matches.size()-1-matchNum];
            }

            /**
              * this is for on-the-fly inserts and appends
              * will only search the string header and won't call the search function
              */
            inline bool matchesSearch(const char * str) {
                if (searchTargeted) return false;

                // must match each string in searchTerms
                for (std::vector<std::string>::iterator it = searchTerms.begin()
                    , itE = searchTerms.end(); it != itE; ++it) {
                    if (!mrutils::stristr(str, it->c_str())) return false;
                }

                // TODO how to handle new additions with search function

                return true;
            }

            inline bool matchesSearch(int id) {
                if (searchTargeted && ((int)cc.targeted.size() <= id || !cc.targeted[id])) return false;

                const char * str = entries[id].c_str();

                // must match each string in searchTerms
                for (std::vector<std::string>::iterator it = searchTerms.begin()
                    , itE = searchTerms.end(); it != itE; ++it) {
                    if (!mrutils::stristr(str, it->c_str())) return false;
                }

                // must also match search term fn (if not null)
                if (cc.searchFn != NULL) {
                    for (std::vector<std::string>::iterator it = searchFnTerms.begin()
                        , itE = searchFnTerms.end(); it != itE; ++it) {
                        if (!cc.searchFn(cc,id,it->c_str(),cc.searchData)) return false;
                    }
                }

                return true;
            }

            // adds up to rows to matches starting with tailId
            inline int addMatches(int maxRows) {
                int rows = 0;
                if (direction > 0) {
                    for (int i = tailId + 1; rows < maxRows && i < (int)entries.size(); ++i) {
                        if (matchesSearch(i)) {
                            matches.push_back(i); ++rows;
                        }
                    }
                } else {
                    for (int i = tailId - 1; rows < maxRows && i >= 0; --i) {
                        if (matchesSearch(i)) {
                            matches[--startNum] = i; ++rows;
                        }
                    }
                }

                return rows;
            }

            inline bool cGT(int a, int b) {
                if (direction<0) return (b > a);
                else return (a > b);
            }
            inline bool cLT(int a, int b) {
                if (direction<0) return (b < a);
                else return (a < b);
            }

        private:
            ColChooser& cc;
            int colNumber;

            int direction;
            int colWidth, colStart;
            std::vector<std::string> entries;
            std::vector<int> matches;
            int startNum;
            int headId, tailId, headNum, matchNum;

            std::vector<int> formatATR;
            std::vector<int> formatCOL;
            bool showNumbers;

        public:
            int searchTerm, searchFnTerm; // livesearch calls every char. have to keep track of term count
            std::vector<std::string> searchTerms;
            std::vector<std::string> searchFnTerms;
            bool searchTargeted;
    };

    friend class Column;

    public:
        ColChooser(int columns = 1
            ,int y0 = 0, int x0 = 0, int rows = -1, int cols = 0);

        ~ColChooser();

    public:
       /*******************
        * Init & setup
        *******************/

        inline void setSelectedColors(int atr, int col) {
            this->atrSelected = atr;
            this->colSelected = col;
        }

        inline void setDirectionUp(bool tf = true, int column=-1) { 
            if (column < 0) {
                for (int i =0; i < nCols; ++i) cols[i].direction = (tf?-1:1);
            } else {
                if (column > nCols-1) return;
                cols[column].direction = (tf?-1:1);
            }
        }

        void clear();

        inline void setStartIndex(int startIndex) 
        { this->startIndex = startIndex; }
        
        inline void setSearchFunction(searchFunc searchFn, void * searchData = NULL)
        { this->searchFn = searchFn; this->searchData = searchData; }

        inline void setOptionsFunction(optionsFunc optionsFn, void * optionsData = NULL)
        { this->optionsFn = optionsFn; this->optionsData = optionsData; }

        inline void setSelectionFunction(selectionFunc selectionFn, void * selectionData = NULL)
        { this->selectionFn = selectionFn; this->selectionData = selectionData; }

        inline void setTargetFunction(targetFunc targetFn, void * targetData = NULL)
        { this->targetFn = targetFn; this->targetData = targetData; }

        inline void setShowNumbers(bool tf = true, int column=-1) { 
            if (column < 0) {
                for (int i =0; i < nCols; ++i) cols[i].showNumbers = tf;
            } else {
                if (column > nCols-1) return;
                cols[column].showNumbers = tf;
            }
        }

        inline void freeze()
        { frozen = true; }

        void thaw();

    public:
       /********************
        * Adding data
        ********************/

        // add to next column
        inline ColChooser& next() {
            if (building) {
                std::string entry = builder.str(); builder.str("");
                cols[activeCol+1].addEntry(entry);
            } building = true;
            return *this;
        }

        template <class T>
        inline ColChooser& operator << (const T& d) {
            builder << d;
            return *this;
        }
        inline ColChooser& operator << (const char * d) {
            builder << d;
            return *this;
        }

        void remove(int id, int column = 0, bool lock = true);
        void insert(int id, const char * str, int column = 0, bool lock=true);
        void move(int id, int to, int column = 0, bool lock = true);

        void setColor(int id, int ATR, int COL, int column = 0, bool lock = true) {
            if (column < 0 || column > nCols-1) return;
            if (lock) mrutils::mutexAcquire(mutex);
                if (id >= 0) {
                    if (id+1 > (int)cols[column].formatATR.size()) {
                        cols[column].formatATR.resize(id+1,gui::ATR_BLANK);
                        cols[column].formatCOL.resize(id+1,gui::COL_BLANK);
                    }
                    if (cols[column].formatATR[id] != ATR 
                        || cols[column].formatCOL[id] != COL) {
                        cols[column].formatATR[id] = ATR;
                        cols[column].formatCOL[id] = COL;
                        cols[column].redraw(id);
                    }
                }
            if (lock) mrutils::mutexRelease(mutex);
        }

        void clearTargets();
        void setTarget(int id, bool tf = true);
        inline int append(const char * str, int column = 0, bool lock = true) {
            if (column < 0 || column > nCols-1 || column > activeCol+1) return -1;
            if (lock) mrutils::mutexAcquire(mutex);
                const int id = cols[column].appendEntry(str);
            if (lock) mrutils::mutexRelease(mutex);
            return id;
        }

        inline void redraw(int id, const char * str, int column = 0) {
            if (column < 0 || column > nCols-1) return;

            mrutils::mutexAcquire(mutex);
                if (id >= 0 && id < (int)cols[column].entries.size()) {
                    cols[column].entries[id] = str;
                    cols[column].redraw(id, true);
                }
            mrutils::mutexRelease(mutex);
        }

    public:
       /********************
        * Getting data
        ********************/

        void show();
        inline std::set<int>& getTargets() { return targets; }


    public:
       /********************
        * Functions to assign
        ********************/

        /**
          * Display only those results already targeted
          */
        inline bool toggleSearchTargeted() {
            mrutils::mutexAcquire(mutex);
            searchTargeted = !searchTargeted;
            cols[activeCol].searchTargeted = searchTargeted;
            if (activeCol == nCols-1) 
                cols[activeCol].search();
            mrutils::mutexRelease(mutex);
            return true;
        }

        inline void liveSearch(const char * search) {
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].search(search);
            mrutils::mutexRelease(mutex);
        }
        inline bool liveSearchEnd(bool keepSearch) { 
            mrutils::mutexAcquire(mutex);
            if (keepSearch) {
                if (activeCol >= 0) ++cols[activeCol].searchTerm;
            } else if ((int)cols[activeCol].searchTerms.size() == cols[activeCol].searchTerm+1) {
                cols[activeCol].searchTerms.pop_back();
                cols[activeCol].search();
            }
            mrutils::mutexRelease(mutex);

            return true; 
        }

        inline void liveSearchFn(const char * search) {
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].search(search, true);
            mrutils::mutexRelease(mutex);
        }
        inline bool liveSearchFnEnd(bool keepSearch) { 
            mrutils::mutexAcquire(mutex);
            if (keepSearch) {
                if (activeCol >= 0) ++cols[activeCol].searchFnTerm;
            } else if ((int)cols[activeCol].searchFnTerms.size() == cols[activeCol].searchFnTerm+1) {
                cols[activeCol].searchFnTerms.pop_back();
                cols[activeCol].search();
            }
            mrutils::mutexRelease(mutex);

            return true; 
        }

        inline bool clearSearch() {
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) {
                cols[activeCol].searchTerms.clear();
                cols[activeCol].searchTerm = 0;
                cols[activeCol].searchFnTerms.clear();
                cols[activeCol].searchFnTerm = 0;
                cols[activeCol].search();
            }
            mrutils::mutexRelease(mutex);
            return true;
        }

        inline void lineSearch(const char * ids) {
            mrutils::mutexAcquire(mutex);
            int id;
            if (activeCol >= 0 && ids[0] != '\0'
                && ((id = atoi(ids) - startIndex) >= 0)
                ) cols[activeCol].highlight(id);
            mrutils::mutexRelease(mutex);
        }
        inline bool lineSearchEnd(_UNUSED bool keepSearch) { return true; }

        inline bool nextRow() { 
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].nextRow(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }
        inline bool prevRow() { 
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].prevRow(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }
        inline bool nextPage() {
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].nextPage(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }
        inline bool prevPage() { 
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].prevPage(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }

        inline bool top() { 
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].top(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }

        inline bool bottom() { 
            mrutils::mutexAcquire(mutex);
            if (activeCol >= 0) cols[activeCol].bottom(); 
            mrutils::mutexRelease(mutex);
            return true; 
        }

        /**
          * These are separated so they can be used as fastdelegate
          * function pointers
          */
        inline bool nextCol() {
            nextColDirect();
            return true;
        }
        inline bool prevCol() {
            prevColDirect();
            return true;
        }

        void nextColDirect(bool lock = true);
        void prevColDirect(bool lock = true);

        inline bool toggleTarget() {
            mrutils::mutexAcquire(mutex);
                if (activeCol != nCols-1 || cols[activeCol].matchNum < 0) {
                    mrutils::mutexRelease(mutex);
                    return true;
                }
                int id = cols[activeCol].match(cols[activeCol].matchNum);
                if ((int)targeted.size() < id+1) targeted.resize(id+1,false);
                bool tf = !targeted[id];
            mrutils::mutexRelease(mutex);

            setTarget(id, tf);

            if (targetFn != NULL) targetFn(*this, id, tf, targetData);

            return true;
        }

    private:

        inline void selectionChanged() {
            if (selectionFn != NULL) {
                selectionFn(*this, selectionData);
            }
        }
        inline void buildLastColumn() {
            if (activeCol+1 == nCols) return;
            targeted.clear(); targets.clear();

            if (activeCol >= 0) {
                cols[activeCol+1].colStart = 
                    cols[activeCol].colStart +
                    cols[activeCol].colWidth + 1;
            }

            cols[activeCol+1].clear();

            if (optionsFn != NULL) {
                optionsFn(*this, optionsData);
                if (building) {
                    std::string entry = builder.str(); builder.str("");
                    cols[activeCol+1].addEntry(entry); building = false;
                }
            }

            if (activeCol+1 == nCols-1) {
                targeted.resize(cols[activeCol+1].entries.size(),false);
            }

            cols[activeCol+1].rebuild();
        }

    private:
        bool init_; 
        public: int winRows, winCols; private:
        public: void * win; private:
        bool frozen;

        optionsFunc optionsFn; void * optionsData;
        selectionFunc selectionFn; void * selectionData;
        targetFunc targetFn; void * targetData;

        searchFunc searchFn; void * searchData;

        public: int activeCol, nCols; private:
        public: std::vector<Column> cols; private:
        public: std::vector<int> selected; private: // list of selected ids for each column

        public: MUTEX mutex; private:

        int atrSelected, colSelected;
        static char spaces[256];

    private:
       /*** for the construction of entries ***/
        bool building; int startIndex;
        std::stringstream builder;
        char numberBuf[6];

    public:
       /*** targetting -- only applicable for last column. ***/
        std::vector<bool> targeted;
        std::set<int> targets;
        bool searchTargeted;
};

}

#endif
