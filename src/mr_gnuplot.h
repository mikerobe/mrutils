#ifndef _MR_CPP_GNUPLOT2_H
#define _MR_CPP_GNUPLOT2_H
#define _CRT_SECURE_NO_DEPRECATE

#include "mr_exports.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <algorithm>
#include "float.h"
#include "fastdelegate.h"
#include "mr_strutils.h"
#include "mr_files.h"
#include "mr_numerical.h"
#include "mr_threads.h"

#define _MR_GNUPLOT_DEFAULT_COLOR "black"

namespace mrutils {
    class _API_ Gnuplot {
        public:
            enum linestyle { 
                LS_SOLID = 1,
                LS_DASHED = 2,
                LS_LONGDASH = 4,
                LS_DOTTED = 3
            };
            enum axis { AXIS_X=0, AXIS_Y=1, AXIS_Y2=2};
            enum style { POINTS, LINES, FILLED, STEPS, BOXES, IMPULSES, ERRORS };
            enum xtype { X_PLAIN, X_DATE, X_MILLIS, X_EPOCH };

        public:
            Gnuplot()
            : gnucmd(NULL), isImage(false)
             ,init_(false), valid(false)
             ,xType(X_PLAIN)
             ,drawnTitle(false)
            {
                memset(drawnAxis,0,sizeof(drawnAxis));
                memset(setRanges,0,sizeof(setRanges));
                zeroY[0] = true; zeroY[1] = true;

                nextPlot.yAxis = 1;
                nextPlot.keyHeight = 0;
            }

            inline ~Gnuplot() {
                if (gnucmd != NULL) {
                    if (isImage) {
                        cmd("exit\n"); 
                    } else {
                        MR_PCLOSE(gnucmd);
                    }
                }
            }

        public:
           /*****************
            * Settings
            *****************/

            inline void clear(bool clearRanges = true) {
                if (init_ && valid && drawnTitle) {
                    if (isImage) cmd("unset multiplot");
                    else cmd("clear");
                }
                drawnTitle = false;
                memset(drawnAxis,0,sizeof(drawnAxis));
                if (clearRanges) memset(setRanges,0,sizeof(setRanges));
                nextPlot.keyHeight = 0;
            }

            inline void draw() {
                if (!isImage) return;
                if (!drawnTitle) return;
                cmd("unset multiplot");
                drawnTitle = false;
                memset(drawnAxis,0,sizeof(drawnAxis));
                nextPlot.keyHeight = 0;
            }

            /**
              * Draws a 50% transparent white box over previous curves
              */
            bool fadeOldCurves(const double frac = 0.5);

            inline void setXRange(double min, double max) 
            { xRange[0] = min; xRange[1] = max; rangeFinish(AXIS_X); }

            inline void setYRange(double min, double max) 
            { yRange[0] = min; yRange[1] = max; rangeFinish(AXIS_Y); }

            inline void setY2Range(double min, double max) 
            { y2Range[0] = min; y2Range[1] = max; rangeFinish(AXIS_Y2); }

            template <class T>
            inline void setXRange(const std::vector<T> *v, ...) {
                xRange[0] = DBL_MAX; xRange[1] = -DBL_MAX;

                va_list list; va_start(list,v);
                for(;v != NULL;v = va_arg(list, const std::vector<T>*)) {
                    if (!v->empty()) {
                        xRange[0] = std::min(xRange[0],(double)mrutils::min_element(*v));
                        xRange[1] = std::max(xRange[1],(double)mrutils::max_element(*v));
                    }
                }

                if (xRange[0] < xRange[1]) rangeFinish(AXIS_X);
            }

            template <class T>
            inline void setYRange(const std::vector<T> *v, ...) {
                yRange[0] = DBL_MAX; yRange[1] = -DBL_MAX;

                va_list list; va_start(list,v);
                for(;v != NULL;v = va_arg(list, const std::vector<T>*)) {
                    if (!v->empty()) {
                        yRange[0] = std::min(yRange[0],(double)mrutils::min_element(*v));
                        yRange[1] = std::max(yRange[1],(double)mrutils::max_element(*v));
                    }
                }

                if (yRange[0] < yRange[1]) rangeFinish(AXIS_Y);
            }

