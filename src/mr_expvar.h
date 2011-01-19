#ifndef _MR_CPPLIB_EXPVAR_H
#define _MR_CPPLIB_EXPVAR_H

#include "mr_exports.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "float.h"
#include "mr_numerical.h"

namespace mrutils {

class _API_ ExpVar {
    public:
        ExpVar(int length=40, int header=4, int minPoints=40, unsigned updateMillis=0, const char * name = "");

    public:
       /************
        * Init
        ************/

        inline void rebuild(int length,int header,int minPoints,unsigned updateMillis=0, const char * name="") {
            this->length = length;
            this->t = pow(1./length,1./length);
            this->header = header;
            this->minPoints = minPoints;
            this->interval = updateMillis;
            if (name[0] != 0) this->name = name;
        }

    public:
       /******************
        * Main functions
        ******************/

        bool addMult(double x, int times, unsigned now = 0);

        inline bool add(const unsigned now, double x) {
            if (now < lastUpdate + interval) return false;
            lastUpdate = now;

            Sx  = n < header 
                  ?(Sx*n+x)/(n+1) 
                  :(1-t)*x+t*Sx;

            x *= x;

            Sxx = n < header 
                  ?(Sxx*n+x)/(n+1) 
                  :(1-t)*x+t*Sxx;

            ++n;

            good = n > minPoints;
            return true;
        }

        void clear() {
            lastUpdate = 0;
            n = 0; Sxx = Sx = 0;
        }

    public:

        inline double var()   const { return (Sxx - Sx*Sx); }
        inline double stdev() const { return sqrt(Sxx - Sx*Sx); }

        std::string name;
        unsigned lastUpdate;
        int header, n;

        bool good;

    private:
        double Sx, Sxx;
        unsigned interval;
        double t;
        int length;
        int minPoints;


};

}

#endif
