#ifndef _MR_CPPLIB_OU_PROCESS_H
#define _MR_CPPLIB_OU_PROCESS_H

#include "mr_strutils.h"
#include "mr_beta.h"

namespace mrutils {

    class OUProcess {
        public:
            OUProcess(const char * name = "")
            : name(name), n(0)
             ,lambda(0), sigma(0), mu(0)
             ,broken(true)
            {}

        public:
           /**
            * After this is called, the next observation is not assumed
            * to be an equal time step away from the previous
            */
            inline void addBreak() { broken = true; }
            inline void clear() {
                broken = true; n = 0; 
                Sy = Syy = Sx = Sxx = Sxy = 0;
            }

            inline void add(double d) {
                if (!broken) {
                    Sx += x0; Sxx += x0*x0;
                    Sy += d; Syy += d*d; 
                    Sxy += d*x0;
                    ++n;

                    if (n > 2) {
                        const double beta  = (n*Sxy-Sx*Sy)/(n*Sxx-Sx*Sx);
                        const double alpha = Sy/n - beta * Sx/n;

                        lambda = -log(beta);
                        mu = alpha/(1-beta);

                        R2  = Sxy*n - Sx*Sy;
                        R2 *= R2;
                        R2 /= (Sxx*n-Sx*Sx) * (Syy*n-Sy*Sy);

                        RSS = (1-R2) * (Syy - Sy*Sy/n);

                        sigma = sqrt( 2*lambda*RSS/((n-2)*(1-beta*beta)) );
                    }
                } else broken = false;

                x0 = d;
            }

            inline void print(std::ostream& os = std::cout) {
                os << "OUProcess (" << name << ")\n";
                os << "    lambda: " << lambda << "\n";
                os << "    sigma:  " << sigma << "\n";
                os << "    mu:     " << mu << "\n";
            }

            inline friend std::ostream& operator<<(std::ostream& os, const OUProcess& ou) {
                return oprintf(os, "OUProcess (%s) (lambda %f, sigma %f, mu %f)"
                    ,ou.name.c_str(),ou.lambda,ou.sigma,ou.mu);
            }
        public:
            const std::string name;
            unsigned n;
            double lambda, sigma, mu;
            double x0;

            double R2, RSS;

        private:
            double Sy, Syy, Sx, Sxx, Sxy;
            bool broken;
    };

}


#endif
