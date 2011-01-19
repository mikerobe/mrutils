#include "mr_numerical.h"
#include "gsl/gsl_cdf.h"
#include "gsl/gsl_errno.h"

inline void gsl_error(const char * reason, const char * file, int line, int gsl_errno) {
    throw mrutils::ExceptionNumerical(mrutils::stringstream().clear()
        << "GSL error " << gsl_errno << " (" << reason << ") at " << file << ": " << line);
}

namespace {
    inline bool initGSL() {
        gsl_set_error_handler(&gsl_error);
        return true;
    } static const bool initGSL_ = initGSL();
}

namespace mrutils {
    namespace stats {
        double qgamma(const double P, const double a, const double b) throw (mrutils::ExceptionNumerical) {
            return gsl_cdf_gamma_Pinv(P,a,b);
        }
    }
}
