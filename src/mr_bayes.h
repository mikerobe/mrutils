#ifndef _MR_CPPLIB_BAYESIAN_H
#define _MR_CPPLIB_BAYESIAN_H

#include "mr_numerical.h"
#include "mr_strutils.h"

namespace mrutils {
    namespace bayes {

        /**
         * For an observation with z = a+bx
         * and variance c.
         */
        struct linearObs_t {
            double a, b, c;
        };

       /**
        * Bayesian X
        * ===============
        * There's an underlying x. Observe z = x + e with e ~
        * N(0,sigma_e^2). You have a prior on x specified with mu, var. Each
        * call to update(z,var) does a Bayesian update of your prior given
        * the observation. 
        */
        class BayesianX {
            public:
                BayesianX(const double mu = 0, const double var = 1)
                : mu(mu), var(var)
                {}

                BayesianX(const BayesianX& other) 
                : mu(other.mu), var(other.var)
                {}

                inline BayesianX& operator=(const BayesianX& other) {
                    if (this != &other) {
                        mu = other.mu; var = other.var;
                    } return *this;
                }

            public:
                inline void update(const double z, const double varZ) {
                    const double tv = varZ + var;

                    mu = (z*var + mu*varZ)/ tv; 
                    var *= varZ/tv;
                }

               /**
                * Returns the updated mean and var that would result
                * from an observation of the form a+bx with variance
                * c.
                */
                inline void update(linearObs_t* obs) {
                    const double tv = obs->c + var;

                    obs->a = (obs->a * var + mu * obs->c)/tv;
                    obs->b = (obs->b * var + mu * obs->c)/tv;
                    obs->c *= var / tv;
                }

                inline friend std::ostream& operator<<(std::ostream& os, const BayesianX& x) {
                    return os << "BayesianX (" << x.mu << ", " << x.var << ")";
                }

            public:
                double mu, var;
        };

       /**
        * LinearX
        * =======
        * Just like BayesianX, but instead of updates like (z, varZ), 
        * updates are assumed linear functions of x. a+bx with variance c.
        * This variable has mean a+bx and variance c.
        */
        class LinearX {
            public:
                LinearX(const double a = 0, const double b = 0, const double c = 1)
                : a(a), b(b), c(c)
                {}

                LinearX(const LinearX& other) 
                : a(other.a), b(other.b), c(other.c)
                {}

                inline LinearX& operator=(const LinearX& other) {
                    if (this != &other) {
                        a = other.a; b = other.b; c = other.c;
                    } return *this;
                }

            public:
                inline void update(const double a, const double b, const double c) {
                    const double tv = c + this->c;
                    this->a = (a*this->c+this->a*c)/tv;
                    this->b = (b*this->c+this->b*c)/tv;
                    this->c *= c / tv;
                }

            public:
                double a, b, c;
        };

       /**
        * BayesianXY
        * ==========
        * There are 2 underlyings that you want to know and have
        * priors for (mu[0],var[0],mu[1],var[1]). You observe z =
        * x+y+e with e ~ N(0,var_e). Set the priors, then call
        * update(z,varZ) to get the posteriors.
        */
        class BayesianXY {
            enum { X = 0, Y = 1 };

            public:
                BayesianXY(const double mux = 0, const double varx = 1, const double muy = 0, const double vary = 1)
                : x(xs[0]), y(xs[1]), n(0)
                { 
                    xs[X].mu = mux; xs[X].var = varx;
                    xs[Y].mu = muy; xs[Y].var = vary;
                }

            public:
                inline void update(const double z, const double varZ) {
                    if (++n == 1) {
                        for (unsigned s = 0; s < 2; ++s) {
                            ls[s].a = xs[s].mu;
                            ls[s].b = 0; 
                            ls[s].c = xs[s].var;
                        }
                    }

                    for (unsigned s = 0; s < 2; ++s) {
                        const double b = 1 + ls[!s].b;
                        const double c = ls[!s].c + varZ;
                        const double tv = c + b*b*xs[s].var;

                        xs[s].mu = (c*xs[s].mu - xs[s].var*b*(ls[!s].a-z))/tv;
                        xs[s].var *= c/tv;

                        ls[!s].update(z,-1,varZ);
                    }
                }

                inline void print(std::ostream& os = std::cout) const {
                    os << "BayesianXY\n";
                    oprintf(os,"    x: %f (%f)\n",x.mu,x.var);
                    oprintf(os,"    y: %f (%f)\n",y.mu,y.var);
                }

            public:
                BayesianX &x, &y;
                unsigned n;

            private:
                BayesianX xs[2];
                LinearX ls[2];
        };

        /**
         * Assume that there's a random walk added to an OU process that
         * we seek to estimate. OU is x, noise is e. Initially seed x0
         * with no uncertainty, then take a measurement that is x1+e1.
         * The next measurement is x2+e1+e2. 
         *
         * varE is provided with z, which indicates the variance of the
         * added e. so z2=x2+e1+e2, varE is var(e2)
         *
         * OU coefficients are lambda, mu, sigma. These can't be set
         * directly, but are updatable with an inlined function
         */
        class BayesianOURW {
            public:
                BayesianOURW(const double lambda, const double sigma, const double mu)
                : n(0), alpha(mu*(1-exp(-lambda))), beta(exp(-lambda))
                 ,sigma2(sigma*sigma*(1-exp(-2*lambda))/(2*lambda))
                 ,lastZ(0)
                {}

                BayesianOURW()
                : n(0), alpha(0), beta(1), sigma2(0), lastZ(0)
                {}

            public:
                inline void clear() { n = 0; }

                inline void seedX(const double z0) {
                    x.mu =  z0;
                }

                inline void update(const double z, const double varE) {
                    if (++n == 1) {
                        e.mu = 0; e.var = varE;
                        x.mu = alpha + beta*x.mu;
                        x.var = sigma2;

                        e.update(z - x.mu, x.var);
                        x.update(z,varE);
                    } else {
                        const double a = x.mu, c = x.var;
                        const double tv = c*(beta-1)*(beta-1)+sigma2+varE;

                        e.var = (sigma2*varE + c*(sigma2+beta*beta*varE))/tv;
                        e.mu = (c*(beta-1)*(-z+alpha+lastZ*beta)+(lastZ-a)*sigma2+(z-alpha-a*beta)*varE)/tv;

                        x.var = ((sigma2*varE+c*(sigma2+beta*beta*varE))/tv);
                        x.mu = (c*(1-beta)*(alpha+(lastZ-z)*beta)+(a-lastZ+z)*sigma2+(alpha+a*beta)*varE)/tv;
                    }

                    lastZ = z;
                }

                inline void updateOUCoefs(const double lambda, const double sigma, const double mu) {
                    alpha = mu * (1-exp(-lambda));
                    beta = exp(-lambda);
                    sigma2 = sigma*sigma*(1-exp(-2*lambda))/(2*lambda);
                }

                inline void print(std::ostream& os = std::cout) const {
                    os << "BayesianOURW\n";
                    oprintf(os,"    x: %f (%f)\n",x.mu,x.var);
                    oprintf(os,"    e: %f (%f)\n",e.mu,e.var);
                }

            public:
                unsigned n;
                BayesianX x, e;

            private:
                double alpha, beta, sigma2;
                double lastZ;
        };

    }
}

#endif
