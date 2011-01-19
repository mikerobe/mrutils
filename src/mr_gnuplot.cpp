#include <algorithm>
#include "mr_gnuplot.h"
#include "mr_strutils.h"
#include "mr_numerical.h"

#define PLOT_START \
    if (!valid && (init_ || !init())) return false;\
    if (!drawnTitle) {\
        fputs("set multiplot\n",gnucmd);\
        if (!title.empty())\
            fprintf(gnucmd, "set title \"%s\"\n", title.c_str());\
        drawnTitle = true;\
    } else fputs("set title \"\"\n",gnucmd);\
    if (!drawnAxis[AXIS_X]) {\
        if (!labels[AXIS_X].empty())\
            fprintf(gnucmd, "set xlab \"%s\"\n", labels[AXIS_X].c_str());\
        fputs("set xtics nomirror\n",gnucmd);\
        fputs("set mxtics 10\n",gnucmd);\
        drawnAxis[AXIS_X] = true;\
    } else { \
        fputs("set xlab \"\"\n",gnucmd);\
        fputs("unset xtics\n",gnucmd);\
    }\
    switch (nextPlot.yAxis) {\
        case 1:\
            if (!drawnAxis[AXIS_Y]) {\
                if (!labels[AXIS_Y].empty())\
                    fprintf(gnucmd, "set ylab \"%s\"\n", labels[AXIS_Y].c_str());\
                fputs("set ytics nomirror\n",gnucmd);\
                fputs("set mytics 10\n",gnucmd);\
                drawnAxis[AXIS_Y] = true;\
            } else {\
                fputs("set ylab \"\"\n",gnucmd);\
                fputs("unset ytics\n",gnucmd);\
            }\
            fputs("set y2lab \"\"\n",gnucmd);\
            fputs("unset y2tics\n",gnucmd);\
            break;\
        case 2:\
            if (!drawnAxis[AXIS_Y2]) {\
                if (!labels[AXIS_Y2].empty())\
                    fprintf(gnucmd, "set y2lab \"%s\"\n", labels[AXIS_Y2].c_str());\
                fputs("set y2tics nomirror\n",gnucmd);\
                fputs("set my2tics 10\n",gnucmd);\
                drawnAxis[AXIS_Y2] = true;\
            } else {\
                fputs("set y2lab \"\"\n",gnucmd);\
                fputs("unset y2tics\n",gnucmd);\
            }\
            fputs("set ylab \"\"\n",gnucmd);\
            fputs("unset ytics\n",gnucmd);\
            break;\
    }

#define KEY_HEIGHT \
    { mrutils::stringstream ss;\
      ss << "set key height " << (2*nextPlot.keyHeight); \
      cmd(ss.str().c_str()); }

#define PLOT_END\
    points();\
    nextPlot.yAxis = 1;\
    cmd("unset grid");

#define PLOT_FUNC(func_)\
    if (to <= from || samples < 2) return false;\
\
    PLOT_START cmd("unset key");\
\
    if (!setRanges[AXIS_X]) { \
        xRange[0] = from; xRange[1] = to;\
        rangeFinish(AXIS_X); \
    } else { \
        from = xRange[0]; to = xRange[1];\
    }\
