#ifndef _MR_BETA_H
#define _MR_BETA_H

#include "mr_exports.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include "float.h"
#include <stdio.h>
#include "mr_gnuplot.h"
#include "mr_threads.h"
#include "mr_files.h"

namespace mrutils {

class _API_ Beta {

    public:
        Beta(const char * name = "", const char * xname = "", const char * yname="", double betaConstraint = DBL_MAX);
        ~Beta();

        void clear();

    public:
       /******************************
        * Settings
        ******************************/

        inline void setStoreData(bool tf = true) 
        { storeData = tf; }

        inline void setPlotOnUpdate(bool tf = true) 
        { plotOnUpdate = tf; }

        inline void setName(const char * name)
        { this->name = name; }

        /**
         * If set, rather than using the actual x & y observations, 
         * will compute the change each time over the previous
         * set of observations. 
         * */
        inline void setUseChange(bool tf = true) { useChange = true; }
        inline void setMinTimeBetween(unsigned time) { timeBetween = time; }
        inline void setInterval(unsigned time) { timeBetween = time; }

    public:
       /******************************
        * Main methods
        ******************************/

        void add(double x, double y, unsigned time = 0, double weight = 1);
        bool add(const char * path, char delim='\t');

    public:
       /******************************
        * Data
        ******************************/

        inline double sigmaX() { return sqrt((Sxx - Sx*Sx/n)/(n-1)); }
        inline double sigmaY() { return sqrt((Syy - Sy*Sy/n)/(n-1)); }
        inline double betaStdErr() {
            return sqrt(n*RSS / (n-2) / (n*Sxx-Sx*Sx));
        }

        inline double Y(double x) { return (alpha + beta * x); }
        inline double YError(double x) { return sqrt(error(x,false)); }
        inline double forecastErr(double x) { return sqrt(error(x,true)); }
        inline double forecastVar(double x) { return error(x,true); }

        double alpha, beta; 
        double R2, RSS;
        double lastX, lastY;
        int n;

    public:
       /******************************
        * Utils
        ******************************/

        void printData(std::ostream& os = std::cout);
        void printSummary(std::ostream& os = std::cout);
        void plotSummary();
        inline void plot() { plotSummary(); }

        // reads from the file & updates beta as new points are added
        bool monitorFile(int fd, char delim = '\t');
        inline void monitorFile(const char * path, char delim='\t') {
            for (;;) {
                // wait for file to be created
                while (!mrutils::fileExists(path)) mrutils::sleep(100);

                int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
                if (fd < 0) break; 
                monitorFile(fd, delim);
                MR_CLOSE(fd);
            }
        }

        /**
          * NOTE: this function will call delete on the data
          * so this should point to some allocated memory
          */
        inline void monitorFileThread(void * data) { 
            monitordata_t * d = (monitordata_t*)data;
            monitorFile(d->path.c_str(),d->delim);
            delete d;
        } struct monitordata_t { std::string path; char delim; };

        inline bool streamTo(const char * path, bool append=true) {
            if (stream != NULL) fclose(stream);
            stream = fopen(path, (append?"a":"w"));
            return stream != NULL;
        }

        inline void flushStream() {
            if (stream == NULL) return;
            fflush(stream);
        }

    private:
        inline double error(double x, bool forecastError) {
            if (n <= 2 ) return DBL_MAX;
            return RSS/(n-2) * ( (forecastError?1:0) + 1/n + (x-Sx/n)*(x-Sx/n)/(Sxx-Sx*Sx/n));
        }

        // to add line of text read from file
        void addLine(int &columns, char * line, char delim);

    private:
        std::string name, xname, yname;

        unsigned timeBetween, lastTime;
        bool constrained, storeData, useChange, hasFirst;
        bool plotOnUpdate;

        double Sxy, Sxx, Syy, Sx, Sy, Sw;

        std::vector<double> xs, ys;
        std::vector<unsigned> times;
        FILE *stream;

        mrutils::Gnuplot *g;
};

}

#endif
