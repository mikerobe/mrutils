#ifndef _MR_CPPLIB_EXPPOIS_H
#define _MR_CPPLIB_EXPPOIS_H

#include "mr_numerical.h"
#include "mr_gnuplot.h"

namespace mrutils {

template <class T>
class _API_ ExpPoisson {
    public:
        ExpPoisson(double length = 100, const char * name = "")
        : name(name)
         ,kappa(pow(0.5,1./length)), lnkappa(log(kappa))
         ,lastAdd(0), startTime(0), setStart(false)
         ,alpha(0)
         ,likeSum(0), likeInt(0)
         ,g(NULL)
        {}

        ~ExpPoisson() {
            if (g != NULL) delete g;
        }

    public:
       /*****************
        * Init & settings
        *****************/

        inline void setKappa(const double kappa) { 
            this->kappa = kappa; 
            this->lnkappa = log(kappa); 
        }

        inline void setLength(const double length) {
            kappa = pow(0.5,1./length);
            lnkappa = log(kappa);
        }

        inline void start(T milliTime) {
            this->startTime = milliTime;
            setStart = true;
        }

        std::string name;

    public:
       /*****************
        * Main functions
        *****************/

        inline void clear() {
            lastAdd = 0;
            startTime = 0; setStart = false;
            alpha = 0; likeSum = 0; likeInt = 0;
        }

        inline void add(const T milliTime) {
            if (lastAdd > 0) {
                likeSum += log(get(milliTime));

                if (kappa < 1) {
                    likeInt += -alpha*(milliTime-lastAdd)*lnkappa
                        + alpha * log( (1-pow(kappa,milliTime-startTime))/(1-pow(kappa,lastAdd-startTime)) );
                    alpha *= pow(kappa,milliTime - lastAdd);
                } else {
                    likeInt += alpha * log((milliTime-startTime)/(lastAdd-startTime));
                }
            } else if (!setStart) { 
                startTime = milliTime; 
                setStart = true; 
                return;
            }

            alpha += 1;
            lastAdd = milliTime;
        }

        inline double get(const T milliTime) const {
            if (lastAdd == 0) return 0;

            if (kappa == 1) {
                return alpha/(milliTime - startTime);
            } else {
                const double a = alpha * pow(kappa,milliTime - lastAdd);
                const double b = (pow(kappa,milliTime - startTime) - 1)/lnkappa;

                return (a/b);
            }
        }

        inline double var(const T milliTime) const {
            if (lastAdd == 0) return DBL_MAX;

            if (kappa == 1) {
                const double time = milliTime - startTime;
                return (alpha/(time*time));
            } else {
                const double b = (pow(kappa,milliTime - startTime) - 1)/lnkappa;
                const double a = alpha * pow(kappa,milliTime - lastAdd);

                return (a/(b*b));
            }
        }

        /**
         * Returns the log likelihood of the data given
         * this choice of kappa.
         */
        inline double lnLike(const T milliTime) {
            return (likeSum - likeInt
            - (kappa < 1
                ?
                 -alpha*(milliTime-lastAdd)*lnkappa
                    + alpha * log( (1-pow(kappa,milliTime-startTime))/(1-pow(kappa,lastAdd-startTime)) )
                : alpha * log((milliTime-startTime)/(lastAdd-startTime))
              ));
        }

    public:
       /*****************
        * Utilities
        *****************/

        void print(const T milliTime, std::ostream& os = std::cout);
        void plot(const T milliTime);

    private:
        double kappa, lnkappa;
        T lastAdd, startTime; bool setStart;
        double alpha;

        /**
         * likelihood consists of 
         * lnL = \sum ln(\lambda_i) - \int_0^T\lambda(t)dt
         *       likeSum            - likeInt
         */
        double likeSum, likeInt;

        mrutils::Gnuplot* g;
};

}

#endif
