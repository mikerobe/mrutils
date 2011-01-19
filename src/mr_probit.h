#ifndef _MR_CPPLIB_PROBIT_H
#define _MR_CPPLIB_PROBIT_H

#include <iostream>
#include "mr_numerical.h"

namespace mrutils {

class _API_ Probit {
    public:
        Probit(const char * name = "", int initSize = 1024)
        : n(0)
         ,likelihood(0)
         ,name(name)
         ,dirty(false)
        {
            mf = fastdelegate::MakeDelegate(this,&Probit::like);
            y.reserve(initSize);
        }

        ~Probit() {
        }

    public:
       /*****************
        * Main methods
        *****************/

        void add(std::vector<double>& x, double y);

        bool compute(double tol = 1e-7);

        std::vector<double> beta;
        std::vector<double> y;
        std::vector<std::vector<double> > x;
        unsigned n;
        double likelihood; // result of the likelihood function minimization

        std::string name;

    public:
       /*********************
        * Utilities
        *********************/

        void print(std::ostream& os = std::cout);

    private:
        double like(double * beta, void*);
        bool findStart(double * beta);

    private:
        bool dirty;
        mrutils::multiDFunc mf;

};

}


#endif