\
    double x = from; double step = (to-from)/(samples-1);\
    std::vector<double> xs; std::vector<double> ys;\
    xs.reserve(samples); ys.reserve(samples);\
    double yMin = DBL_MAX, yMax = DBL_MIN; \
    for (int i = 0; i < samples; ++i, x += step) {\
        xs.push_back(x); double value = func_;\
        if (value < yMin) yMin = value; if (value > yMax) yMax = value; \
        ys.push_back(value); \
    }\
    if (!setRanges[nextPlot.yAxis]) {\
        if (nextPlot.yAxis == 1) { yRange[0] = yMin; yRange[1] = yMax; }\
        else { y2Range[0] = yMin; y2Range[1] = yMax; }\
        rangeFinish((axis)nextPlot.yAxis);\
    }\
    switch (xType) {\
        case X_DATE:\
        case X_EPOCH:\
            fprintf(gnucmd, "plot '-' using 1:2 with %s axis x1y%d\n"\
                ,styles[nextPlot.style], nextPlot.yAxis);\
            for (int i = 0; i < samples; ++i) \
                fprintf( gnucmd, "%d %f\n", (int)xs[i], ys[i]);\
            break;\
        case X_MILLIS:\
            fprintf(gnucmd, "plot '-' using (timecolumn(1)+$2/1000):3 with %s axis x1y%d\n"\
                ,styles[nextPlot.style], nextPlot.yAxis);\
            for (int i = 0; i < samples; ++i) {\
                    unsigned millis = (unsigned)xs[i];\
                    unsigned time = makeTimeHHMMSS(&millis);\
                    fprintf( gnucmd, "%06u %u %f\n", time, millis, ys[i]);\
            }\
            break;\
        case X_PLAIN:\
            fprintf(gnucmd, "plot '-' using 1:2 with %s axis x1y%d\n"\
                ,styles[nextPlot.style], nextPlot.yAxis);\
            for (int i = 0; i < samples; ++i) \
                fprintf( gnucmd, "%f %f\n", xs[i], ys[i]);\
            break;\
    }\
    fputs("e\n",gnucmd); fflush(gnucmd);\
    PLOT_END


namespace {
    template <class T> 
    inline static bool bye(T a) { 
        std::cerr << "Gnuplot " << a << std::endl;
        return false;
    }

    template <class T> 
    inline static char * byeNull(T a) {
        std::cerr << "Gnuplot " << a << std::endl;
        return NULL;
    }

    inline unsigned makeTimeHHMMSS(unsigned milliTime) {
        unsigned result = 0, tmp = 0;;

        tmp = milliTime / 3600000u;
        milliTime -= tmp * 3600000u;
        result += 10000u * tmp;

        tmp = milliTime / 60000u;
        milliTime -= tmp * 60000u;
        result += 100u * tmp;

        tmp = milliTime / 1000u;
        milliTime -= tmp * 1000u;
        return (result+tmp);
    }

    inline unsigned makeTimeHHMMSS(unsigned *milliTime) {
        unsigned result = 0, tmp = 0;;

        tmp = *milliTime / 3600000u;
        *milliTime -= tmp * 3600000u;
        result += 10000u * tmp;

        tmp = *milliTime / 60000u;
        *milliTime -= tmp * 60000u;
        result += 100u * tmp;

        tmp = *milliTime / 1000u;
        *milliTime -= tmp * 1000u;
        return (result+tmp);
    }
    template <class T>
    inline void helpRange(const std::vector<T>& x, double * range) {
        range[0] = DBL_MAX; range[1] = -DBL_MAX;
        for (typename std::vector<T>::const_iterator it = x.begin();
             it != x.end(); ++it) {
            if (*it < range[0]) range[0] = *it;
            if (*it > range[1]) range[1] = *it;
        }
    }

    static const char * styles[] =  {
        "points ls 1", "lines ls 1"
        , "filledcurve y1=0 ls 1"
        , "steps ls 1"
        , "boxes fs solid 1"
        , "impulses ls 1"
        , "errorbars ls 1"
    };
}

