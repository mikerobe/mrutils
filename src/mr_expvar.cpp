#include "mr_expvar.h"

mrutils::ExpVar::ExpVar(int length, int header, int minPoints, unsigned updateMillis, const char * name)
: name(name)
 ,lastUpdate(0)
 ,header(header)
 ,n(0), good(false)
 ,Sx(0), Sxx(0)
 ,interval(updateMillis)
 ,t(pow(1./length,1./length))
 ,length(length)
 ,minPoints(minPoints)
{}

bool mrutils::ExpVar::addMult(double x, int times, unsigned now) {
    if (now < lastUpdate + interval) return false;
    lastUpdate = now;

    if (n < header) {
        int add = MIN_(header - n, times);
        Sx  = (Sx*n  + add*x  )/(n+add);
        Sxx = (Sxx*n + add*x*x)/(n+add);
        times -= add;
        n += add;
    }

    if (times > 0) {
        double tn = pow(t,times);
        Sxx = (1-tn)*x*x + tn*Sxx;
        Sx  = (1-tn)*x   + tn*Sx ;
        n += times;
    }

    good = n > minPoints;

    return true;
}
