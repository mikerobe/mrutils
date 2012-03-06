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
#include "mr_gnuplot.h"
#include "mr_delim.h"

#define _BETA(a) mrutils::BetaMultiple::var_t(a,#a)

/**
 * Notes:
 * (1) the p-values are computed using normal distributions not student-t
 * (2) this is not returning correct R2 with non-unit weights
 */
namespace mrutils {

class _API_ BetaMultiple {
public:
   /**
    * This simplifies adding named variables, so you can call
    * beta.regress(y) << _BETA(x1) << _BETA(x2)
    * and the names of x1 and x2 will be set
    */
    struct var_t {
        var_t(const std::vector<double>& v, const char * name) : v(v), name(name) {}
        const std::vector<double>& v;
        const char * name;
    };

public:
    BetaMultiple(const int maxCols, const char * name = "");

    ~BetaMultiple()
    {
        if (g != NULL) delete g;
    }


public:
    inline void clear() { dirty = false; Sy = Syy = dp = 0; xs.clear(); ys.clear(); }

    inline void setStoreData(bool tf = true) { storeData = tf; }

   /**
    * Compute must be called before any of the statistics are
    * actually updated
    */
    void compute();

    void add(const std::vector<double> &x, double y, double weight = 1);

    inline BetaMultiple& operator<<(const var_t& var) throw (mrutils::ExceptionNumerical)
    { addCol(var.v,var.name); return *this; }

   /**
    * Returns the coefficient associated with a named column
    */
    inline double operator[](const char * col) throw (mrutils::ExceptionNoSuchColumn)
    {
        for (unsigned i = 0; i < xnames.size(); ++i) {
            if (0==stricmp(col,xnames[i].c_str())) return beta[i];
        }

        throw mrutils::ExceptionNoSuchColumn(col);
    }

   /**
    * This just specifies the y variable. 
    * Allows something like beta.regress(y) << x1 << x2;
    * NOTE: this means you can't do weighted regression
    */
    BetaMultiple& regress(const std::vector<double>& y);

    void addCol(const std::vector<double>& x, const char * name = "") throw (mrutils::ExceptionNumerical);

   /** 
    * Drops the last added column
    */
    void dropCol();

   /**
    * Forecasting
    */
    inline double Y(const std::vector<double>&x)
    {
        if (dirty) compute();
        double y = alpha;
        for (int i = 0; i < cols; ++i) y += beta[i] * x[i];
        return y;
    }

    void print(std::ostream& os = std::cout);
    void plot();

public:
    std::string name;

    std::vector<std::string> xnames;
    double alpha; std::vector<double> beta;
    int n;
    double RSS, R2;

    double alphaStderr; std::vector<double> betaStderr;
    double residStderr;

    std::vector<std::vector<double> > xs;
    std::vector<double> ys, fitted, resid;

private:
    std::vector<double> M;
    std::vector<double> diag;
    std::vector<double> XtXI;

    bool storeData;

    const int maxCols;
    int cols, col;
    bool dirty;

    double dp;
    double Syy, Sy;

    mrutils::Gnuplot *g;
};

}

#endif