bool mrutils::Gnuplot::init(const char * term, const char * file) {
    init_ = true; valid = false;

    // check dispay on unix systems
    #if ( defined(unix) || defined(__unix) || defined(__unix__) ) && !defined(__APPLE__)
        if (getenv("DISPLAY") == NULL) return bye("can't open display");
    #endif

    char path[1024], *p;
    if (!(p = getPath(path))) return bye("can't find executable");

    valid = true; // set here so the commands below will run

    #if defined(__APPLE__)
        // image rendering is MUCH faster than aqua and ~2x faster than x11
        if (term == NULL) term = "png";
    #endif

    if (term != NULL) {
        if (0==stricmp(term,"jpg") || 0==stricmp(term,"jpeg")) {
            if (file != NULL && *file != 0) {
                strcpy(strcpy(p, " > "),file);
            } else {
                strcpy(p, " | /Users/firmion/bin/img --persist -f -");
                //strcpy(p, " | xv -");
            }
            gnucmd = MR_POPEN(path,"w");
            if (!gnucmd) return bye("unable to open pipe to gnuplot");
            //cmd("set terminal jpeg enhanced font \"/usr/x11/lib/x11/fonts/TTF/luxirr.ttf\" 10 size 800,480");
            cmd("set terminal jpeg enhanced font \"/Library/Fonts/Arial.ttf\" 9 size 800,480");
            //cmd("set terminal pngcairo dashed font \"/usr/x11/lib/x11/fonts/TTF/luxirr.ttf, 10\" size 800,480");
            //cmd("set terminal svg font \"/usr/x11/lib/x11/fonts/TTF/luxirr.ttf\" fsize 10");
            isImage = true;
        } else if (0==stricmp(term,"png")) {
            if (file != NULL && *file != 0) {
                strcpy(strcpy(p, " > "),file);
            } else {
                //strcpy(p, " | /Users/firmion/bin/img -f -");
                strcpy(p, " | /Users/firmion/bin/img --persist -f -");
                //strcpy(p, " | xv -");
            }
            gnucmd = MR_POPEN(path,"w");
            if (!gnucmd) return bye("unable to open pipe to gnuplot");
            //cmd("set terminal pngcairo dashed font \"/usr/x11/lib/x11/fonts/TTF/luxirr.ttf, 10\" size 800,480");
            
            // pngcairo doesn't seem to exist anymore
            //cmd("set terminal pngcairo enhanced dashed font \"Arial, 9\" size 800,480");

            cmd("set terminal png enhanced font \"/Library/Fonts/Arial.ttf\" 9 size 800,480");
            isImage = true;
        } else if (0==stricmp(term,"stdout")) {
            gnucmd = stdout;
        } else if (0==stricmp(term,"stderr")) {
            gnucmd = stderr;
        } else {
            isImage = false;
            strcpy(p, " -persist");
            gnucmd = MR_POPEN(path,"w");
            if (!gnucmd) return bye("unable to open pipe to gnuplot");
            fprintf(gnucmd,"set terminal %s\n",term);
            fflush(gnucmd);
        }
    } else {
        strcpy(p, " -persist");
        gnucmd = MR_POPEN(path,"w");
        if (!gnucmd) return bye("unable to open pipe to gnuplot");

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            cmd("set terminal windows enhanced");
        #elif defined(__APPLE__)
            cmd("set terminal aqua dashed 1");
        #elif ( defined(unix) || defined(__unix) || defined(__unix__) ) && !defined(__APPLE__)
            cmd("set terminal x11 enhanced");
        #endif
    }

    cmd("set lmargin 9");
    cmd("set rmargin 9");
    cmd("set tmargin 2");
    cmd("set bmargin 2");
    cmd("set border 0");

    points();

    return true;
}

char * mrutils::Gnuplot::getPath(char * path) {
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        static const char * execName = "pgnuplot.exe";
        static const char * pathGuess = "C:/gnuplot/bin/";
    #elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__)
        static const char * execName = "gnuplot";
        static const char * pathGuess = "/usr/local/bin/";
    #endif

    char * p = strcpy(path, pathGuess);
    *p++ = '/'; p = strcpy(p, execName);

    if (fileExists(path)) return p;

    const char *pathEnv = getenv("PATH"), *np = pathEnv;

    if (pathEnv == NULL) return byeNull("environment PATH is null...");

    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        char pathSep = ';';
    #else 
        char pathSep = ':';
    #endif

    while (*pathEnv) {
        np = strchr(pathEnv, pathSep);
        if (np == NULL) np = &pathEnv[strlen(pathEnv)];
        p = strncpy(path, pathEnv, np - pathEnv);
        *p++ = '/'; p = strcpy(p, execName);
        if (fileExists(path)) return p;
        if (*np == '\0') break;
        pathEnv = ++np;
    }

    return byeNull("Can't find executable in PATH or default search");
}

