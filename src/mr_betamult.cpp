#include "mr_betamult.h"

#define DEBUG_2(r,c) (r)*(cols+2)+c
#define DEBUG_1(r) DEBUG_2(r,cols+1)

#define M(r,c) M[(r)*(maxCols+2)+c]
#define Y(r) M(r,maxCols+1)
#define X(r,c) X[(r)*(maxCols+2)+c]

namespace mrutils {
    BetaMultiple::BetaMultiple(const int maxCols, const char * name)
    : name(name)
     ,alpha(0)
     ,RSS(0), R2(0)
     ,alphaStderr(0)
     ,diag(maxCols+2,0)
     ,XtXI(SQ(maxCols+1),0)
     ,storeData(false)
     ,maxCols(maxCols)
     ,cols(0), col(0)
     ,dirty(false)
     ,dp(0), Syy(0), Sy(0)
     ,g(NULL)
    {
        M.reserve(1024 * (cols+2));
        xs.reserve(maxCols);
    }

    BetaMultiple& BetaMultiple::regress(const std::vector<double>& y) {
        dirty = true;

        if (n == 0) {
            n = (int)y.size();
            M.resize(n*(maxCols+2),0);

            for (int r = 0; r < n; ++r) M(r,0) = 1;
        }

        for (int i = 0; i < n; ++i) { 
            Y(i) = y[i]; 
            Sy += y[i]; Syy += y[i]*y[i];
        }

        if (storeData) ys = y;

        return *this;
    }

    void BetaMultiple::addCol(const std::vector<double>& x, const char * name) throw (mrutils::ExceptionNumerical) {
        dirty = true;

        if (cols == maxCols) throw ExceptionNumerical("BetaMultiple: too many columns added");

        if (n == 0) {
            n = (int)x.size();
            M.resize(n*(maxCols+2),0);
            for (int r = 0; r < n; ++r) M(r,0) = 1;
        }

        if (storeData) { xs.push_back(x); }

        if ((int)xnames.size() < cols+1) xnames.resize(cols+1); 
        if (xnames[cols].empty()) xnames[cols] = name;

        for (unsigned r = 0; r < x.size(); ++r) M(r,cols+1) = x[r];
        ++cols;

        if (col <= 0) return;

        double nrm = 0;

        for (int k = 0; k < col; ++k) { 
            dp = 0;

            for (int r = k; r < n; ++r) dp += M(r,k) * M(r,col);
            dp /= M(k,k);

            for (int r = k; r < n; ++r) M(r,col) -= dp * M(r,k);
        }

        for (int r = col; r < n; ++r) nrm = PYTHAG(nrm,M(r,col));
        if (M(col,col) < 0) nrm = -nrm;

        dp = Y(col);
        for (int r = col; r < n; ++r) {
            M(r,col) /= nrm;
            dp += M(r,col) * Y(r);
        }

        M(col,col) += 1;
        dp /= M(col,col);
        diag[col] = -nrm;

        Y(col) -= dp * M(col,col); 
        diag[maxCols+1] = 0;
        for (int r = col+1; r < n; ++r) {
            Y(r) -= dp * M(r,col); 
            diag[maxCols+1] = PYTHAG(diag[maxCols+1],Y(r));
        }

        ++col;
    }

    void BetaMultiple::dropCol() {
        dirty = true;
        --cols; if (col <= 0) return;
        --col; for (int r = col; r < n; ++r) Y(r) += dp * M(r,col); 
    }

    void BetaMultiple::add(const std::vector<double> &x, double y, double weight) {
        if (weight != 1) weight = sqrt(weight);

        M.resize((n+1)*(maxCols+2),0);

        for (unsigned c = 0; c < x.size(); ++c) {
            M(n,c+1) = x[c] * weight;
            if (storeData) xs[c].push_back(x[c] * weight);
        }

        y *= weight;

        M(n,0) = weight;
        Y(n) = y;
        if (storeData) ys.push_back(y);

        dirty = true;
        Sy  += y; Syy += y*y;
        ++n;

        cols = MAX_(cols, (int)x.size());
        if (col > 0) col = -1;
    }

