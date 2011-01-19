#include "mr_betaseq.h"

using namespace mrutils;

bool BetaSequential::add(const char * path) {
    char buffer[16384]; char *ptr, *start;
    double y; std::vector<double> x;

    FILE *input = fopen(path,"r");
    if (input == NULL) return false;

    bool useDelta = this->useDelta;
    unsigned timeBetween = this->timeBetween;
    this->useDelta = false;
    this->timeBetween = 0;

    while (fgets(buffer,16384,input)) {
        start = ptr = buffer;
        x.clear();

        while (ptr != '\0') {
            ptr = strchr(ptr,'\t');
            if (ptr != NULL) *ptr++ = '\0';
            x.push_back(atof(start));
            if (ptr == NULL) break;
            start = ptr;
        }

        y = x.back(); x.pop_back();
        add(x,y);
    }

    fclose(input);

    this->useDelta = useDelta;
    this->timeBetween = timeBetween;
    this->hasFirst  = false;

    return true;
}

bool BetaSequential::add(std::vector<double> &x, double y, unsigned time) {
    if (time < lastTime + timeBetween) return false;
    lastTime = time;

    if (useDelta) {
        if (!hasFirst) {
            y_ = y; 
            x_ = x;
            return (hasFirst = true);
        }
        double py = y; y = y - y_; y_ = py;
    }

    for (unsigned c = 0; c < x.size(); ++c) {
        cols[c].push_back( (useDelta?x[c] - x_[c] : x[c]) );
        x_[c] = x[c];
        if (stream != NULL) fprintf(stream, "%f\t", cols[c].back());
    }

    beta.add(y);
    if (stream != NULL) fprintf(stream,"%f\n",y);

    return true;
}