void mrutils::Gnuplot::rangeFinish(axis axis) {
    if (!valid && (init_ || !init())) return;

    double padding;

    switch (axis) {
        case AXIS_X:
            //padding = (xRange[1] - xRange[0]) * 0.05;
            //xRange[0] -= padding; xRange[1] += padding;

            switch (xType) {
                case X_DATE:
                case X_EPOCH:
                    fprintf( gnucmd, "set xrange[\"%u\":\"%u\"]\n", (unsigned)xRange[0], (unsigned)xRange[1] );
                    break;
                case X_MILLIS: 
                    fprintf( gnucmd, "set xrange[\"%06u\":\"%06u\"]\n"
                       ,makeTimeHHMMSS((unsigned)xRange[0])
                       ,makeTimeHHMMSS((unsigned)xRange[1]));
                    break;
                default:
                    fprintf( gnucmd, "set xrange[%f:%f]\n", xRange[0], xRange[1] );
                    break;
            }

            setRanges[axis] = true;
            break;

        case AXIS_Y:
            if (zeroY[0]) {
                if (yRange[0] > 0) yRange[0] = 0;
                if (yRange[1] < 0) yRange[1] = 0;
            }

            padding = (yRange[1] - yRange[0]) * 0.05;
            yRange[0] -= padding; yRange[1] += padding;
            fprintf( gnucmd, "set yrange[%f:%f]\n", yRange[0], yRange[1] );

            setRanges[axis] = true;
            break;

        case AXIS_Y2:
            if (zeroY[1]) {
                if (y2Range[0] > 0) y2Range[0] = 0;
                if (y2Range[1] < 0) y2Range[1] = 0;
            }

            padding = (y2Range[1] - y2Range[0]) * 0.05;
            y2Range[0] -= padding; y2Range[1] += padding;
            fprintf( gnucmd, "set y2range[%f:%f]\n", y2Range[0], y2Range[1] );

            setRanges[axis] = true;
            break;

    }
}

bool mrutils::Gnuplot::fadeOldCurves(const double frac) {
    if (!setRanges[AXIS_X] || !setRanges[AXIS_Y]) return false;
    fprintf(gnucmd,"plot '-' w filledcurves  lt rgb \"white\" fs fill transparent solid %f noborder\n",frac);
    fprintf(gnucmd, "%f %f\n%f %f\n%f %f\n%f %f\n%f %f\ne\n"
        ,xRange[0],yRange[0]
        ,xRange[1],yRange[0]
        ,xRange[1],yRange[1]
        ,xRange[0],yRange[1]
        ,xRange[0],yRange[0]);
    fflush(gnucmd);
    return true;
}

bool mrutils::Gnuplot::hline(double y) {
    PLOT_START cmd("unset key");
    fprintf(gnucmd, "plot %f with %s axis x1y%d\n"
        ,y, styles[nextPlot.style], nextPlot.yAxis);
    fflush(gnucmd);
    PLOT_END
    return true;
}

bool mrutils::Gnuplot::vline(double x) {
    PLOT_START cmd("unset key");
    switch (xType) {
        case X_DATE:
        case X_EPOCH:
        case X_PLAIN:
            fprintf(gnucmd, "plot '-' with %s\n", styles[nextPlot.style]);
            fprintf(gnucmd, "%f %f\n", x, yRange[0]);
            fprintf(gnucmd, "%f %f\n", x, yRange[1]);
            fputs("e\n",gnucmd);
            break;
        case X_MILLIS:
        {
            unsigned millis = (unsigned)x;
            unsigned time = makeTimeHHMMSS(&millis);
            fprintf(gnucmd, "plot '-' using (timecolumn(1)+$2/1000):3 with %s\n"
                ,styles[nextPlot.style]);
            fprintf(gnucmd, "%06u %u %f\n", time, millis, yRange[0]);
            fprintf(gnucmd, "%06u %u %f\n", time, millis, yRange[1]);
            fputs("e\n",gnucmd);
        } break;
    } 
    fflush(gnucmd);
    PLOT_END
    return true;
}