            template <class T>
            inline void setY2Range(const std::vector<T> *v, ...) {
                y2Range[0] = DBL_MAX; y2Range[1] = -DBL_MAX;

                va_list list; va_start(list,v);
                for(;v != NULL;v = va_arg(list, const std::vector<T>*)) {
                    if (!v->empty()) {
                        y2Range[0] = std::min(y2Range[0],(double)mrutils::min_element(*v));
                        y2Range[1] = std::max(y2Range[1],(double)mrutils::max_element(*v));
                    }
                }

                if (y2Range[0] < y2Range[1]) rangeFinish(AXIS_Y2);
            }

            inline void setXDate() { setXType(X_DATE); }
            inline void setXMillis() { setXType(X_MILLIS); }
            inline void setXEpoch() { setXType(X_EPOCH); }
            inline void setXType(xtype type) {
                switch (type) {
                    case X_EPOCH:
                        cmd("set xdata time");
                        cmd("set timefmt \"%s\"");
                        cmd("set format x \"%H:%M\"");
                        break;
                    case X_MILLIS:
                        cmd("set xdata time");
                        cmd("set timefmt \"%H%M%S\"");
                        cmd("set format x \"%H:%M\"");
                        break;
                    case X_DATE:
                        cmd("set xdata time");
                        cmd("set timefmt \"%Y%m%d\"");
                        cmd("set format x \"%d%b%y\"");
                        break;
                    case X_PLAIN:
                        cmd("unset xdata");
                        cmd("set format x \"%g\"");
                        break;
                }

                xType = type;
            }

            inline void setLabels(const char * title, const char * xlab="", const char * ylab="", const char * y2lab="") {
                this->title = title;
                labels[AXIS_X] = xlab;
                labels[AXIS_Y] = ylab;
                labels[AXIS_Y2] = y2lab;
                if (xlab[0] != 0) cmd("set bmargin 3");
                if (ylab[0] != 0) cmd("set lmargin 9");
                if (y2lab[0] != 0) cmd("set rmargin 9");
            }
            inline void setTitle(const char * title) 
            { this->title = title; }

            inline void setLabel(axis axis, const char * label) {
                labels[axis] = label; 

                if (label[0] != 0) {
                    // make room
                    switch (axis) {
                        case AXIS_X:
                            cmd("set bmargin 3");
                            break;
                        case AXIS_Y:
                            cmd("set lmargin 9");
                            break;
                        case AXIS_Y2:
                            cmd("set rmargin 9");
                            break;
                    }
                }
            }

            inline void setGrid() { cmd("set grid"); }
            inline void setNoZero(int axis = 1) { 
                if (axis == 1) zeroY[0] = false;
                else zeroY[1] = false;
            }

            inline void setTerm(const char * term, const char * file = NULL) {
                if (gnucmd != NULL) MR_PCLOSE(gnucmd);
                init(term,file);
            }

        public:
           /******************
            * Plot precursors
            ******************/

