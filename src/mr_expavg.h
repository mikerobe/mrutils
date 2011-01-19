#ifndef _MR_CPPLIB_EXPAVG_H
#define _MR_CPPLIB_EXPAVG_H

#include "mr_exports.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "float.h"
#include "mr_files.h"
#include "mr_gnuplot.h"
#include "mr_expvar.h"

namespace mrutils {

class _API_ ExpAverage {
    public:
        ExpAverage(int length=40
            ,int header=4, int minPoints=40
            ,unsigned updateMillis=5000
            ,const char * name = "");

        ~ExpAverage() {
            if (stream != NULL) fclose(stream);
            if (g != NULL) delete g;
        }

    public:
       /******************
        * Init
        ******************/

        inline void rebuild(int length,int header,int minPoints,unsigned updateMillis, const char * name="") {
            this->length = length;
            this->theta = pow(0.5,1./length);
            this->header = header;
            this->minPoints = minPoints;
            this->interval = updateMillis;
            if (name[0] != '\0') this->name = name;
            var.rebuild(length,header,minPoints,0);
        }

        /**
          * Way of setting the theta used directly instead of using length
          */
        inline void setTheta(double theta)
        { this->theta = theta; }


        inline void clear(bool includePlot = false) {
            lastUpdate = 0;
            result = 0; count = 0;
            var.clear();

            if (includePlot) {
                timestamps.clear();
                results.clear();
                xs.clear(); stdevs.clear();
            }
        }

    public:
       /******************************
        * Settings
        ******************************/

        inline void setStoreData(bool tf = true) { 
            storeData = tf; 
            timestamps.reserve(10000);
            xs.reserve(10000);
            results.reserve(10000);
        }

        inline void setUseVar(bool tf = true)
        { this->useVar = tf; }

        inline void setPlotOnUpdate(bool tf = true) 
        { plotOnUpdate = tf; }

        inline void setName(const char * name) { this->name = name; }

    public:
       /******************************
        * Main methods
        ******************************/

        inline bool canAdd(unsigned now) const
        { return (now >= lastUpdate + interval); }

        bool add(const unsigned now, const double x);

        inline void addFast(const double x) {
            result = count < header 
                     ?result*(count/(count+1.)) + x/(count+1) 
                     :result*theta + x*(1-theta);
            ++count;
        }

    public:
        /***********************************
         * Data
         ***********************************/

        inline const char * getName() const { return name.c_str(); }

        std::string name;
        unsigned lastUpdate;
        double theta, result;
        int length, count, minPoints, header;

        bool good;
        
        mrutils::ExpVar var;

    public:
       /******************************
        * Utils
        ******************************/
       
        bool monitorFile(int fd,const char * path);
        inline void monitorFile(const char * path) {
            for (;;) {
                for (;!mrutils::fileExists(path);mrutils::sleep(100)){}
                int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
                if (fd < 0) break;
                monitorFile(fd,path);
                MR_CLOSE(fd);
            }
        }
        inline void monitorFileThread(void * path) 
        { monitorFile((const char *)path); }


        inline bool streamTo(const char * path, bool append=false) {
            if (stream != NULL) fclose(stream);
            stream = fopen(path, (append?"a":"w"));
            return stream != NULL;
        }

        void print(std::ostream & os);
        void plot();

        inline friend std::ostream& operator << (std::ostream & os, const ExpAverage & e) {
            return os << "ExpAverage (" << e.length << "; count " << e.count << 
                "; minPoints " << e.minPoints << "; theta " << e.theta << "; result " << e.result << ")";
        }

    private:
        // to add line of text read from file
        void addLine(char * line);

    private:
        bool plotOnUpdate;

        bool storeData, useVar;
        unsigned interval;

        std::vector<unsigned> timestamps;
        std::vector<double> xs, results, stdevs;

        FILE *stream;

        mrutils::Gnuplot* g;
};
}

#endif