bool mrutils::Gnuplot::slope(double alpha, double beta) {
    PLOT_START cmd("unset key");
    fprintf(gnucmd, "plot %f + %f * x with %s axis x1y%d\n"
        ,alpha, beta, styles[nextPlot.style], nextPlot.yAxis);
    fflush(gnucmd);
    PLOT_END
    return true;
}

bool mrutils::Gnuplot::plot(fastdelegate::FastDelegate1<double,double> func, double from, double to, int samples) {
    PLOT_FUNC(func(x))
    return true;
}

bool mrutils::Gnuplot::plot(fastdelegate::FastDelegate2<double,void*,double> func, void * data, double from, double to, int samples) {
    PLOT_FUNC(func(x,data))
    return true;
}

bool mrutils::Gnuplot::plot(fastdelegate::FastDelegate2<double,double,double> func, double p1, double from, double to, int samples) {
    PLOT_FUNC(func(x,p1))
    return true;
}

bool mrutils::Gnuplot::plot(fastdelegate::FastDelegate3<double,double,double,double> func, double p1, double p2, double from, double to, int samples) {
    PLOT_FUNC(func(x,p1,p2))
    return true;
}

bool mrutils::Gnuplot::plot(fastdelegate::FastDelegate6<double,double,double,double,double,double,double> func
    ,double p1, double p2, double p3, double p4, double p5
    ,double from, double to, int samples) {
    PLOT_FUNC(func(x,p1,p2,p3,p4,p5))
    return true;
}

template <class Tx, class T>
bool mrutils::Gnuplot::diff(const std::vector<Tx> &x, const std::vector<T> & y1, const std::vector<T> & y2, const char * labelGT, const char * labelLT) {
    if (x.size() <= 1 || y1.size() <= 1 || y2.size() <= 1) return false;
    PLOT_START if (labelGT[0]==0&&labelLT[0]==0) cmd("unset key"); 
               else { cmd("set key"); KEY_HEIGHT nextPlot.keyHeight += (int)(labelGT[0]==0)+(int)(labelLT[0]==0); }

    if (!setRanges[AXIS_X]) { 
        helpRange(x, xRange);
        rangeFinish(AXIS_X);
    } 

    // have to provide the same data twice...
    typename std::vector<Tx>::const_iterator itx;
    for (int i = 0; i < 2; ++i) {
        itx = x.begin();
        switch (xType) {
            case X_DATE:
            case X_EPOCH:
                fprintf(gnucmd, "plot '-' using 1:2:3 with filledcu above fs solid 1 lt rgb \"green\" title \"%s\", '-' using 1:2:3 with filledcu below fs solid 1 lt rgb \"red\" title \"%s\"\n"
                    ,labelGT, labelLT);
                for (typename std::vector<T>::const_iterator it1 = y1.begin()
                    ,it2 = y2.begin(); it1 != y1.end() && it2 != y2.end() && itx != x.end()
                    ;++it1, ++it2, ++itx) 
                    fprintf( gnucmd, "%d %f %f\n", (int)*itx, (double)*it1, (double)*it2 );
                break;
            case X_MILLIS:
                fprintf(gnucmd, "plot '-' using (timecolumn(1)+$2/1000):3:4 with filledcu above fs solid 1 lt rgb \"green\" title \"%s\", '-' using (timecolumn(1)+$2/1000):3:4 with filledcu below fs solid 1 lt rgb \"red\" title \"%s\"\n"
                    ,labelGT, labelLT);
                for (typename std::vector<T>::const_iterator it1 = y1.begin()
                    ,it2 = y2.begin(); it1 != y1.end() && it2 != y2.end() && itx != x.end()
                    ;++it1, ++it2, ++itx) {
                    unsigned millis = (unsigned)*itx;
                    unsigned time = makeTimeHHMMSS(&millis);
                    fprintf( gnucmd, "%06u %u %f %f\n", time, millis, (double)*it1, (double) *it2);
                }
                break;
            case X_PLAIN:
                fprintf(gnucmd, "plot '-' using 1:2:3 with filledcu above fs solid 1 lt rgb \"green\" title \"%s\", '-' using 1:2:3 with filledcu below fs solid 1 lt rgb \"red\" title \"%s\"\n"
                    ,labelGT, labelLT);
                for (typename std::vector<T>::const_iterator it1 = y1.begin()
                    ,it2 = y2.begin(); it1 != y1.end() && it2 != y2.end() && itx != x.end()
                    ;++it1, ++it2, ++itx)
                    fprintf( gnucmd, "%f %f %f\n", (double)*itx, (double)*it1, (double)*it2);
                break;
        }
        fputs("e\n",gnucmd); 
    }
    fflush(gnucmd);

    PLOT_END
    return true;
}

