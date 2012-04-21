#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUI_H
#endif

#ifndef _MR_CPPLIB_GUI_H
#define _MR_CPPLIB_GUI_H
#define _MR_CPPLIB_GUI_H_INCLUDE

#include "mr_threads.h"
#include "mr_signal.h"

// globals used instead of ncurses (which is wrong)
    /*
#ifdef __cplusplus
    extern "C" {
#endif
        extern int LINES;
        extern int COLS;
#ifdef __cplusplus
    }
#endif
    */

namespace mrutils {
    namespace gui {
        extern mrutils::mutex_t mutex;

        extern const int COL_BLANK    ;
        extern const int ATR_BLANK    ;
        
        extern const int COL_SELECTED ;
        extern const int ATR_SELECTED ;
        
        extern const int COL_INPUT    ;
        extern const int ATR_INPUT    ;
       
        extern const int COL_TARGETED ;
        extern const int ATR_TARGETED ;
      
        extern const int COL_CURSOR   ;
        extern const int ATR_CURSOR   ;
     
        extern const int COL_BLACK    ;
        extern const int ATR_BLACK    ;

        extern const int COL_WHITE    ;
        extern const int ATR_WHITE    ;

        extern const int COL_ERROR    ;
        extern const int ATR_ERROR    ;

        extern const int COL_NEW      ;
        extern const int ATR_NEW      ;

        extern const int COL_DIR      ;
        extern const int ATR_DIR      ;
  
       /**
        * these are all for bloomberg emulation
        */
        extern const int COL_R        ;
        extern const int COL_HR       ;
        extern const int COL_XR       ;
        extern const int COL_B        ;
        extern const int COL_HB       ;
        extern const int COL_XB       ;
        
        extern const int ATR_R        ;
        extern const int ATR_HR       ;
        extern const int ATR_XR       ;
        extern const int ATR_B        ;
        extern const int ATR_HB       ;
        extern const int ATR_XB       ;

        extern int _ACS_VLINE;
        extern int _ACS_HLINE;

        void init(int lines=0, int cols=0);
        void setTermTitle(const char * title);
        void mrefresh(void * win = NULL);
        void forceClearScreen(bool cursor = false);

        void * _newwin(int rows, int cols, int y, int x);
        void _wrefresh(void * win);
        void _refresh();
        void _wmove(void * win, int y, int x);
        void _mvwaddnstr(void * win, int y, int x, const char * str, size_t len);
        void _werase(void * win);
        void _mvwvline(void * win, int y, int x, int ch, int n);
        void _mvwhline(void * win, int y, int x, int ch, int n);

        /**
         * Brute force destruction of ncurses & resetting of terminal
         * back to usable state
         */
        void restoreTerm();

        extern std::string termTitle;
    };
}

#endif
