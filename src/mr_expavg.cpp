#include "mr_expavg.h"
#include <cmath>
#include "float.h"
#include "mr_gnuplot.h"

using namespace mrutils;

ExpAverage::ExpAverage(int length, int header, int minPoints, unsigned updateMillis, const char * name)
: name(name)
 ,lastUpdate(0)
 ,theta(pow(0.5,1./length))
 ,result(0)
 ,length(length), count(0), minPoints(minPoints)
 ,header(header)
 ,good(false)
 ,var(length,header,minPoints,0)
 ,plotOnUpdate(false), storeData(false), useVar(false)
 ,interval(updateMillis)
 ,stream(NULL)
 ,g(NULL)
{}

void ExpAverage::print(std::ostream& os) {
    os << *this << std::endl;
    if (storeData) {
        os << "Point\tResult" << std::endl;

        for (unsigned i = 0; i < xs.size(); i++) {
            os << xs[i] << "\t" << results[i] << std::endl;
        }
    }
}

bool ExpAverage::add(const unsigned now, const double x) {
    if (now < lastUpdate + interval) return false;
    lastUpdate = now;

    result = count < header 
             ?result*(count/(count+1.)) + x/(count+1) 
             :result*theta + x*(1-theta);
    ++count;

    if (useVar) var.add(now,x);
    if (storeData) {
        timestamps.push_back(now);
        xs.push_back(x);
        results.push_back(result);
        if (useVar) stdevs.push_back(var.stdev());
    }

    if (stream != NULL) {
        fprintf(stream, "%u\t%f\t%f\n", now, x, result);
    }

    if (plotOnUpdate) plot();

    good = count > minPoints;

    return true;
}

void ExpAverage::addLine(char * line) {
    char * p = strchr(line,'\t'); if (p == NULL) return;
    *p++ = '\0';
    unsigned now = atoi(line); line = p;
    p = strchr(p, '\t'); if (p == NULL) return;
    *p++ = '\0';
    double x = atof(line); 
    double result = atof(p); 

    this->result = result;
    ++this->count;
    this->lastUpdate = now;

    if (useVar) var.add(now,x);
    if (storeData) {
        timestamps.push_back(now);
        xs.push_back(x);
        results.push_back(result);
        if (useVar) stdevs.push_back(var.stdev());
    }

    if (plotOnUpdate) plot();
}

bool ExpAverage::monitorFile(int fd, const char * path) {
    if (fd < 0) return false;

    char buffer[1024];
    mrutils::Tail tail(fd, path, buffer, sizeof(buffer));

    if (plotOnUpdate) {
        plotOnUpdate = false;
        while (mrutils::readLine(fd, buffer, sizeof(buffer))) 
            addLine(buffer);
        plotOnUpdate = true;
        plot();
    }

    // TODO: want to aggregate & read everything on the buffer
    // at once, then update, then repeat
    while (tail.nextLine()) addLine(tail.line);

    return true;
}

void ExpAverage::plot() {
    if (xs.size() <= 2) return;

    if (g == NULL) {
        g = new mrutils::Gnuplot;
        g->setTerm("jpg");
    }

    g->clear();
    g->setGrid();
    g->setNoZero();
    g->setXMillis();
    g->setLabels(name.c_str());
    g->points(1,"grey").plot(timestamps, xs);
    g->lines(2,"black").plot(timestamps, results);
    if (count > minPoints) 
        g->lines(1,"black").vline(timestamps[minPoints]);

    if (useVar) {
        // generate +/- 1 stdev line
        std::vector<double> upper, lower;
        upper.reserve(timestamps.size());
        lower.reserve(timestamps.size());
        for (unsigned i = 0; i < timestamps.size(); ++i) {
            upper.push_back(results[i]+stdevs[i]);
            lower.push_back(results[i]-stdevs[i]);
        }
        g->lines(1,"blue").plot(timestamps, upper);
        g->lines(1,"blue").plot(timestamps, lower);
    }

    g->draw();
}