template <class T>
bool mrutils::Gnuplot::diff(const std::vector<T> & y1, const std::vector<T> & y2, const char * labelGT, const char * labelLT) {
    if (y1.size() <= 1 || y2.size() <= 1) return false;
    PLOT_START if (labelGT[0]==0&&labelLT[0]==0) cmd("unset key"); 
               else { cmd("set key"); KEY_HEIGHT nextPlot.keyHeight += (int)(labelGT[0]==0)+(int)(labelLT[0]==0); }

    if (!setRanges[AXIS_X]) { 
        xRange[0] = 0; xRange[1] = MIN_(y1.size(),y2.size())-1;
        rangeFinish(AXIS_X);
    }

    if (!setRanges[nextPlot.yAxis]) {
        switch (nextPlot.yAxis) {
            case 1:
                helpRange(y1, yRange); 
                helpRange(y2, yRange); 
                rangeFinish(AXIS_Y); 
                break;
            case 2:
                helpRange(y1, y2Range); 
                helpRange(y2, y2Range); 
                rangeFinish(AXIS_Y2); 
                break;
        }
    }

    fprintf(gnucmd, "plot '-' using 0:1:2 with filledcu above fs solid 1 lt rgb \"green\" axis x1y%d title \"%s\", '-' using 0:1:2 with filledcu below fs solid 1 lt rgb \"red\" axis x1y%d title \"%s\"\n"
        ,nextPlot.yAxis, labelGT, nextPlot.yAxis, labelLT);

    // have to provide the same data twice...
    for (typename std::vector<T>::const_iterator it1 = y1.begin()
        ,it2 = y2.begin(); it1 != y1.end() && it2 != y2.end()
        ;++it1, ++it2) fprintf(gnucmd, "%f %f\n",(double)*it1, (double)*it2);
    fputs("e\n",gnucmd); 
    for (typename std::vector<T>::const_iterator it1 = y1.begin()
        ,it2 = y2.begin(); it1 != y1.end() && it2 != y2.end()
        ;++it1, ++it2) fprintf(gnucmd, "%f %f\n",(double)*it1, (double)*it2);
    fputs("e\n",gnucmd); fflush(gnucmd);

    PLOT_END
    return true;
}

template <class T>
bool mrutils::Gnuplot::plot(const std::vector<T> & x, const char * label) {
    if (x.size() <= 1) return false;
    PLOT_START if (label[0]==0) cmd("unset key"); 
               else { cmd("set key"); KEY_HEIGHT ++nextPlot.keyHeight; }

    if (!setRanges[AXIS_X]) { 
        xRange[0] = 0; xRange[1] = x.size()-1;
        rangeFinish(AXIS_X);
    }

    if (!setRanges[nextPlot.yAxis]) {
        switch (nextPlot.yAxis) {
            case 1:
                helpRange(x,yRange);
                rangeFinish(AXIS_Y);
                break;
            case 2:
                helpRange(x,y2Range);
                rangeFinish(AXIS_Y2);
                break;
        }
    }

    fprintf(gnucmd, "plot '-' using 0:1 with %s axis x1y%d title \"%s\"\n"
        ,styles[nextPlot.style], nextPlot.yAxis, label);

    for (typename std::vector<T>::const_iterator it = x.begin();
            it != x.end(); ++it) fprintf( gnucmd, "%f\n", (double)*it);
    fputs( "e\n", gnucmd ); fflush( gnucmd );

    PLOT_END
    return true;
}

