#include "mr_probit.h"

#define LARGE 1e6;

void mrutils::Probit::add(std::vector<double>& xp, double yp) {
    const unsigned size = (unsigned)xp.size();

    if (x.empty()) {
        x.resize(size);
        for (unsigned j = 0; j < size; ++j) 
            x[j].reserve(y.capacity());
        beta.resize(1+size);
    }

    for (unsigned j = 0; j < xp.size(); ++j) {
        x[j].push_back(xp[j]);
    } y.push_back(yp);

    ++n; dirty = true;
}

bool mrutils::Probit::compute(double tol) {
    if (!dirty) return true;
    if (!findStart(&beta[0])) return false;
    return mrutils::minimize(mf,beta.size(),NULL,&beta[0],&likelihood, tol);
}

double mrutils::Probit::like(double * beta, void*) {
    double result = 0;

    for (unsigned i = 0; i < n; ++i) {
        double xB = beta[0];
        for (unsigned j = 0; j < x.size(); ++j) 
            xB += beta[1+j] * x[j][i];

        const double prob = mrutils::stats::pnorm(xB);

        if (prob == 1) {
            //if (y[i] != 1) return LARGE;
            if (y[i] != 1) result -= LARGE;
        } else if (prob == 0) {
            //if (y[i] != 0) return LARGE;
            if (y[i] != 0) result -= LARGE;
        } else {
            result += y[i] * log(prob) + (1-y[i])*log(1-prob);
        }
    }

    printf("%f",beta[0]);
    for (unsigned j = 1; j <= x.size(); ++j) printf(",%f",beta[j]);
    printf(": %f\n",-result);
    fflush(stdout);

    return (-result);
}

bool mrutils::Probit::findStart(double * beta) {
    beta[0] = 0;

    for (unsigned j = 0; j < x.size(); ++j) {
        double mean = 0; unsigned added = 0;
        for (unsigned i = 0; i < n; ++i) {
            if (x[j][i] != 0) {
                const double f = added/(double)(added+1);
                ++added; mean = f * mean + ABS_(x[j][i])/added;
            }
        }
        std::cout << "MEAN of " << j << " IS: " << mean << std::endl;

        beta[1+j] = 1./mean/x.size();
        std::cout << beta[1+j] << std::endl;
        if (beta[1+j] != beta[1+j]) return false;
    }

    const double result = like(beta,NULL);
    if (result != result) return false;
    //if (result == LARGE) return false;
    return true;
}

void mrutils::Probit::print(std::ostream& os) {
    if (n == 0) {
        os << "Probit";
        if (!name.empty()) os << "(" << name << ")";
        os << ": n=0" << std::endl;
    } else {
        if (!compute()) {
            os << "Probit... could not compute" << std::endl;
        } else {
            os << "Probit...";
            if (!name.empty()) os << "(" << name << ")";
            os << std::endl;
            os << "\t" << "Y = I(" << beta[0];
            for (unsigned c = 1; c < beta.size(); ++c) os << " + " << beta[c] << " * x" << c;
            os << ")" << std::endl;
            os << "\t" << "n:             " << n                    << std::endl;
            os << "\t" << "likelihood:    " << likelihood           << std::endl;
        }
    }

}
