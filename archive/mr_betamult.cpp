#include "mr_betamult.h"
#include "mr_numerical.h"
#include "mr_strutils.h"

#define DEBUG_2(r,c) (r)*(maxCols+2)+c
#define DEBUG_1(r) DEBUG_2(r,maxCols+1)

#define M(r,c) M[(r)*(maxCols+2)+c]
#define Y(r) M(r,maxCols+1)
#define X(r,c) X[(r)*(maxCols+2)+c]

using namespace mrutils;

const std::vector<double> BetaMultiple::empty;

bool BetaMultiple::add(double y, unsigned time) {
    return add(empty,y,time);
}

void BetaMultiple::regress(const std::vector<double>& y, int count, ...) {
    std::vector<const std::vector<double>*> xs;
    std::vector<double> x;

    unsigned maxPts = y.size();

    va_list list; va_start(list,count);
    for(const std::vector<double>* v
        ;count != 0 && NULL != (v = va_arg(list, const std::vector<double>*))
        ;--count) { xs.push_back(v); if (v->size() < maxPts) maxPts = v->size(); }
    x.resize(xs.size());


    for (unsigned i = 0; i < maxPts; ++i) {
        for (unsigned j = 0; j < x.size(); ++j) x[j] = xs[j]->at(i);
        add(x,y[i]);
    }
}


bool BetaMultiple::add(const std::vector<double> &x, double y, unsigned time, double weight) {
    if (!bc.empty()) return false;
    if (time < lastTime + timeBetween) return false;
    lastTime = time;

    if (weight != 1) weight = sqrt(weight);

    if (useDelta) {
        if (!hasFirst) {
            y_ = y;
            x_ = x;
            return (hasFirst = true);
        }
        double py = y; y = y - y_; y_ = py;
    }

    if (stream != NULL && timeBetween > 0) fprintf(stream, "%u\t", time);

    M.resize((n+1)*(maxCols+2),0);

    for (unsigned c = 0; c < x.size(); ++c) {
        M(n,c+1) = (useDelta? x[c] - x_[c] : x[c]);
        if (stream != NULL) fprintf(stream, "%f\t", M(n,c+1));
        x_[c] = x[c];
        M(n,c+1) *= weight;
    }

    if (stream != NULL) fprintf(stream,"%f\n",y);

    y *= weight;

    M(n,0) = weight;
    Y(n) = y;

    cols = sMAX(cols, (unsigned)x.size());

    dirty = true;
    n++;
    Sy  += y;
    Syy += y*y;

    if (col > 0) col = -1;

    return true;
}

bool BetaMultiple::addConstraint(const BetaConstraint& cc) {
    this->bc = cc;
    if (!bc.solve() || cols == 0) return false;
    dirty = true;

    if (col <= 0) {
        houseTransform(M, n, cols+2, diag, dp, maxCols+2, false);
        col = cols + 1;
    }

    /* **** backup M ***** */
    for (unsigned i = 0; i < sMIN(n*(maxCols+2),SQ(maxCols+2)); ++i)
        _backM[i] = M[i];

    Y(cols+1) = diag[maxCols+1];
    for (unsigned k = 0; k < cols+1; ++k) {
        M(k,k) = diag[k];
        for (unsigned r = k+1; r < cols+2; ++r) M(r,k) = 0;
    }

    /* **** backup diag, dp ***** */
    for (unsigned i = 0; i < maxCols+2; ++i) _backD[i] = diag[i];
    _backdp = dp;

    Y(0) -= bc.sum * M(0,1) / bc.coef[0];
    Y(1) -= bc.sum * M(1,1) / bc.coef[0]; 

    matrixMultiply(M, cols+1, cols+1, maxCols+2, bc.C, cols, false, true);
    houseTransform(M, cols+2, cols+1, diag, dp, maxCols+2, true);
    --cols; --col;

    return true;
}

