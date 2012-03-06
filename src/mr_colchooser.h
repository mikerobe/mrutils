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

public:
    class Column {
        friend class ColChooser; 

        public:
            struct searchTerm_t {
                std::string m_term;
                bool m_invert;
                bool m_useFn; /**< use the search function */

                searchTerm_t(std::string const &term) :
                        m_term(term),
                        m_invert(false),
                        m_useFn(false)
                {}

                searchTerm_t() :
                        m_invert(false),
                        m_useFn(false)
                {}
                

                inline char const *c_str() const
                { return m_term.c_str(); }

                inline bool empty() const
                { return m_term.empty(); }
            };


        public:
            Column(ColChooser& cc) :
                cc(cc),
                colNumber(0),
                direction(1),
                colWidth(0),
                colStart(0),
                startNum(0),
                headId(0),
                tailId(-1),
                headNum(0),
                matchNum(-1),
                showNumbers(false),
                searchTerm(0),
                searchTargeted(false)
            {}

            // only called once, on instantiation of the vector
            inline Column& operator=(Column const&)
            { return *this; }

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

            inline void search(searchTerm_t const &term)
            {
                if (!term.empty())
                {
                    searchTerms.resize(searchTerm+1);
                    searchTerms[searchTerm] = term;
                }

                search();
            }

            void search();

            inline void top()
            {
                if (matches.empty())
                    return;
                highlight(match(0));
            }

            void bottom();

            void rebuild();
            void clear(bool inclEntries = true);
            void addEntry(std::string const& str);
            int appendEntry(const char * str);

        private:
            void highlightTail();
            void highlightHead();
            void deselect(int i = -1);
            void select(int i = -1);

            inline void printEntry(int row, int id, bool clrSpaces=false);

            inline int match(int matchNum)
            {
                if (direction > 0)
                    return matches[matchNum];
                else
                    return matches[matches.size()-1-matchNum];
            }

            /**
              * this is for on-the-fly inserts and appends
              * will only search the string header and won't call the search function
              */
            bool matchesSearch(const char * str);
            bool matchesSearch(int id);

            /**
             * Starting with tailId, adds at most maxRows rows to the
             * matches structure
             */
            int addMatches(int maxRows);

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
            /**
             * live search calls every character, so have to separately
             * track which term we're on
             */
            int searchTerm;
            typedef std::vector<searchTerm_t> searchTerms_t;
            searchTerms_t searchTerms;

            bool searchTargeted;
    };

    friend class Column;

public:
    ColChooser(int columns = 1
        ,int y0 = 0, int x0 = 0, int rows = -1, int cols = 0);

    ~ColChooser();

public: // init & setup

    inline void setSelectedColors(int atr, int col)
    {
        this->atrSelected = atr;
        this->colSelected = col;
    }

    /**
     * Used to set the direction of new entries -- so the higher row
     * numbers are on top instead of at the bottom
     */
    void setDirectionUp(bool tf = true, int column = -1);

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

    /**
     * Show the row numbers
     */
    void setShowNumbers(bool tf = true, int column = -1);

    inline void freeze()
    { frozen = true; }

    void thaw();

public: // Adding data
    /**
     * Add to the next column
     */
    inline ColChooser& next()
    {
        if (building)
        {
            std::string entry = builder.str();
            builder.str("");
            cols[activeCol+1].addEntry(entry);
        }
        building = true;

        return *this;
    }

    template <class T>
    inline ColChooser& operator << (const T& d)
    { return builder << d, *this; }

    inline ColChooser& operator << (const char * d)
    { return builder << d, *this; }

    void remove(int id, int column = 0, bool lock = true);
    void insert(int id, const char * str, int column = 0, bool lock=true);
    void move(int id, int to, int column = 0, bool lock = true);
    void setColor(int id, int ATR, int COL, int column = 0,
            bool lock = true);

    void clearTargets();
    void setTarget(int id, bool tf = true, bool lock = true);
    int append(const char * str, int column = 0, bool lock = true);
    void redraw(int id, const char * str, int column = 0);

public: // getting data
    void show();
    inline std::set<int>& getTargets()
    { return targets; }

    int winRows, winCols;
    void * win;

    int activeCol, nCols;
    std::vector<Column> cols;
    std::vector<int> selected; ///< list of selected ids for each column
    mrutils::mutex_t mutex;

public: // functions to search & assign

    /**
      * Display only those results already targeted
      */
    bool toggleSearchTargeted();

    inline void liveSearch(Column::searchTerm_t const &search)
    {
        mrutils::mutexScopeAcquire lock(mutex);

        if (activeCol >= 0)
            cols[activeCol].search(search);
    }
    bool liveSearchEnd(bool keepSearch);

    bool clearSearch();

    void lineSearch(Column::searchTerm_t const &search);
    inline bool lineSearchEnd(bool /*keepSearch*/)
    { return true; }

public: // navigation
    inline bool nextRow()
    { 
        mrutils::mutexAcquire(mutex);
        if (activeCol >= 0) cols[activeCol].nextRow(); 
        mrutils::mutexRelease(mutex);
        return true; 
    }
    inline bool prevRow()
    { 
        mrutils::mutexAcquire(mutex);
        if (activeCol >= 0) cols[activeCol].prevRow(); 
        mrutils::mutexRelease(mutex);
        return true; 
    }
    inline bool nextPage()
    {
        mrutils::mutexAcquire(mutex);
        if (activeCol >= 0) cols[activeCol].nextPage(); 
        mrutils::mutexRelease(mutex);
        return true; 
    }
    inline bool prevPage()
    { 
        mrutils::mutexAcquire(mutex);
        if (activeCol >= 0) cols[activeCol].prevPage(); 
        mrutils::mutexRelease(mutex);
        return true; 
    }

    inline bool top()
    { 
        mrutils::mutexAcquire(mutex);
        if (activeCol >= 0) cols[activeCol].top(); 
        mrutils::mutexRelease(mutex);
        return true; 
    }

    inline bool bottom()
    { 
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

    /**
     * Toggle target on current row
     */
    bool toggleTarget();

private:

    inline void selectionChanged()
    {
        if (selectionFn != NULL) {
            selectionFn(*this, selectionData);
        }
    }

    void buildLastColumn();

private:
    bool init_; 
    bool frozen;

    optionsFunc optionsFn; void * optionsData;
    selectionFunc selectionFn; void * selectionData;
    targetFunc targetFn; void * targetData;

    searchFunc searchFn; void * searchData;

    int atrSelected, colSelected;
    static char spaces[256];

private: // for the construction of entries
    bool building; int startIndex;
    std::stringstream builder;
    char numberBuf[6];

public: // targetting -- applicable to the last column
    std::vector<bool> targeted;
    std::set<int> targets;
    bool searchTargeted;
};

}

#endif
