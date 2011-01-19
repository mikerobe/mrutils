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
        inline void rebuild(int length,int header,int minPoints,unsigned updateMillis, const char * name="") {
            setLength(length);
            this->header_ = header;
            this->minPoints_ = minPoints;
            this->updateMillis_ = updateMillis;
            if (name[0] != '\0') this->name_ = name;
        }


        inline void clear(bool includePlot = false) {
            lastTime_ = 0;
            result_ = 0;
            cached_ = 0;
            count_ = 0;

            if (includePlot) {
                timestamps.clear();
                results.clear();
                xs.clear();
            }
        }


    public:
       /******************************
        * Settings
        ******************************/

        inline void setStoreData(bool tf = true) 
        { storeData = tf; }

        inline void setPlotOnUpdate(bool tf = true) 
        { plotOnUpdate = tf; }

        inline void setLength(int length) { 
            length_ = length; 
            theta_ = pow(1/static_cast<double>(length),1/static_cast<double>(length));
        }

        inline void setHeader(int header) { header_ = header; }
        inline void setMinPoints(int minPoints) { minPoints_ = minPoints; }
        inline void setUpdateMillis(unsigned millis) { updateMillis_ = millis; }
        inline void setName(const char * name) { name_ = name; }

    public:
       /******************************
        * Main methods
        ******************************/

        inline bool canAdd(unsigned now) 
        { return (now > lastTime_ + updateMillis_); }

        bool add(unsigned now, double x);
       
        // reads from the file & updates beta as new points are added
        bool monitor(int fd);
        inline void monitor(const char * path) {
            for (;;) {
                // wait for file to be created
                while (!mrutils::fileExists(path)) mrutils::sleep(100);

                int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
                if (fd < 0) break;
                monitor(fd);
                MR_CLOSE(fd);
            }
        }
        inline void monitorThread(void * path) 
        { monitor((const char *)path); }

    public:
        /***********************************
         * Data
         ***********************************/

        inline bool good() const { return (count_ > minPoints_); }
        inline unsigned lastUpdate() const { return lastTime_; }
        inline int count() const { return count_; }
        inline double result() const { return result_; }
        inline double cached() const { return cached_; }
        inline double theta() const { return theta_; }
        inline int length() const { return length_; }
        inline const char * name() const { return name_.c_str(); }


    public:
       /******************************
        * Utils
        ******************************/

        inline bool streamTo(const char * path, bool append=false) {
            if (stream != NULL) fclose(stream);
            stream = fopen(path, (append?"a":"w"));
            return stream != NULL;
        }

        void printSummary(std::ostream & os);
        void plotSummary();

        inline void plot()
        { plotSummary(); }

        inline friend std::ostream& operator << (std::ostream & os, const ExpAverage & e) {
            return os << "ExpAverage (" << e.length_ << "; count " << e.count_ << 
                "; minPoints " << e.minPoints_ << "; theta " << e.theta_ << "; result " << e.result_ << ")";
        }

    private:
        // to add line of text read from file
        void addLine(char * line);

    private:
        std::string name_;
        bool plotOnUpdate;

        bool storeData;
        unsigned lastTime_, updateMillis_;
        double theta_;
        double result_, cached_;
        int length_;
        int count_, minPoints_, header_;

        std::vector<unsigned> timestamps;
        std::vector<double> xs;
        std::vector<double> results;

        FILE *stream;

        mrutils::Gnuplot* g;
};
}

#endif
