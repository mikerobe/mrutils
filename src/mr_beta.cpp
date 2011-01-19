#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include <string>
#include <sstream>
#include "mr_beta.h"
#include <stdio.h>
#include "float.h"
#include "mr_numerical.h"
#include "mr_strutils.h"

using namespace mrutils;

Beta::Beta(const char* name, const char *xname, const char*yname)
: alpha(0)
 ,R2(0), RSS(0), lastX(0), lastY(0), n(0)
 ,g(NULL)
 ,name(name), xname(xname), yname(yname)
 ,timeBetween(0), lastTime(0)
 ,storeData(false)
 ,useChange(false), hasFirst(false)
 ,plotOnUpdate(false)
 ,Sxy(0), Sxx(0), Syy(0), Sx(0), Sy(0), Sw(0)
 ,stream(NULL)
{}

Beta::~Beta() {
    if (stream != NULL) fclose(stream);
    if (g != NULL) delete g;
}

void Beta::clear() {
    Sxy = Sxx = Syy = Sx = Sy = Sw = RSS = 0;
    lastX = lastY = 0;
    n = 0;
    lastTime = 0;

    xs.clear(); ys.clear();
}


void Beta::addLine(int &columns, char * buffer, char delim) {
    char * ptr = buffer;

    if (columns == 1) {
        // count columns
        while (NULL != (ptr = strchr(ptr,delim))) {
            ++columns; ++ptr;
        }
        ptr = buffer;
    }

    if (columns == 2) {
        *(ptr = strchr(ptr,delim))++ = '\0';
        add(atof(buffer),atof(ptr),0);
    } else {
        *(ptr = strchr(ptr,delim))++ = '\0';
        char * x = ptr;
        *(ptr = strchr(ptr,delim))++ = '\0';
        add(atof(x),atof(ptr),atoi(buffer));
    }
}

bool Beta::monitorFile(int fd, const char * path, char delim) {
    if (fd < 0) return false;
    bool useChange = this->useChange;
    this->useChange = false;

    const size_t bufSz = 1024;
    char buffer[bufSz]; int columns = 1;

    mrutils::Tail tail(fd, path, buffer, bufSz);

    if (plotOnUpdate) {
        plotOnUpdate = false;
        while (mrutils::readLine(fd, buffer, bufSz)) addLine(columns, buffer, delim);
        plotOnUpdate = true;
        plot();
    }

    while (tail.nextLine()) addLine(columns, tail.line, delim);

    this->useChange = useChange;
    this->hasFirst  = false;

    return true;
}

bool Beta::add(const char * path, char delim) {
    char buffer[128];
    int columns = 1;

    FILE *input = fopen(path,"r");
    if (input == NULL) return false;

    bool useChange = this->useChange;
    this->useChange = false;

    while (fgets(buffer,128,input)) 
        addLine(columns, buffer, delim);

    fclose(input);

    this->useChange = useChange;
    this->hasFirst  = false;

    return true;
}

void Beta::plot() {
    if (!storeData) return;
    if (xs.size() <= 2) return;

    if (g == NULL) {
        g = new mrutils::Gnuplot;
        g->setTerm("jpg");
    }

    g->clear();
    g->setNoZero();
    g->setGrid();
    
    {mrutils::stringstream ss; ss << name << "; R2 " << R2; 
    g->setLabels(ss.str().c_str(), xname.c_str(), yname.c_str());}
    g->plot(xs,ys);
    g->lines(2,"grey").slope(alpha,beta);
    if (plotOnUpdate) g->draw();
}

void Beta::printData(std::ostream& os) {
    if (n == 0 || !storeData) return;
    for (int i = 0; i < n; ++i) {
        if (timeBetween > 0) os << times[i] << "\t";
        os << xs[i] << "\t" << ys[i] << "\n";
    }

}
void Beta::print(std::ostream& os) {
    if (n == 0) {
        os << "Beta: n=0" << std::endl;
    } else if (n <= 2) {
        os << "Beta: 0<n<=2" << std::endl;
    } else {
        os << "Beta: " << name << "..." << std::endl;
        os << "\t" << "Y = " << alpha << " + " << beta << " * " << lastX << std::endl;
        os << "\t" << "Y(" << lastX << ") = " << Y(lastX) << std::endl;
        os << "\t" << "Beta Stdev:    " << betaStdErr()         << std::endl;
        os << "\t" << "Beta Sig(0):   " << 
            2*stats::normsdist(-ABS_(beta-0)/betaStdErr())
           << std::endl;
        os << "\t" << "Beta Sig(1):   " << 
            2*stats::normsdist(-ABS_(beta-1)/betaStdErr())
           << std::endl;
        os << "\t" << "YError:        " << YError(lastX)        << std::endl;
        os << "\t" << "forecasterr:   " << forecastErr(lastX)   << std::endl;
        os << "\t" << "n:             " << n                    << std::endl;
        os << "\t" << "R2:            " << R2                   << std::endl;
        os << "\t" << "correlation:   " << sqrt(R2)             << std::endl;
    }
}

void Beta::add(double x, double y, unsigned time, double w) {
    if (time < lastTime + timeBetween) return;
    lastTime = time;

    double px = x, py = y;

    if (useChange) {
        if (!hasFirst) {
            lastX = x; lastY = y; 
            hasFirst = true; return;
        }

        px = x / lastX - 1;
        py = y / lastY - 1;
    }

    lastX = x; lastY = y;
    if (storeData) { times.push_back(time); xs.push_back(px); ys.push_back(py); }
    if (stream != NULL) {
        if (timeBetween > 0) fprintf(stream, "%u\t", time);
        fprintf(stream, "%f\t%f\n", px, py);
    }

    Sxx += w * px * px;
    Sxy += w * px * py;
    Syy += w * py * py;
    Sx  += w * px;
    Sy  += w * py;
    Sw  += w;
    ++n;

    beta  = Sw * Sxy - Sx*Sy;
    beta /= Sw * Sxx - Sx*Sx;

    alpha = Sy/Sw - beta * Sx/Sw;

    R2  = Sxy*Sw - Sx*Sy;
    R2 *= R2;
    R2 /= (Sxx*Sw-Sx*Sx) * (Syy*Sw-Sy*Sy);

    RSS = (1-R2) * (Syy - Sy*Sy/Sw);

    if (plotOnUpdate) plot();
}