    void BetaMultiple::compute() {
        if (!dirty) return;

        if (col <= 0) {
            houseTransform(M, n, cols+2, diag, dp, maxCols+2);
            col = cols + 1;
        } dirty = false;

        mrutils::printVector(std::cout,diag);
        
        beta.resize(cols); xnames.resize(cols); betaStderr.resize(cols);
        memset(&XtXI[0],0,col*col*8);
        double sum = 0;

        /* Invert R */
        for (int j = col-1; j >= 0; --j) {
            if (ABS_(diag[j]) < 1e-6) continue;

            XtXI[j*col+j] = 1/diag[j];
            for (int i = j - 1; i >= 0; i--) {
                sum = -M(i,j)/diag[j];
                for (int k = j-1; k > i; k--) { 
                    sum -= M(i,k)*XtXI[col*k+j];
                }
                if (ABS_(diag[i]) >= 1e-6)
                    XtXI[col*i+j] = sum/diag[i];
            }
        }

        // compute solution
        for (int i = 0; i < col; ++i) {
            sum = 0;
            for (int j = 0; j < col; ++j) {
                sum += XtXI[col*i+j] * Y(j);
            }

            if (i == 0) alpha = sum;
            else beta[i-1] = sum;
        }

        // RSS
        RSS = SQ(diag[maxCols+1]);
        R2 = (1 - RSS/(Syy - Sy*Sy/n));
        residStderr = sqrt(RSS/(n-cols-1));

        // multiply upper triangular by t(upper triangular)
        // to get standard errors
        for (int i = 0; i < col; ++i) {
            for (int j = i; j < col; ++j) {
                XtXI[col*i+j] *= XtXI[col*j+j];
                for (int k = j+1; k < col; ++k) {
                    XtXI[col*i+j] += XtXI[col*i+k] * XtXI[col*j+k];
                }
            }

            if (i == 0) alphaStderr = sqrt(XtXI[0]) * residStderr;
            else betaStderr[i-1] = sqrt(XtXI[col*i+i]) * residStderr;
        }

        if (storeData) {
            fitted.clear();
            fitted.reserve(n);
            resid.clear(); resid.reserve(n);
            for (unsigned i = 0; i < ys.size(); ++i) {
                double y = alpha;
                for (int j = 0; j < cols; ++j) y += beta[j] * xs[j][i];
                fitted.push_back(y);
                resid.push_back(y - ys[i]);
            }
        }
    }

    void BetaMultiple::plot() {
        if (!storeData) return;
        if (g == NULL) g = new mrutils::Gnuplot();

        g->setLabels(mrutils::stringstream().clear() << "BetaMultiple (" << name << ")","fitted","y");
        g->plot(fitted,ys);
        g->draw();
    }

    void BetaMultiple::print(std::ostream& os) {
        if (n == 0) {
            oprintf(os,"BetaMultiple (%s): n=0\n",name.c_str());
        } else {
            compute();

            oprintf(os,"BetaMultiple (%s)...\n",name.c_str());

            mrutils::ColumnBuilder cb;
            cb.cols(-1,"","Estimate","Std. Error","t value","Pr(>|t|)","",NULL);
            for (unsigned i = 0; i <= beta.size(); ++i) {
                const double err = i == 0 ? alphaStderr : betaStderr[i-1];
                const double coef = i == 0 ? alpha : beta[i-1];

                const double sig = 2*stats::pnorm(-ABS_(coef)/err);

                cb.fro(i == 0 ? "(intercept)" : xnames[i-1]);
                cb.add(coef);
                cb.add(err);

                if (sig < 2e-16) {
                    cb.add("< 2e-16");
                } else cb.add(sig);

                if (sig < 0.001) cb.add("***");
                else if (sig < 0.01) cb.add("**");
                else if (sig < 0.05) cb.add("*");
                else if (sig < 0.1) cb.add(".");
            } cb.print(os); 

            cb.clear(); cb.cols(2,"","");
            cb.fro("    n:"); cb.add(n);
            cb.fro("    RSS:"); cb.add(RSS);
            cb.fro("    stderr:"); cb.add(residStderr);
            cb.fro("    R2:"); cb.add(R2);
            cb.print();
        }
    }
}
