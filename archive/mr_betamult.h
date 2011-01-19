#ifndef _MR_CPPLIB_BETAMULT_H
#define _MR_CPPLIB_BETAMULT_H

#include "mr_exports.h"
#include <iostream>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include "mr_strutils.h"
#include "mr_numerical.h"

// TODO DEBUG: this is not returning correct R2 with non-unit weights

namespace mrutils {

class _API_ BetaConstraint {
    friend class BetaMultiple;

    public:
        double sum;
        std::vector<double> coef;

        BetaConstraint()
        : sum(0) {}

        inline bool empty() { return coef.empty(); }

        inline void clear() { 
            sum = 0;
            coef.clear(); 
            C.clear();
        }

        BetaConstraint& operator=(const BetaConstraint& other) {
            if (this != &other) {
                this->sum = other.sum;
                this->coef = other.coef;
            }

            return *this;
        }

        friend std::ostream& operator << (std::ostream& os, const BetaConstraint& bc) {
            os << "BetaConstraint (sum " << bc.sum << "; coef ";
            printVector(os,bc.coef);
            return os << ")";
        }

    private:
        std::vector<double> C;

        bool solve() {
            if (coef.empty()) return false;
            if (coef[0] == 0) return false;
            C.clear();
            double nrm = normalize(coef);

            double maxVal = *std::max_element(coef.begin(), coef.end());
            bool gotMax = false;

            C.push_back(1); C.push_back(0); C.push_back(0);

            for (unsigned oneCol = 1; oneCol <= coef.size(); ++oneCol) {
                if (!gotMax && coef[oneCol-1] == maxVal) { gotMax=true; continue; }
                C.push_back(0); 
                for (unsigned c = 0; c < coef.size(); ++c) 
                    C.push_back((c+1==oneCol?1:0)-coef[oneCol-1]*coef[c]);
            }

            for (unsigned i = 0; i < coef.size(); ++i) coef[i] *= nrm;

            return true;
        }
};


class _API_ BetaMultiple {
    public:
        BetaMultiple(int maxCols) 
        : n(0)
         ,diag(maxCols+2,0)
         ,XtXI(SQ(maxCols+1),0)
         ,_backM(SQ(maxCols+2),0)
         ,_backD(maxCols+2,0)
         ,cols(0), maxCols(maxCols), col(0), dirty(false)
         ,dp(0), alpha_(0), beta_(maxCols,0)
         ,Syy(0), Sy(0), RSS_(0), R2_(0)
         ,x_(maxCols,0)
         ,useDelta(false), hasFirst(false)
         ,lastTime(0), timeBetween(0), stream(NULL)
        {
            M.reserve(1000 * (maxCols+2));
        }

        ~BetaMultiple() {
            if (stream != NULL) fclose(stream);
        }

        inline void setName(const char * name) 
        { this->name = name; }
        
        inline void setUseDelta(bool tf = true) { useDelta = true; }
        inline void setMinTimeBetween(unsigned time) { timeBetween = time; }

        inline bool streamTo(const char * path, bool append=true) {
            if (stream != NULL) fclose(stream);
            stream = fopen(path, (append?"a":"w"));
            return stream != NULL;
        }

        inline void clear() {
            cols = 0; dirty = false;
            col = 0;
            alpha_ = 0; Syy = 0; Sy = 0; RSS_ = 0;
            R2_ = 0; n = 0; dp = 0;
            hasFirst = false;
            lastTime = 0; 
        }

    public:
        inline double alpha() { if (dirty) compute(); return alpha_; }
        inline double beta(int col) { if(dirty) compute(); return beta_[col]; }
        inline double R2() { if (dirty) compute(); return R2_; }
        inline double RSS() { if (dirty) compute(); return RSS_; }
        int n;

    public:
        inline double Y(const std::vector<double>&x) {
            if (dirty) compute();
            double y = alpha_;
            for (unsigned i =0; i < cols; ++i) {
                y += beta_[i] * x[i];
            }
            return y;
        }
        bool add(const std::vector<double> &x, double y, unsigned time = 0, double weight = 1);
        bool add(double y, unsigned time = 0);
        bool addCol(std::vector<double> &dim);
        bool dropCol();

       /**
        * Regress y against a collection of x vectors. Pass pointers to
        * vectors for the x's and y is passed by reference.
        */
        void regress(const std::vector<double>& y, int count, ...);

        bool addConstraint(const BetaConstraint& bc);
        void dropConstraint();

    public:
        void print(std::ostream& os = std::cout);
        void compute();

    private:
        std::string name;

        std::vector<double> M;
        std::vector<double> diag;
        std::vector<double> XtXI;

        BetaConstraint bc;
        std::vector<double> _backM;
        std::vector<double> _backD;
        double _backdp; double _backSyy, _backSy;

        unsigned cols, maxCols;
        int col;
        bool dirty;

        double dp;
        public: double alpha_; std::vector<double> beta_; private:
        double Syy, Sy, RSS_, R2_;

        double y_;
        std::vector<double> x_;
        bool useDelta, hasFirst;
        unsigned lastTime, timeBetween;
        FILE *stream;

        static const std::vector<double> empty;
};
}

#endif
