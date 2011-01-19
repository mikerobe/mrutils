#include "mr_expavg.h"
#include <cmath>
#include "float.h"
#include "mr_gnuplot.h"

using namespace mrutils;

ExpAverage::ExpAverage(int length, int header, int minPoints, unsigned updateMillis, const char * name)
: name_(name), plotOnUpdate(false), storeData(false), lastTime_(0), updateMillis_(updateMillis)
 ,theta_( pow(1/static_cast<double>(length),1/static_cast<double>(length)) )
 ,result_(0), cached_(DBL_MAX)
 ,length_(length), count_(0), minPoints_(minPoints), header_(header)
 ,stream(NULL)
 ,g(NULL)
{
}

void ExpAverage::printSummary(std::ostream& os) {
    os << *this << std::endl;
    if (storeData) {
        os << "Point\tResult" << std::endl;

        for (unsigned i = 0; i < xs.size(); i++) {
            os << xs[i] << "\t" << results[i] << std::endl;
        }
    }
}

bool ExpAverage::monitor(int fd) {
    if (fd < 0) return false;

    char buffer[1024];
    mrutils::Tail tail(fd, buffer, sizeof(buffer));

    if (plotOnUpdate) {
        plotOnUpdate = false;
        while (mrutils::readLine(fd, buffer, sizeof(buffer))) addLine(buffer);
        plotOnUpdate = true;
        plotSummary();
    }

    while (tail.nextLine()) addLine(tail.line);

    return true;
}

void ExpAverage::plotSummary() {
    if (xs.size() <= 2) return;

    if (g == NULL) g = new mrutils::Gnuplot;

    g->clear();
    g->setGrid();
    g->setNoZero();
    g->setXMillis();
    g->setLabels(name_.c_str());
    g->lines(1,"grey").plot(timestamps, xs);
    g->lines(2,"black").plot(timestamps, results);
    g->draw();
}

void ExpAverage::addLine(char * line) {
    char * p = strchr(line,'\t'); if (p == NULL) return;
    *p++ = '\0';
    unsigned now = atoi(line); line = p;
    p = strchr(p, '\t'); if (p == NULL) return;
    *p++ = '\0';
    double x = atof(line); 
    double result_ = atof(p); 

    this->result_ = result_;
    ++this->count_;
    this->lastTime_ = now;
    if (count_ <= minPoints_ || cached_ == DBL_MAX) cached_ = result_;

    if (storeData) {
        timestamps.push_back(now);
        xs.push_back(x);
        results.push_back(result_);
    }

    if (plotOnUpdate) plotSummary();
}

bool ExpAverage::add(unsigned now, double x) {
    if (now < lastTime_ + updateMillis_) return false;
    result_ = (count_ < header_ 
        ? (result_*count_+x)/(count_+1) 
        : (1-theta_)*x+theta_*result_ );
    count_++; lastTime_ = now;
    if (count_ <= minPoints_ || cached_ == DBL_MAX) cached_ = result_;

    if (storeData) {
        timestamps.push_back(now);
        xs.push_back(x);
        results.push_back(result_);
    }

    if (stream != NULL) {
        fprintf(stream, "%u\t%f\t%f\n", now, x, result_);
    }

    if (plotOnUpdate) plotSummary();

    return true;
}