            inline Gnuplot& lines(int thickness = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR, linestyle lineStyle = LS_SOLID) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lt %d lc rgb \"%s\" lw %d\n", lineStyle, color, thickness);
                    nextPlot.style = LINES; 
                } return *this;
            }
            inline Gnuplot& points(int size = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 ps %f lc rgb \"%s\"\n", 0.5 * size, color);
                    nextPlot.style = POINTS; 
                } return *this;
            }
            inline Gnuplot& filled(const char * color = _MR_GNUPLOT_DEFAULT_COLOR, double transparency = 1) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lc rgb \"%s\"\n", color);
                    fprintf( gnucmd, "set style fill transparent solid %f noborder\n", transparency);
                    nextPlot.style = FILLED; 
                } return *this;
            }
            inline Gnuplot& steps(int thickness = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR, linestyle lineStyle = LS_SOLID) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lt %d lc rgb \"%s\" lw %d\n", lineStyle, color, thickness);
                    nextPlot.style = STEPS; 
                } return *this;
            }
            inline Gnuplot& boxes(int thickness = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR, linestyle lineStyle = LS_SOLID) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lt %d lc rgb \"%s\" lw %d\n", lineStyle, color, thickness);
                    nextPlot.style = BOXES; 
                } return *this;
            }
            inline Gnuplot& impulse(int thickness = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR, linestyle lineStyle = LS_SOLID) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lt %d lc rgb \"%s\" lw %d\n", lineStyle, color, thickness);
                    nextPlot.style = IMPULSES; 
                } return *this;
            }
            inline Gnuplot& errors(int thickness = 1, const char * color = _MR_GNUPLOT_DEFAULT_COLOR, linestyle lineStyle = LS_SOLID) { 
                if (valid || (!init_ && init())) {
                    fprintf( gnucmd, "set style line 1 pt 5 lt %d lc rgb \"%s\" lw %d\n", lineStyle, color, thickness);
                    nextPlot.style = ERRORS; 
                } return *this;
            }

            /**
              * Use y2 axis for next plot
              */
            inline Gnuplot& y2() { nextPlot.yAxis = 2; return *this; }

        public:
           /**********************
            * Main plot functions
            **********************/

            bool hline(double y = 0);
            bool vline(double x = 0);

            bool slope(double alpha, double beta);

            template <class T> bool plot(const std::vector<T> & x, const char * label);
            template <class T> inline bool plot(const std::vector<T> & x) { return plot(x,""); }

            template <class T, class T2> bool plotErrors(const std::vector<T>&x, const std::vector<T2>& lower, const std::vector<T2>& upper, const std::vector<T2>& mid, const char * label);
            template <class T, class T2> inline bool plotErrors(const std::vector<T>&x, const std::vector<T2>& lower, const std::vector<T2>& upper, const std::vector<T2>& mid) { return plotErrors(x,lower,upper,mid,""); }

            template <class T>
            bool diff(const std::vector<T> & y1, const std::vector<T> & y2, const char * labelGT, const char * labelLT);
            template <class T> inline bool diff(const std::vector<T> & y1, const std::vector<T> & y2) { return diff(y1,y2,"",""); }

            template <class Tx, class T>
            bool diff(const std::vector<Tx> & x, const std::vector<T> & y1, const std::vector<T> & y2, const char * labelGT, const char * labelLT);
            template <class Tx, class T> inline bool diff(const std::vector<Tx> & x, const std::vector<T> & y1, const std::vector<T> & y2)
            { return diff(x,y1,y2,"",""); }

            template <class T1, class T2>
            bool plot(const std::vector<T1> & x, const std::vector<T2> & y, const char * label);
            template <class T1, class T2> inline bool plot(const std::vector<T1> & x, const std::vector<T2> & y) { return plot(x,y,""); }

            bool plot(fastdelegate::FastDelegate1<double,double> func, double from=0, double to=1, int samples=1000);
            bool plot(fastdelegate::FastDelegate2<double,void*,double> func, void * data, double from=0, double to=1, int samples=1000);
            bool plot(fastdelegate::FastDelegate2<double,double,double> func, double p1, double from=0, double to=1, int samples=1000);
            bool plot(fastdelegate::FastDelegate3<double,double,double,double> func, double p1, double p2, double from=0, double to=1, int samples=1000);
            bool plot(fastdelegate::FastDelegate6<double,double,double,double,double,double,double> func
                ,double p1, double p2, double p3, double p4, double p5
                ,double from=0, double to=1, int samples=1000);

        public:
           /*******************
            * Utilities
            *******************/

            inline bool cmd(const char * s) {
                if (!valid && (init_ || !init())) return false;
                fputs(s, gnucmd); fputs("\n", gnucmd); fflush(gnucmd);
                return true;
            }

            double xRange[2], yRange[2], y2Range[2];

        private:
            bool init(const char * term = NULL, const char * file = NULL);
            char * getPath(char * path);
            void rangeFinish(axis axis);

        private:
            FILE * gnucmd;
            bool isImage;

            bool init_, valid;
            std::string title;
            std::string labels[3]; 
            xtype xType; bool zeroY[2];
            bool drawnAxis[3]; // x, y1, y2
            bool setRanges[3]; // x, y1, y2
            bool drawnTitle;

            struct nextPlot {
                int yAxis;
                mrutils::Gnuplot::style style;
                int keyHeight;
            } nextPlot;
    };
}

#endif