bool BetaMultiple::addCol(std::vector<double> &dim) {
    if (!bc.empty()) return false;
    dirty = true;

    for (unsigned r = 0; r < dim.size(); ++r) M(r,cols+1) = dim[r];
    ++cols;

    if (col <= 0) return true;

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
    return true;
}

void BetaMultiple::dropConstraint() {
    if (bc.empty()) return;
    if (dirty) ++cols;
    col = cols+1;
    dirty = true;

    for (unsigned i = 0; i < sMIN(n*(maxCols+2),SQ(maxCols+2)); ++i)
        M[i] = _backM[i];
    for (unsigned i = 0; i < maxCols+2; ++i) diag[i] = _backD[i];
    dp = _backdp;

    bc.clear();
}

bool BetaMultiple::dropCol() {
    if (!bc.empty()) return false;
    dirty = true;
    --cols;
    if (col <= 0) return true;
    --col; 
    for (int r = col; r < n; ++r) Y(r) += dp * M(r,col); 
    return true;
}

void BetaMultiple::compute() {
    if (!dirty) return;
    if (col <= 0) {
        houseTransform(M, n, cols+2, diag, dp, maxCols+2);
        col = cols + 1;
    }
    dirty = false;

    memset(&XtXI[0],0,col*col*8);
    double sum = 0;

    /* Invert R */
    for (int j = col-1; j >= 0; --j) {
        XtXI[j*col+j] = 1/diag[j];
        for (int i = j - 1; i >= 0; i--) {
            sum = -M(i,j)/diag[j];
            for (int k = j-1; k > i; k--) { 
                sum -= M(i,k)*XtXI[col*k+j];
            }
            XtXI[col*i+j] = sum/diag[i];
        }
    }

    // compute solution
    for (int i = 0; i < col; ++i) {
        sum = 0;
        for (int j = 0; j < col; j++) {
            sum += XtXI[col*i+j] * Y(j);
        }

        if (i == 0) alpha_ = sum;
        else beta_[i-1] = sum;
    }

    if (!bc.empty()) {
        std::vector<double> v;
        v.push_back(alpha_); 
        for (unsigned i = 0 ; i < cols; ++i) v.push_back(beta_[i]);
        matrixMultiply(bc.C,col+1,cols+1,col+1,v,1,true,true);
        alpha_ = bc.C[0];
        for (int i = 0 ; i < col; ++i) beta_[i] = bc.C[i+1];
        beta_[0] += bc.sum / bc.coef[0];
    }

    // multiply upper triangular by t(upper triangular)
    for (int i = 0; i < col; ++i) {
        for (int j = i; j < col; ++j) {
            XtXI[col*i+j] *= XtXI[col*j+j];
            for (int k = j+1; k < col; ++k) {
                XtXI[col*i+j] += XtXI[col*i+k] * XtXI[col*j+k];
            }
        }
    }

    // RSS
    RSS_ = SQ(diag[maxCols+1]);
    R2_ = (1 - RSS_/(Syy - Sy*Sy/n));

    if (!bc.empty()) { ++cols; ++col; }
}

void BetaMultiple::print(std::ostream& os) {
    if (n == 0) {
        os << "BetaMultiple";
        if (!name.empty()) os << "(" << name << ")";
        os << ": n=0" << std::endl;
    } else {
        compute();

        os << "BetaMultiple...";
        if (!name.empty()) os << "(" << name << ")";
        os << std::endl;
        if (!bc.empty()) os << "\t" << bc << std::endl;
        os << "\t" << "Y = " << alpha_;
        for (unsigned c = 0; c < cols; ++c) os << " + " << beta_[c] << " * x" << c;
        os << std::endl;
        //os << "\t" << "Y(" << x << ") = " << Y(x) << std::endl;
        //os << "\t" << "YError:        " << YError(x)            << std::endl;
        //os << "\t" << "ForecastError: " << ForecastError(x)     << std::endl;
        os << "\t" << "n:             " << n                   << std::endl;
        os << "\t" << "R2:            " << R2()                 << std::endl;
    }
}
