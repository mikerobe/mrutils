#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUILIST_H
#endif

#ifndef _MR_CPPLIB_GUILIST_H
#define _MR_CPPLIB_GUILIST_H
#define _MR_CPPLIB_GUILIST_H_INCLUDE

#include <iostream>
#include <vector>
#include "fastdelegate.h"
#include "mr_numerical.h"
#include "mr_gui.h"
#include "mr_threads.h"

namespace mrutils {

class GuiListEntry {
    public:
        virtual bool operator<(const GuiListEntry& other) const = 0;
};

class GuiList {
    public:
        class Row;
        friend class Row;

        GuiList(int y0 = 0, int x0 = 0, int rows = 0, int cols = 0);
        ~GuiList();

    public:
       /********************
        * Main functions
        ********************/

        void show();
        Row& add(GuiListEntry& entry);

        /**
         * Clears & frees all data
         * */
        void reset(bool lock = true);

        inline void freeze() { frozen = true; }
        void thaw();

    private:
        void debug();

    private:
        std::vector<Row*> rows;

        bool init_;
        public: int winRows, winCols; private:
        void * win;
        bool frozen;

        public: mrutils::mutex_t mutex; private:

    public:
        class Row {
            friend class GuiList;
            public:
                inline static bool compare(Row* a, Row* b) 
                { return a->entry < b->entry; }

            public:
                Row(GuiList* list, GuiListEntry& entry, int row = 0)
                : highlight(0), isB(true), row(row), entry(entry), list(*list)
                {
                    str[0] = '\0';
                }

            public:

                void update(bool lock = true);
                void drop();

            public:
                // content
                static const int strSize = 128;
                char str[strSize];

                int highlight;

            private:
                void print();

            private:
                bool isB; unsigned row;
                GuiListEntry& entry;
                GuiList& list;
        };


};

}

#endif
