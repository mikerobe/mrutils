#ifndef _MR_CPPLIB_BETASEQ_H
#define _MR_CPPLIB_BETASEQ_H

#include "mr_exports.h"
#include <iostream>
#include <stdio.h>
#include <vector>
#include "fastdelegate.h"
#include "mr_betamult.h"

namespace mrutils {

class _API_ BetaSequential {
    typedef std::vector<double> Col;
    typedef fastdelegate::FastDelegate2<mrutils::BetaMultiple&,std::vector<int>&,bool> okFunc;
    typedef fastdelegate::FastDelegate2<std::vector<int>&,mrutils::BetaConstraint&> constraintFunc;

    public:
        BetaSequential(unsigned cols, okFunc ok, constraintFunc cf) 
        : beta(cols), cols(new Col[cols]), ok(ok), cf(cf)
         ,maxCols(cols), used(new bool[cols]), y_(0)
         ,useDelta(false), hasFirst(false)
         ,timeBetween(0), lastTime(0), stream(NULL)
        {
            x_.reserve(cols);
            for (unsigned c = 0; c < maxCols; ++c) used[c] = false;
        }

        ~BetaSequential() {
            delete[] cols;
            delete[] used;
            if (stream != NULL) fclose(stream);
        }

        inline void setUseDelta(bool tf = true) { useDelta = true; }
        inline void setMinTimeBetween(unsigned time) { timeBetween = time; }

        inline bool streamTo(const char * path, bool append=true) {
            if (stream != NULL) fclose(stream);
            stream = fopen(path, (append?"a":"w"));
            return stream != NULL;
        }
    public:
        bool add(const char * path);
        bool add(std::vector<double> &x, double y, unsigned time = 0);

        void run() {
            if (usedCols.size() > 0) return;

            int bestCol; double bestR2 = -1;
            BetaConstraint bestConstraint;


            while (usedCols.size() < maxCols) {
                bestCol = -1;

                for (unsigned c = 0; c < maxCols; ++c) {
                    if (used[c]) continue;

                    usedCols.push_back(c);
                    beta.addCol(cols[c]);

                    if ( (ok)(beta,usedCols) ) {
                        if( beta.R2() > bestR2 ) {
                            bestCol = c;
                            bestR2 = beta.R2();
                            bestConstraint.clear();
                        }
                    } else {
                        constraints.clear();
                        (cf)(usedCols,constraints);
                        beta.addConstraint(constraints);
                        if ( (ok)(beta,usedCols) && beta.R2() > bestR2 ) {
                            bestCol = c;
                            bestR2 = beta.R2();
                            bestConstraint = constraints;
                        }
                        beta.dropConstraint();
                    }

                    usedCols.pop_back();
                    beta.dropCol();
                }

                if (bestCol == -1) break;

                used[bestCol] = true;
                usedCols.push_back(bestCol);
                beta.addCol(cols[bestCol]);
            }

            if (!bestConstraint.empty()) beta.addConstraint(bestConstraint);
        }

        void printResult(std::ostream& os = std::cout) {
        }

        BetaMultiple beta;
        std::vector<int> usedCols;

    private:
        Col *cols;
        okFunc ok;
        constraintFunc cf; BetaConstraint constraints;
        unsigned maxCols;

        bool *used;

        double y_;
        std::vector<double> x_;
        bool useDelta, hasFirst;
        unsigned timeBetween, lastTime;
        FILE *stream;

};

}

#endif