template <class T1, class T2>
bool mrutils::Gnuplot::plot(const std::vector<T1> & x, const std::vector<T2> & y, const char * label) {
    if (x.size() <= 1 || y.size() <= 1) return false;
    PLOT_START if (label[0]==0) cmd("unset key"); 
               else { cmd("set key"); KEY_HEIGHT ++nextPlot.keyHeight; }

    if (!setRanges[AXIS_X]) { 
        helpRange(x, xRange); 
        rangeFinish(AXIS_X); 
    } 

    if (!setRanges[nextPlot.yAxis]) {
        switch (nextPlot.yAxis) {
            case 1:
                helpRange(y,yRange);
                rangeFinish(AXIS_Y);
                break;
            case 2:
                helpRange(y,y2Range);
                rangeFinish(AXIS_Y2);
                break;
        }
    }

    typename std::vector<T1>::const_iterator itx = x.begin();
    typename std::vector<T2>::const_iterator ity = y.begin();
    switch (xType) {
        case X_DATE:
        case X_EPOCH:
            fprintf(gnucmd, "plot '-' using 1:2 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && ity != y.end(); ++itx, ++ity) 
                if (*ity == *ity) fprintf( gnucmd, "%d %f\n", (int)*itx, (double)*ity);
            break;
        case X_MILLIS:
            fprintf(gnucmd, "plot '-' using (timecolumn(1)+$2/1000):3 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && ity != y.end(); ++itx, ++ity) {
                if (*ity == *ity) {
                    unsigned millis = (unsigned)*itx;
                    unsigned time = makeTimeHHMMSS(&millis);
                    fprintf( gnucmd, "%06u %u %f\n", time, millis, (double)*ity);
                }
            }
            break;
        case X_PLAIN:
            fprintf(gnucmd, "plot '-' using 1:2 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && ity != y.end(); ++itx, ++ity) 
                if (*ity == *ity) fprintf( gnucmd, "%f %f\n", (double)*itx, (double)*ity);
            break;
    }
    fputs("e\n", gnucmd); fflush(gnucmd);

    PLOT_END
    return true;
}

template <class T1, class T2>
bool mrutils::Gnuplot::plotErrors(const std::vector<T1> & x, const std::vector<T2> & lower, const std::vector<T2>& upper, const std::vector<T2>& mid, const char * label) {
    if (x.size() <= 1 || lower.size() <= 1 || upper.size() <= 1 || mid.size() <= 1) return false;
    PLOT_START if (label[0]==0) cmd("unset key"); 
               else { cmd("set key"); KEY_HEIGHT ++nextPlot.keyHeight; }

    if (!setRanges[AXIS_X]) { 
        helpRange(x, xRange); 
        rangeFinish(AXIS_X); 
    } 

    if (!setRanges[nextPlot.yAxis]) {
        switch (nextPlot.yAxis) {
            case 1:
                setYRange(&lower,&upper,&mid,NULL);
                break;
            case 2:
                setY2Range(&lower,&upper,&mid,NULL);
                break;
        }
    }

    typename std::vector<T1>::const_iterator itx = x.begin();
    typename std::vector<T2>::const_iterator itl = lower.begin();
    typename std::vector<T2>::const_iterator itu = upper.begin();
    typename std::vector<T2>::const_iterator itm = mid.begin();
    switch (xType) {
        case X_DATE:
        case X_EPOCH:
            fprintf(gnucmd, "plot '-' using 1:2:3:4 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && itl != lower.end() && itu != upper.end() && itm != mid.end(); ++itx, ++itl, ++itu, ++itm) 
                fprintf( gnucmd, "%d %f %f %f\n", (int)*itx, (double)*itm,(double)*itl, (double)*itu);
            break;
        case X_MILLIS:
            fprintf(gnucmd, "plot '-' using (timecolumn(1)+$2/1000):3:4:5 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && itl != lower.end() && itu != upper.end() && itm != mid.end(); ++itx, ++itl, ++itu, ++itm) {
                unsigned millis = (unsigned)*itx;
                unsigned time = makeTimeHHMMSS(&millis);
                fprintf( gnucmd, "%06u %u %f %f %f\n", time, millis, (double)*itm,(double)*itl, (double)*itu);
            }
            break;
        case X_PLAIN:
            fprintf(gnucmd, "plot '-' using 1:2:3:4 with %s axis x1y%d title \"%s\"\n"
                ,styles[nextPlot.style], nextPlot.yAxis, label);
            for (; itx != x.end() && itl != lower.end() && itu != upper.end() && itm != mid.end(); ++itx, ++itl, ++itu, ++itm) 
                fprintf( gnucmd, "%f %f %f %f\n", (double)*itx, (double)*itm,(double)*itl, (double)*itu);
            break;
    }
    fputs("e\n", gnucmd); fflush(gnucmd);

    PLOT_END
    return true;
}

#ifdef BUILD_DLL
template _API_ bool mrutils::Gnuplot::diff(const std::vector<int>& x, const std::vector<int>& y1, const std::vector<int>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<int>& x, const std::vector<double>& y1, const std::vector<double>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<int>& x, const std::vector<unsigned>& y1, const std::vector<unsigned>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<double>& x, const std::vector<int>& y1, const std::vector<int>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<double>& x, const std::vector<double>& y1, const std::vector<double>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<double>& x, const std::vector<unsigned>& y1, const std::vector<unsigned>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<unsigned>& x, const std::vector<int>& y1, const std::vector<int>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<unsigned>& x, const std::vector<double>& y1, const std::vector<double>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<unsigned>& x, const std::vector<unsigned>& y1, const std::vector<unsigned>& y2, const char * labelGT, const char * labelLT);

template _API_ bool mrutils::Gnuplot::diff(const std::vector<int>& y1, const std::vector<int>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<double>& y1, const std::vector<double>& y2, const char * labelGT, const char * labelLT);
template _API_ bool mrutils::Gnuplot::diff(const std::vector<unsigned>& y1, const std::vector<unsigned>& y2, const char * labelGT, const char * labelLT);

template _API_ bool mrutils::Gnuplot::plot(const std::vector<int> & x, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<double> & x, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<unsigned> & x, const char * label);

template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<int> &x, const std::vector<int> &lower, const std::vector<int>& upper, const std::vector<int>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<int> &x, const std::vector<double> &lower, const std::vector<double>& upper, const std::vector<double>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<int> &x, const std::vector<unsigned> &lower, const std::vector<unsigned>& upper, const std::vector<unsigned>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<double> &x, const std::vector<int> &lower, const std::vector<int>& upper, const std::vector<int>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<double> &x, const std::vector<double> &lower, const std::vector<double>& upper, const std::vector<double>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<double> &x, const std::vector<unsigned> &lower, const std::vector<unsigned>& upper, const std::vector<unsigned>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<unsigned> &x, const std::vector<int> &lower, const std::vector<int>& upper, const std::vector<int>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<unsigned> &x, const std::vector<double> &lower, const std::vector<double>& upper, const std::vector<double>& mid, const char * label);
template _API_ bool mrutils::Gnuplot::plotErrors(const std::vector<unsigned> &x, const std::vector<unsigned> &lower, const std::vector<unsigned>& upper, const std::vector<unsigned>& mid, const char * label);

template _API_ bool mrutils::Gnuplot::plot(const std::vector<int> &x, const std::vector<int> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<int> &x, const std::vector<double> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<int> &x, const std::vector<unsigned> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<double> &x, const std::vector<int> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<double> &x, const std::vector<double> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<double> &x, const std::vector<unsigned> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<unsigned> &x, const std::vector<int> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<unsigned> &x, const std::vector<double> &y, const char * label);
template _API_ bool mrutils::Gnuplot::plot(const std::vector<unsigned> &x, const std::vector<unsigned> &y, const char * label);
#endif
