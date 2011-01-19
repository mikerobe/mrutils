#include <algorithm>
#include <sstream>
#include "mr_numerical.h"
#include "mr_gnuplot.h"
#include "mr_strutils.h"
#include "mr_beta.h"

#ifdef _MR_USE_GSL
    #include "mr_numerical_gsl.cpp"
#else
    namespace mrutils {
        namespace stats {

            double qgamma(const double p, const double k, const double lambda) throw (mrutils::ExceptionNumerical) {
                throw mrutils::ExceptionNumerical("qgamma not compiled -- mrutils compiled without GSL");
            }
        }
    }
#endif

/**************************************************************
 * From Numerical Recipes
 **************************************************************/

#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);
#define SWAP(a,b) temp=(a);(a)=(b);(b)=temp;

namespace {
    double adinf(double z) { 
        /* max |error| < .000002 for z<2, (p=.90816...) */
        /* max |error|<.0000008 for 4<z<infinity */

        if(z<2.) 
            return exp(-1.2337141/z)/sqrt(z)*(2.00012+(.247105-  \
                    (.0649821-(.0347962-(.011672-.00168691*z)*z)*z)*z)*z);
        else 
            return exp(-exp(1.0776-(2.30695-(.43424-(.082433-(.008056 \
                -.0003146*z)*z)*z)*z)*z));
    }

    /*
    The procedure  errfix(n,x)  corrects the error caused
    by using the asymptotic approximation, x=adinf(z).
    Thus x+errfix(n,x) is uniform in [0,1) for practical purposes;
    accuracy may be off at the 5th, rarely at the 4th, digit.
    */
    double errfix(int n, double x) {
        double c,t;
        if(x>.8) return
            (-130.2137+(745.2337-(1705.091-(1950.646-(1116.360-255.7844*x)*x)*x)*x)*x)/n;
        c=.01265+.1757/n;
        if(x<c){ t=x/c;
            t=sqrt(t)*(1.-t)*(49*t-102);
            return t*(.0037/(n*n)+.00078/n+.00006)/n;
        }
        t=(x-c)/(.8-c);
        t=-.00022633+(6.54034-(14.6538-(14.458-(8.259-1.91864*t)*t)*t)*t)*t;
        return t*(.04213+.01365/n)/n;
    }

    /* The function AD(n,z) returns Prob(A_n<z) where
        A_n = -n-(1/n)*[ln(x_1*z_1)+3*ln(x_2*z_2+...+(2*n-1)*ln(x_n*z_n)]
              z_1=1-x_n, z_2=1-x_(n-1)...z_n=1-x_1, and
        x_1<x_2<...<x_n is an ordered set of iid uniform [0,1) variates.
    */
    double AD(int n,double z){
        double c,v,x = adinf(z);

        /* now x=adinf(z). Next, get v=errfix(n,x) and return x+v; */
        if(x>.8) {
            v=(-130.2137+(745.2337-(1705.091-(1950.646-(1116.360-255.7844*x)*x)*x)*x)*x)/n;
            return x+v;
        }
        c=.01265+.1757/n;
        if(x<c){ v=x/c;
            v=sqrt(v)*(1.-v)*(49*v-102);
            return x+v*(.0037/(n*n)+.00078/n+.00006)/n;
        }
        v=(x-c)/(.8-c);
        v=-.00022633+(6.54034-(14.6538-(14.458-(8.259-1.91864*v)*v)*v)*v)*v;
        return x+v*(.04213+.01365/n)/n;
    }

    double poly(const float *cc, int nord, double x) {
        int j;
        double p, ret_val;/* preserve precision! */

        ret_val = cc[0];
        if (nord > 1) {
        p = x * cc[nord-1];
        for (j = nord - 2; j > 0; j--)
            p = (p + cc[j]) * x;

        ret_val += p;
        }
        return ret_val;
    }

    double probks(double x) {
        static const double EPS1 = 0.001;
        static const double EPS2 = 1.0e-8;

        int j; 
        double a2,fac=2.0,sum=0.0,term,termbf=0.0; 
        a2 = -2.0*x*x; 
        for (j=1;j<=100;j++) { 
            term=fac*exp(a2*j*j); 
            sum += term; 
            if (fabs(term) <= EPS1*termbf || fabs(term) <= EPS2*sum) return sum; 
            fac = -fac; 
                termbf=fabs(term); 
        } 
        return 1.0; 
    }


    /** NR trapzd: used in integration. */
    void trapzd(const mrutils::oneDFunc& func, void* data, const double a, const double b, const int n, double * s) {
        double x,tnm,sum,del;
        int it,j;

        if (n == 1) {
            *s=0.5*(b-a)*(func(a,data)+func(b,data));
        } else {
            for (it=1,j=1;j<n-1;j++) it <<= 1;
            tnm=it;
            del=(b-a)/tnm;
            x=a+0.5*del;
            for (sum=0.0,j=1;j<=it;j++,x+=del) sum += func(x,data);
            *s=0.5*(*s+(b-a)*sum/tnm);
        }
    }

    /** NR sort2: used by rankCorrel **/
    void sort2(unsigned n, double arr[], double brr[]) {
        static const unsigned M = 7;

        unsigned i,ir=n,j,k,l=1;
        int jstack=0;
        double a,b,temp;

        std::vector<int> istack; istack.resize(1 + MIN_(50,2+2*log2(n)));
        --arr; --brr;

        for (;;) {
            if (ir-l < M) {
                for (j=l+1;j<=ir;j++) {
                    a=arr[j];
                    b=brr[j];
                    for (i=j-1;i>=1;i--) {
                        if (arr[i] <= a) break;
                        arr[i+1]=arr[i];
                        brr[i+1]=brr[i];
                    }
                    arr[i+1]=a;
                    brr[i+1]=b;
                }
                if (!jstack) return;
                ir=istack[jstack];
                l=istack[jstack-1];
                jstack -= 2;
            } else {
                k=(l+ir) >> 1;
                SWAP(arr[k],arr[l+1])
                SWAP(brr[k],brr[l+1])
                if (arr[l+1] > arr[ir]) {
                    SWAP(arr[l+1],arr[ir])
                    SWAP(brr[l+1],brr[ir])
                }
                if (arr[l] > arr[ir]) {
                    SWAP(arr[l],arr[ir])
                    SWAP(brr[l],brr[ir])
                }
                if (arr[l+1] > arr[l]) {
                    SWAP(arr[l+1],arr[l])
                    SWAP(brr[l+1],brr[l])
                }
                i=l+1;
                j=ir;
                a=arr[l];
                b=brr[l];
                for (;;) {
                    do i++; while (arr[i] < a);
                    do j--; while (arr[j] > a);
                    if (j < i) break;
                    SWAP(arr[i],arr[j])
                    SWAP(brr[i],brr[j])
                }
                arr[l]=arr[j];
                arr[j]=a;
                brr[l]=brr[j];
                brr[j]=b;
                jstack += 2;
                if (ir-i+1 >= j-l) {
                    istack[jstack]=ir;
                    istack[jstack-1]=i;
                    ir=j-1;
                } else {
                    istack[jstack]=j-1;
                    istack[jstack-1]=l;
                    l=i;
                }
            }
        }
    }

    /** NR crank: gives the rank of each and returns Sum (f^3 - f) where
     * f is # elements in each tie
     */
    void crank(unsigned n, double w[], double * s) {
        unsigned long j=1,ji,jt;
        float t,rank;
        --w;

        *s=0.0;
        while (j < n) {
            if (w[j+1] != w[j]) {
                w[j]=j;
                ++j;
            } else {
                for (jt=j+1;jt<=n && w[jt]==w[j];jt++){}
                rank=0.5*(j+jt-1);
                for (ji=j;ji<=(jt-1);ji++) w[ji]=rank;
                t=jt-j;
                *s += t*t*t-t;
                j=jt;
            }
        }
        if (j == n) w[n]=n;
    }
}

void mrutils::NumericalSummary::plot() {
    if (data.empty()) return;
    mrutils::stringstream ss;
    ss << "Hist of " << name << "; n = " << n;
    mrutils::stats::hist(data,0,poisson,ss.str().c_str());
}

/*************************************************************
 * Functions for functions
 *************************************************************/

namespace {
    class minHelper {
        public:
            minHelper(mrutils::multiDFunc& func, int n, void * data) 
            : ncom(n)
             ,pcom((double*)malloc(8*n))
             ,xicom((double*)malloc(8*n))
             ,xt((double*)malloc(8*n))
             ,func(func)
             ,data(data)
            {
                oneD = fastdelegate::MakeDelegate(this,&minHelper::f1dim);
            }

            ~minHelper() {
                delete pcom;
                delete xicom;
                delete xt;
            }

        public:
            double f1dim(double x,void* data) {
                for (int j=0; j < ncom; ++j) 
                    xt[j]=pcom[j]+x*xicom[j];

                return func(xt,data);
            }

            bool linmin(double *p, double *xi, double *fret) {
                double xx,xmin,fb,fa,bx,ax;

                static const double TOL = 2.0e-4;

                for (int j=0; j < ncom; ++j) {
                    pcom[j]=p[j];
                    xicom[j]=xi[j];
                }

                ax=0.0; xx=1.0;
                fa = oneD(ax,data); *fret = oneD(xx,data);
                mrutils::bracketMinimum(oneD,NULL,&ax,&xx,&bx,&fa,fret,&fb);
                if (!mrutils::minimize(oneD,NULL,ax,xx,bx,&xmin,fret,TOL)) 
                    return false;

                for (int j=0; j < ncom; ++j) {
                    xi[j] *= xmin;
                    p[j] += xi[j];
                }

                return true;
            }

        private:
            const int ncom;
            double * const pcom;
            double * const xicom;
            double * const xt;
            const mrutils::multiDFunc& func;
            mrutils::oneDFunc oneD;
            void * data;
    };
}

namespace mrutils {

    bool minimize(multiDFunc& func, int n, void * data, double * p, double *fret, double tol) {
        int ibig;
        double del,fp,fptt,t;

        static const int ITMAX = 200;

        minHelper helper(func,n,data);

        double * const pt  = (double*)malloc(8*n);
        double * const ptt = (double*)malloc(8*n);
        double * const xit = (double*)malloc(8*n);
        double * const xi  = (double*)malloc(8*n*n);

        // columns of xi are the unit vectors 
        memset(xi,0,8*n*n);
        for (int j=0; j < n; ++j) xi[(n+1)*j] = 1;

        *fret=func(p,data);

        for (int j=0; j < n; ++j) pt[j]=p[j];

        for (int iter=1;;++iter) {
            fp=*fret;
            ibig=0; del=0.0;

            for (int i=0; i < n; ++i) {

                for (int j=0; j < n; ++j) xit[j]=xi[j*n+i];
                fptt = *fret;

                if (!helper.linmin(p,xit,fret)) {
                    delete xit; delete ptt; delete pt; delete xi; return false;
                }

                if (fabs(fptt-(*fret)) > del) {
                    del=fabs(fptt-(*fret));
                    ibig=i;
                }
            }
            if (2.0*fabs(fp-(*fret)) <= tol*(ABS_(fp)+ABS_(*fret))) {
                delete xit; delete ptt; delete pt; delete xi; return true;
            }

            if (iter == ITMAX) 
            { fprintf(stderr,"powell minimization exceeding maximum iterations.\n"); fflush(stderr); }

            for (int j=0; j < n; ++j) {
                ptt[j] = 2.0*p[j]-pt[j];
                xit[j] = p[j]-pt[j];
                pt[j] = p[j];
            }

            fptt=func(ptt,data);
            if (fptt < fp) {
                t=2.0*(fp-2.0*(*fret)+fptt)*SQ(fp-(*fret)-del)-del*SQ(fp-fptt);

                if (t < 0.0) {
                    if (!helper.linmin(p,xit,fret)) {
                        delete xit; delete ptt; delete pt; delete xi; return false;
                    }

                    for (int j=0; j < n; ++j) {
                        xi[j*n+ibig]=xi[j*n+n-1];
                        xi[j*n+n-1]=xit[j];
                    }
                }
            }
        }

        // unreachable
        delete xit; delete ptt; delete pt; delete xi; return false;
    }


    bool minimizeFast(const oneDFunc& func, void * data
        ,double a, double x, double b
        ,double *xmin, double *fx
        ,double fa, double fb
        ,double tolX
        ) {

        static const double TINY = 1.0e-10;
        static const unsigned ITMAX = 100;

        if (a > b) {
            double tmp = a; a = b; b = tmp;
            tmp = fa; fa = fb; fb = tmp;
        }

        for (unsigned i=0; i < ITMAX; ++i) {
            const double da = a - x;
            const double db = b - x;
            const double dfa = fa - *fx;
            const double dfb = fb - *fx;
            const double denom = da * dfb - db * dfa;

            if (denom < TINY && denom > -TINY) {
                fprintf(stderr,"Warning: minimizeFast encountered colinear points\n");
                fflush(stderr);
                return minimize(func,data,a,x,b,xmin,fx,1e-7,tolX);
            }

            const double d = 0.5 * (da*da*dfb - db*db*dfa)/denom;

            if (d > tolX) { a = x; fa = *fx; }
            else if (d < -tolX) { b = x; fb = *fx; }
            else { *xmin = x+d; *fx = func(*xmin,data); return true; }

            x += d; *fx = func(x,data);
        }

        fprintf(stderr,"Error: too many iterations in minimizeFast\n");
        fflush(stderr);
        return false;
    }

    bool minimize(const oneDFunc& func, void * data
        ,double ax, double bx, double cx
        ,double *xmin, double *fx
        ,double tolX
        ,double tol
        ) {
        int iter;
        double a,b,d=0,etemp,fu,fv,fw,p,q,r,tol1,tol2,u,v,w,x,xm;
        double e=0.0;

        static const int ITMAX = 100;
        static const double CGOLD = 0.3819660;
        static const double ZEPS = 1.0e-10;

        a=(ax < cx ? ax : cx);
        b=(ax > cx ? ax : cx);
        x=w=v=bx;
        fw=fv=*fx;
        for (iter=1;iter<=ITMAX;iter++) {
            xm=0.5*(a+b);
            tol2=2.0*(tol1=tol*fabs(x)+ZEPS);
            if (b-a < tolX || fabs(x-xm) <= (tol2-0.5*(b-a))) {
                *xmin=x;
                return true;
            }
            if (fabs(e) > tol1) {
                r=(x-w)*(*fx-fv);
                q=(x-v)*(*fx-fw);
                p=(x-v)*q-(x-w)*r;
                q=2.0*(q-r);
                if (q > 0.0) p = -p;
                q=fabs(q);
                etemp=e;
                e=d;
                if (fabs(p) >= fabs(0.5*q*etemp) || p <= q*(a-x) || p >= q*(b-x))
                    d=CGOLD*(e=(x >= xm ? a-x : b-x));
                else {
                    d=p/q;
                    u=x+d;
                    if (u-a < tol2 || b-u < tol2) {
                        if (xm-x >= 0) d = ABS_(tol1);
                        else d = -ABS_(tol1);
                    }
                }
            } else {
                d=CGOLD*(e=(x >= xm ? a-x : b-x));
            }
            u=(fabs(d) >= tol1 ? x+d : x+(d >= 0?ABS_(tol1):-ABS_(tol1)));
            fu=func(u,data);
            if (fu <= *fx) {
                if (u >= x) a=x; else b=x;
                SHFT(v,w,x,u)
                SHFT(fv,fw,*fx,fu)
            } else {
                if (u < x) a=u; else b=u;
                if (fu <= fw || w == x) {
                    v=w;
                    w=u;
                    fv=fw;
                    fw=fu;
                } else if (fu <= fv || v == x || v == w) {
                    v=u;
                    fv=fu;
                }
            }
        }
        fprintf(stderr,"1d function minimization: too many iterations in brent\n");
        fflush(stderr);
        return false;
    }

    void bracketMinimum(const oneDFunc& func, void * data
            ,double *ax, double *bx, double *cx, double *fa, double *fb, double *fc) {
        double ulim,u,r,q,fu,dum;
        static const double GOLD  = 1.618034;
        static const double GLIMIT = 100.0;
        static const double TINY = 1.0e-20;

        //*fa=func(*ax,data);
        //*fb=func(*bx,data);
        if (*fb > *fa) {
            SHFT(dum,*ax,*bx,dum)
            SHFT(dum,*fb,*fa,dum)
        }
        *cx=(*bx)+GOLD*(*bx-*ax);
        *fc=func(*cx,data);
        while (*fb > *fc) {
            r=(*bx-*ax)*(*fb-*fc);
            q=(*bx-*cx)*(*fb-*fa);

            const double tmp = q - r;
            if (tmp >= 0) {
                u=(*bx)-((*bx-*cx)*q-(*bx-*ax)*r)/
                    (2.0*fabs(tmp > TINY ?tmp :TINY));
            } else {
                u=(*bx)-((*bx-*cx)*q-(*bx-*ax)*r)/
                    (-2.0*fabs(-tmp > TINY ?-tmp :TINY));
            }
            ulim=(*bx)+GLIMIT*(*cx-*bx);
            if ((*bx-u)*(u-*cx) > 0.0) {
                fu=func(u,data);
                if (fu < *fc) {
                    *ax=(*bx);
                    *bx=u;
                    *fa=(*fb);
                    *fb=fu;
                    return;
                } else if (fu > *fb) {
                    *cx=u;
                    *fc=fu;
                    return;
                }
                u=(*cx)+GOLD*(*cx-*bx);
                fu=func(u,data);
            } else if ((*cx-u)*(u-ulim) > 0.0) {
                fu=func(u,data);
                if (fu < *fc) {
                    SHFT(*bx,*cx,u,*cx+GOLD*(*cx-*bx))
                    SHFT(*fb,*fc,fu,func(u,data))
                }
            } else if ((u-ulim)*(ulim-*cx) >= 0.0) {
                u=ulim;
                fu=func(u,data);
            } else {
                u=(*cx)+GOLD*(*cx-*bx);
                fu=func(u,data);
            }
            SHFT(*ax,*bx,*cx,u)
            SHFT(*fa,*fb,*fc,fu)
        }
    }

    double integrate(const oneDFunc& func, void * data, const double a, const double b) {
        const double EPS = 1e-7;
        const int JMAX = 20;
        double s,st=0,ost,os;

        ost = os = -1.0e30;
        for (int j=1; j<=JMAX; ++j) {
            trapzd(func,data,a,b,j,&st);
            s=(4.0*st-ost)/3.0;
            if (fabs(s-os) < EPS*fabs(os)) return s;
            if (s == 0.0 && os == 0.0 && j > 6) return s;
            os=s;
            ost=st;
        }

        fprintf(stderr,"Too many steps in routine qsimp"); fflush(stderr);

        return 0.0;
    }

    bool derivative(oneDCFunc& func, void * data, double x, double h, std::complex<double> * result, double *err) {
        static const double CON = 1.4;
        static const double CON2 = CON*CON;
        static const double BIG = 1e30;
        static const double SAFE = 2;

        typedef std::complex<double> complex;

        complex a[100]; double hh=h; bool ret = false;

        a[0]=(func(x+hh,data)-func(x-hh,data))/(2.0*hh); *err=BIG;
        for (int i=1;i<10;i++) {
            hh /= CON;
            a[i]=(func(x+hh,data)-func(x-hh,data))/(2.0*hh);
            double fac=CON2;
            for (int j=1;j<=i;j++) {
                a[j*10+i]=(a[j*10-10+i]*fac-a[10*j-10+i-1])/(fac-1.0);
                fac=CON2*fac;
                const double errt=std::max(std::abs(a[j*10+i]-a[10*j-10+i]),std::abs(a[10*j+i]-a[10*j-10+i-1]));
                if (errt <= *err) {
                    ret = true;
                    *err=errt;
                    *result=a[10*j+i];
                }
            }
            if (std::abs(a[10*i+i]-a[10*i-10+i-1]) >= SAFE*(*err)) {
                return true;
            }
        }
        return ret;
    }

    bool derivative(const oneDFunc& func, void * data, double x, double h, double * result, double *err) {
        static const double CON = 1.4;
        static const double CON2 = CON*CON;
        static const double BIG = 1e30;
        static const double SAFE = 2;

        double a[100]; double hh=h; bool ret = false;

        a[0]=(func(x+hh,data)-func(x-hh,data))/(2.0*hh); *err=BIG;
        for (int i=1;i<10;i++) {
            hh /= CON;
            a[i]=(func(x+hh,data)-func(x-hh,data))/(2.0*hh);
            double fac=CON2;
            for (int j=1;j<=i;j++) {
                a[j*10+i]=(a[j*10-10+i]*fac-a[10*j-10+i-1])/(fac-1.0);
                fac=CON2*fac;
                const double errt=std::max(fabs(a[j*10+i]-a[10*j-10+i]),fabs(a[10*j+i]-a[10*j-10+i-1]));
                if (errt <= *err) {
                    ret = true;
                    *err=errt;
                    *result=a[10*j+i];
                }
            }
            if (fabs(a[10*i+i]-a[10*i-10+i-1]) >= SAFE*(*err)) {
                return true;
            }
        }
        return ret;
    }

    void boundInverse(const oneDFunc& func, void * data
        ,const double y, double * const x1, double * const x2) throw (ExceptionNumerical) {

        if (*x1 == *x2) throw ExceptionNumerical("boundInverse: initial range has zero width");

        static const double factor = 1.6;
        static const unsigned maxIter = 50;

        double f1 = func(*x1,data) - y, f2 = func(*x2,data) - y;
        for (unsigned i = 0; i < maxIter; ++i) {
            if (f1*f2 < 0) return;
            if (ABS_(f1) < ABS_(f2))
                f1 = func(*x1 += factor * (*x1-*x2),data) - y;
            else 
                f2 = func(*x2 += factor * (*x2-*x1),data) - y;
        }

        throw ExceptionNumerical("boundInverse: failed to find bounds");
    }

    double invertMonotonic(const oneDFunc& func, void *data
        ,const double y, const double xL, const double xU
        ,const double xtol) throw (ExceptionNumerical) {
        static const unsigned maxIt = 100;

        const double fL = func(xL,data), fU = func(xU,data);
        double f1, f2, x1, x2;

        // x1, f1 is best
        if (ABS_(fU - y) < ABS_(fL - y)) {
            f1 = fU; f2 = fL;
            x1 = xU; x2 = xL;
        } else {
            f1 = fL; f2 = fU;
            x1 = xL; x2 = xU;
        }

        for (unsigned i = 0; i < maxIt; ++i) {
            const double dx = (y-f1)*(x2-x1)/(f2-f1);
            x2 = x1; f2 = f1;
            x1 += dx; 

            if (x1 > xU) x1 = 0.5*(x2 + xU); 
            else if (x1 < xL) x1 = 0.5*(x2+xL);

            f1 = func(x1,data);

            if (ABS_(dx) < xtol || f1 == y) return x1;
            if (x2 == x1) throw ExceptionNumerical("invertMonotonic: result may be outside bounds");
        }

        throw ExceptionNumerical("invertMonotonic failed to converge");
    }

    double invertMonotonicWeak(const oneDFunc& func, void *data
        ,double y, double x0, double x1
        ,int smallLarge
        ,double tol
    ) throw (mrutils::ExceptionNumerical) {
        double xp, yp;
        double y0 = (func)(x0,data), y1 = (func)(x1,data);

        if (0==CMP(y0,y1,tol)) {
            if (0==CMP(y0,y,tol)) return (smallLarge<0?x0:x1);
            else throw ExceptionNumerical("invert: value not in range");
        } else if ( (CMP(y1,y,tol)>0 && CMP(y0,y,tol)>0)
            || (CMP(y1,y,tol)<0 && CMP(y0,y,tol)<0) )
            throw ExceptionNumerical("invert: value not in range");

        int cmp  = CMP(y1, y0);

        do {
            xp = x1/2. + x0/2.;
            yp = (func)(xp,data);

            int cmpP = CMP(yp, y, tol);

            if (cmpP == cmp) {
                x1 = xp;
            } else if (cmpP == -cmp) {
                x0 = xp;
            } else {
                if (0==smallLarge) return xp;
                while (x1-x0 > tol) {
                    xp = x1/2. + x0/2.;
                    yp = (func)(xp,data);
                    if (smallLarge < 0) {
                        if ( 0==CMP(yp, y, tol) ) x1 = xp;
                        else x0 = xp;
                    } else {
                        if ( 0==CMP(yp, y, tol) ) x0 = xp;
                        else x1 = xp;
                    }
                }
                return xp;
            }
        } while ( x1-x0 > tol );

        return (smallLarge<0?x0:x1);
    }
}

/**************************************************************
 * Gamma
 **************************************************************/

namespace {
    double lngamma(double x, double& sgngam) {
        double result, a,b,c,p,q,u,w,z,logpi,ls2pi,tmp;
        int i;

        sgngam = 1;
        logpi = 1.14472988584940017414;
        ls2pi = 0.91893853320467274178;
        if( x<-34.0 )
        {
            q = -x;
            w = lngamma(q, tmp);
            p = static_cast<int>(q);
            i = static_cast<int>(p+0.5);
            if( i%2==0 )
            {
                sgngam = -1;
            }
            else
            {
                sgngam = 1;
            }
            z = q-p;
            if( z>0.5 )
            {
                p = p+1;
                z = p-q;
            }
            z = q*sin(PI*z);
            result = logpi-log(z)-w;
            return result;
        }
        if( x<13 )
        {
            z = 1;
            p = 0;
            u = x;
            while(u>=3)
            {
                p = p-1;
                u = x+p;
                z = z*u;
            }
            while(u<2)
            {
                z = z/u;
                p = p+1;
                u = x+p;
            }
            if( z<0 )
            {
                sgngam = -1;
                z = -z;
            }
            else
            {
                sgngam = 1;
            }
            if( u==2 )
            {
                result = log(z);
                return result;
            }
            p = p-2;
            x = x+p;
            b = -1378.25152569120859100;
            b = -38801.6315134637840924+x*b;
            b = -331612.992738871184744+x*b;
            b = -1162370.97492762307383+x*b;
            b = -1721737.00820839662146+x*b;
            b = -853555.664245765465627+x*b;
            c = 1;
            c = -351.815701436523470549+x*c;
            c = -17064.2106651881159223+x*c;
            c = -220528.590553854454839+x*c;
            c = -1139334.44367982507207+x*c;
            c = -2532523.07177582951285+x*c;
            c = -2018891.41433532773231+x*c;
            p = x*b/c;
            result = log(z)+p;
            return result;
        }
        q = (x-0.5)*log(x)-x+ls2pi;
        if( x>100000000 )
        {
            result = q;
            return result;
        }
        p = 1/(x*x);
        if( x>=1000.0 )
        {
            q = q+((7.9365079365079365079365*0.0001*p-2.7777777777777777777778*0.001)*p+0.0833333333333333333333)/x;
        }
        else
        {
            a = 8.11614167470508450300*0.0001;
            a = -5.95061904284301438324*0.0001+p*a;
            a = 7.93650340457716943945*0.0001+p*a;
            a = -2.77777777730099687205*0.001+p*a;
            a = 8.33333333333331927722*0.01+p*a;
            q = q+a/x;
        }
        result = q;
        return result;
    }

    double gammastirf(double x) {
        double result, y,w,v,stir;

        w = 1/x;
        stir = 7.87311395793093628397E-4;
        stir = -2.29549961613378126380E-4+w*stir;
        stir = -2.68132617805781232825E-3+w*stir;
        stir = 3.47222221605458667310E-3+w*stir;
        stir = 8.33333333333482257126E-2+w*stir;
        w = 1+w*stir;
        y = exp(x);
        if( x>143.01608 )
        {
            v = pow(x, 0.5*x-0.25);
            y = v*(v/y);
        }
        else
        {
            y = pow(x, x-0.5)/y;
        }
        result = 2.50662827463100050242*y*w;
        return result;
    }

    double igamma(double a, double x);
}

namespace mrutils {
    double igammac(double a, double x) {
        double result;
        double igammaepsilon;
        double igammabignumber;
        double igammabignumberinv;
        double ans;
        double ax;
        double c;
        double yc;
        double r;
        double t;
        double y;
        double z;
        double pk;
        double pkm1;
        double pkm2;
        double qk;
        double qkm1;
        double qkm2;
        double tmp;

        igammaepsilon = 0.000000000000001;
        igammabignumber = 4503599627370496.0;
        igammabignumberinv = 2.22044604925031308085*0.0000000000000001;
        if( x<=0||a<=0 )
        {
            result = 1;
            return result;
        }
        if( x<1||x<a )
        {
            result = 1-igamma(a, x);
            return result;
        }
        ax = a*log(x)-x-lngamma(a, tmp);
        if( ax<-709.78271289338399 )
        {
            result = 0;
            return result;
        }
        ax = exp(ax);
        y = 1-a;
        z = x+y+1;
        c = 0;
        pkm2 = 1;
        qkm2 = x;
        pkm1 = x+1;
        qkm1 = z*x;
        ans = pkm1/qkm1;
        do
        {
            c = c+1;
            y = y+1;
            z = z+2;
            yc = y*c;
            pk = pkm1*z-pkm2*yc;
            qk = qkm1*z-qkm2*yc;
            if( qk!=0 )
            {
                r = pk/qk;
                t = fabs((ans-r)/r);
                ans = r;
            }
            else
            {
                t = 1;
            }
            pkm2 = pkm1;
            pkm1 = pk;
            qkm2 = qkm1;
            qkm1 = qk;
            if( fabs(pk)>igammabignumber )
            {
                pkm2 = pkm2*igammabignumberinv;
                pkm1 = pkm1*igammabignumberinv;
                qkm2 = qkm2*igammabignumberinv;
                qkm1 = qkm1*igammabignumberinv;
            }
        }
        while(t>igammaepsilon);
        result = ans*ax;
        return result;
    }

    double igamma(double a, double x) {
        double result;
        double igammaepsilon;
        double ans;
        double ax;
        double c;
        double r;
        double tmp;

        igammaepsilon = 0.000000000000001;
        if( x<=0||a<=0 )
        {
            result = 0;
            return result;
        }
        if( x>1&&x>a )
        {
            result = 1-igammac(a, x);
            return result;
        }
        ax = a*log(x)-x-lngamma(a, tmp);
        if( ax<-709.78271289338399 )
        {
            result = 0;
            return result;
        }
        ax = exp(ax);
        r = a;
        c = 1;
        ans = 1;
        do
        {
            r = r+1;
            c = c*x/r;
            ans = ans+c;
        }
        while(c/ans>igammaepsilon);
        result = ans*ax/a;
        return result;
    }

    double gamma(double x) {
        double result, p, pp, q, qq, z, sgngam;
        int i;

        sgngam = 1;
        q = fabs(x);
        if( q>33.0 )
        {
            if( x<0.0 )
            {
                p = static_cast<int>(q);
                i = static_cast<int>(p+0.5);
                if( i%2==0 )
                {
                    sgngam = -1;
                }
                z = q-p;
                if( z>0.5 )
                {
                    p = p+1;
                    z = q-p;
                }
                z = q*sin(PI*z);
                z = fabs(z);
                z = PI/(z*gammastirf(q));
            }
            else
            {
                z = gammastirf(x);
            }
            result = sgngam*z;
            return result;
        }
        z = 1;
        while(x>=3)
        {
            x = x-1;
            z = z*x;
        }
        while(x<0)
        {
            if( x>-0.000000001 )
            {
                result = z/((1+0.5772156649015329*x)*x);
                return result;
            }
            z = z/x;
            x = x+1;
        }
        while(x<2)
        {
            if( x<0.000000001 )
            {
                result = z/((1+0.5772156649015329*x)*x);
                return result;
            }
            z = z/x;
            x = x+1.0;
        }
        if( x==2 )
        {
            result = z;
            return result;
        }
        x = x-2.0;
        pp = 1.60119522476751861407E-4;
        pp = 1.19135147006586384913E-3+x*pp;
        pp = 1.04213797561761569935E-2+x*pp;
        pp = 4.76367800457137231464E-2+x*pp;
        pp = 2.07448227648435975150E-1+x*pp;
        pp = 4.94214826801497100753E-1+x*pp;
        pp = 9.99999999999999996796E-1+x*pp;
        qq = -2.31581873324120129819E-5;
        qq = 5.39605580493303397842E-4+x*qq;
        qq = -4.45641913851797240494E-3+x*qq;
        qq = 1.18139785222060435552E-2+x*qq;
        qq = 3.58236398605498653373E-2+x*qq;
        qq = -2.34591795718243348568E-1+x*qq;
        qq = 7.14304917030273074085E-2+x*qq;
        qq = 1.00000000000000000320+x*qq;
        result = z*pp/qq;
        return result;
    }
}


/**************************************************************
 * Stats
 **************************************************************/

namespace {
    /** This is used by some of the R code 
      * |x| * signum(y)
      */
    inline double fsign(double x, double y) {
        return ((y >= 0) ? fabs(x) : -fabs(x));
    }
}
namespace mrutils {
    namespace stats {
        void acf(std::vector<double>& x_, unsigned lags, std::vector<double> *correls_) {
            const unsigned size = x_.size();
            if (lags > 64) lags = 64;
            if (size <= lags) lags = size-1;

            double SX = 0, SXX = 0; double Sxy[64]; // Sxy starts at [1]
            memset(Sxy,0,sizeof(Sxy));
            for (unsigned i = 0; i < size; ++i) {
                const double x = x_[i];
                const unsigned l = i > lags ?lags :i;

                SX += x; SXX += x*x;
                for (unsigned j = 1; j <= l; ++j) Sxy[j] += x * x_[i-j];
            }

            std::vector<double> correls__;
            std::vector<double> & correls = correls_ == NULL ?*&correls__ :*correls_;

            std::vector<unsigned> lagsPlot;
            double Lx = 0, Lxx = 0, Ux = 0, Uxx = 0;
            for (unsigned l = 1; l <= lags; ++l) {
                const double xL = x_[l-1];
                const double xU = x_[size-l];
                Lx += xL; Lxx += xL*xL; Ux += xU; Uxx += xU*xU;

                const double Sx = SX - Ux;
                const double Sy = SX - Lx;
                const double Sxx = SXX - Uxx;
                const double Syy = SXX - Lxx;
                const int n = size - l;

                lagsPlot.push_back(l);
                correls.push_back( (Sxy[l]*n - Sx*Sy)/sqrt( (Sxx*n-Sx*Sx) * (Syy*n-Sy*Sy) ) );
            }

            const double ci = 1.9599/sqrt((double)size);

            mrutils::Gnuplot g;
            g.setLabels("ACF","Lag","Correlation");
            g.impulse(2,"red").plot(lagsPlot,correls);
            g.lines(1,"blue",mrutils::Gnuplot::LS_DASHED).hline(ci);
            g.lines(1,"blue",mrutils::Gnuplot::LS_DASHED).hline(-ci);
            g.lines(1,"black").hline(0);
        }

        void hist(std::vector<double> & x, unsigned bins, bool poisson, const char * name) {
            if (bins == 0) bins = CEIL(log2((double)x.size()) + 1);
            if (bins < 3) return;

            static const int curveResolution = 1000;

            mrutils::Gnuplot g;
            g.setTitle(name);

            sort(x.begin(), x.end()); 
            double binSize = (x[x.size()-1] - x[0])/bins;
            const unsigned n = x.size();

            // first, plot the cumulative distribution on y2
            std::vector<double> cumY; cumY.resize(x.size());
            for (unsigned i = 0; i < n; ++i) cumY[i] = i/(double)n;
            g.cmd("set grid y2tics xtics");
            g.cmd("set y2tics 0.1");
            g.y2().lines(1,"black").plot(x,cumY);

            NumericalSummary xSummary;
            std::vector<double> y, xp; y.resize(bins, 0);
            for (unsigned b = 0, i = 0; b < bins && i < n; ++b) {
                xp.push_back(x.front() + b * binSize);
                while (i < n && x[i] < xp.back() + binSize) (xSummary.add(x[i++]), y[b]+=1./n/binSize);
            }

            g.steps(1,"black").plot(xp,y);

            double mu = xSummary.mean(), sig = xSummary.stdev();
            std::vector<double> xn, yn; binSize = (x.back()-x.front())/curveResolution;
            for (int i = 0; i < curveResolution; ++i) {
                xn.push_back( x.front() + i * binSize );
                if (poisson) 
                    yn.push_back( stats::dpoisson( xn.back() + binSize, mu ) );
                else
                    yn.push_back( stats::dnorm( xn.back() + binSize, mu, sig ) );
            }

            g.lines(2,"grey").plot(xn, yn);
        }

        double rankCorrel(std::vector<double>& x_, std::vector<double>& y_, double * pval) {
            const unsigned n = x_.size();
            double * x = &x_[0], *y = &y_[0];
            double sf, sg;

            sort2(n,x,y);
            crank(n,x,&sf);
            sort2(n,y,x);
            crank(n,y,&sg);

            double d = 0;
            for (unsigned j = 0; j < n; ++j) d += (x[j] - y[j])*(x[j]-y[j]);

            const unsigned n3n = n*n*n - n;
            const double f = (1. - sf/n3n) * (1. - sf/n3n);

            const double rs = (1. - 6./n3n * (d + (sf+sg)/12.)) / sqrt(f);

            // subtract mean of d, compute sdev
            d -= n3n/6. - (sf+sg)/12.;
            const double sd = n*(n+1)/6. * sqrt( (n-1) * f );
            *pval = 2*pnorm(-ABS_(d),0,sd);

            return rs;
        }

        void qqnorm(std::vector<double> & x, const char * name) {
            if (x.size() < 3) return;

            std::vector<double> y;
            sort(x.begin(), x.end());

            double p = 0, delta = 1./x.size(), q25 = 0, q75 = 0;
            static const double y25 = stats::qnorm(0.25);
            static const double y75 = stats::qnorm(0.75);

            for (std::vector<double>::iterator it = x.begin();
                 it != x.end(); ++it) {
                p += delta; if (p >= 1) p -= delta;
                if ( p <= 0.25 ) q25 = *it;
                if ( p <= 0.75 ) q75 = *it; 

                y.push_back(stats::qnorm(p));
            }

            Gnuplot g;
            mrutils::stringstream ss;
            ss << "Normal Q-Q Plot for " << name << "; Null p-value: " 
               << stats::normalTest(x); 
            g.setLabels(ss.str().c_str(),
                    "Theoretical Quantiles",
                    "Sample Quantiles");
            g.points(1,"black").plot(y,x);

            const double slope = (q75-q25)/(y75-y25);
            g.lines(2, "black").slope(q25 - slope*y25,slope);
        }

        double distTest(std::vector<double> &x, fastdelegate::FastDelegate1<double,double> func, bool withPlot) {
            sort(x.begin(), x.end());

            std::vector<double> xx, fx, fy;

            int j = 0, n = x.size();
            double dt,d=0,dx=0,en = n,ff,c,co = 0;
            double d1;

            while (j < n) {
                d1 = x[j];
                while (j < n && d1 == x[j]) c = ++j/en;

                ff = func(d1);

                if (withPlot) {
                    xx.push_back(d1);
                    fx.push_back(c);
                    fy.push_back(ff);
                }

                dt = std::max(fabs(co - ff), fabs(c - ff));

                if (dt > d) { dx = d1; d = dt; }

                co = c;
            }

            en = sqrt(en);
            double p = probks((en+0.12+0.11/en)*d);

            if (withPlot) {
                mrutils::Gnuplot g;
                mrutils::stringstream ss; 
                ss << "Distribution test, d = " << d << ", pval = " << p;
                g.setLabels(ss.str().c_str());
                g.lines(2,"black").plot(xx,fx);
                g.lines(2,"grey" ).plot(xx,fy);
                g.lines(1,"black").vline(dx);
            }

            return p;
        }

        double distTest(std::vector<double> &x, fastdelegate::FastDelegate2<double,double,double> func, double p1, bool withPlot) {
            sort(x.begin(), x.end());

            std::vector<double> xx, fx, fy;

            int j = 0, n = x.size();
            double dt,d=0,dx=0,en = n,ff,c,co = 0;
            double d1;

            while (j < n) {
                d1 = x[j];
                while (j < n && d1 == x[j]) c = ++j/en;

                ff = func(d1,p1);

                if (withPlot) {
                    xx.push_back(d1);
                    fx.push_back(c);
                    fy.push_back(ff);
                }

                dt = std::max(fabs(co - ff), fabs(c - ff));

                if (dt > d) { dx = d1; d = dt; }

                co = c;
            }

            en = sqrt(en);
            double p = probks((en+0.12+0.11/en)*d);

            if (withPlot) {
                mrutils::Gnuplot g;
                mrutils::stringstream ss; 
                ss << "Distribution test, d = " << d << ", pval = " << p;
                g.setLabels(ss.str().c_str());
                g.lines(2,"black").plot(xx,fx);
                g.lines(2,"grey" ).plot(xx,fy);
                g.lines(1,"black").vline(dx);
            }

            return p;
        }

        double distTest(std::vector<double> &x, fastdelegate::FastDelegate3<double,double,double,double> func, double p1, double p2, bool withPlot) {
            sort(x.begin(), x.end());

            std::vector<double> xx, fx, fy;

            int j = 0, n = x.size();
            double dt,d=0,dx=0,en = n,ff,c,co = 0;
            double d1;

            while (j < n) {
                d1 = x[j];
                while (j < n && d1 == x[j]) c = ++j/en;

                ff = func(d1,p1,p2);

                if (withPlot) {
                    xx.push_back(d1);
                    fx.push_back(c);
                    fy.push_back(ff);
                }

                dt = std::max(fabs(co - ff), fabs(c - ff));

                if (dt > d) { dx = d1; d = dt; }

                co = c;
            }

            en = sqrt(en);
            double p = probks((en+0.12+0.11/en)*d);

            if (withPlot) {
                mrutils::Gnuplot g;
                mrutils::stringstream ss; 
                ss << "Distribution test, d = " << d << ", pval = " << p;
                g.setLabels(ss.str().c_str());
                g.lines(2,"black").plot(xx,fx);
                g.lines(2,"grey" ).plot(xx,fy);
                g.lines(1,"black").vline(dx);
            }

            return p;
        }

        double distTest(std::vector<double> &x, std::vector<double> &y, bool withPlot) {
            sort(x.begin(), x.end()); sort(y.begin(), y.end());

            std::vector<double> xx, xy, fx, fy;

            int n1 = x.size(), n2 = y.size();
            int j1 = 0, j2 = 0;
            double d = 0, dt, dx = 0, dxt = 0, d1, d2, c1=0, c2=0;
            double en1 = n1, en2 = n2;

            while (j1 < n1 && j2 < n2) {
                if ( (d1 = x[j1]) <= (d2 = y[j2]) ) {
                    while (j1 < n1 && x[j1] == d1) c1 = ++j1/en1;
                    if (withPlot) { xx.push_back(d1); fx.push_back(c1); }
                    dxt = d1;
                }

                if (d2 <= d1) {
                    while (j2 < n2 && y[j2] == d2) c2 = ++j2/en2;
                    if (withPlot) { xy.push_back(d2); fy.push_back(c2); }
                    dxt = d2;
                }

                if ((dt=fabs(c2-c1)) > d) { d = dt; dx = dxt; }
            }

            double en = sqrt(en1*en2/(en1+en2));
            double p  = probks((en+0.12+0.11/en)*d);

            if (withPlot) {
                while (j1 < n1) {
                    d1 = x[j1];
                    while (j1 < n1 && x[j1] == d1) c1 = ++j1/en1;
                    if (withPlot) { xx.push_back(d1); fx.push_back(c1); }
                }

                while (j2 < n2) {
                    d2 = y[j2];
                    while (j2 < n2 && y[j2] == d2) c2 = ++j2/en2;
                    if (withPlot) { xy.push_back(d2); fy.push_back(c2); }
                }

                mrutils::Gnuplot g;
                mrutils::stringstream ss; 
                ss << "Distribution test, d = " << d << ", pval = " << p;
                g.setLabels(ss.str().c_str());
                g.lines(2,"black").plot(xx,fx);
                g.lines(2,"grey" ).plot(xy,fy);
                g.lines(1,"black").vline(dx);
            }

            return p;
        }

        double normalTest(std::vector<double> &x) {
            /* polynomial coefficients */
            const static float g[2]  = { -2.273f,.459f };
            const static float c1[6] = { 0.f,.221157f,-.147981f,-2.07119f, 4.434685f, -2.706056f };
            const static float c2[6] = { 0.f,.042981f,-.293762f,-1.752461f,5.682633f, -3.582633f };
            const static float c3[4] = { .544f,-.39978f,.025054f,-6.714e-4f };
            const static float c4[4] = { 1.3822f,-.77857f,.062767f,-.0020322f };
            const static float c5[4] = { -1.5861f,-.31082f,-.083751f,.0038915f };
            const static float c6[3] = { -.4803f,-.082676f,.0030302f };
            //const static float c7[2] = { .164f,.533f };
            //const static float c8[2] = { .1736f,.315f };
            //const static float c9[2] = { .256f,-.00635f };

            sort(x.begin(), x.end());

            const static double smallDouble = 1e-19;

            int n = x.size(), n2 = n / 2;
            double range = x[n - 1] - x[0];

            std::vector<double> aStorage(n, 0);
            double *a = &aStorage[0];

            if (range < smallDouble) return -1;
            if (n > 5000) return -1;
            if (n < 3) return -1;

            double w, w1;

            double summ2, ssumm2;
            double a1, a2, sa, xx, sx;
            double fac, asa, an25, rsn;

            --a; /* so we can keep using 1-based indices */

            if (n == 3) {
                const static double sqrth = sqrt(0.5);
                a[1] = sqrth;
            } else {
                an25 = n + .25;
                summ2 = 0;
                for (int i = 1; i <= n2; ++i) {
                    a[i] = mrutils::stats::qnorm((i - .375f) / an25);
                    summ2 += a[i] * a[i];
                }
                summ2 *= 2;
                ssumm2 = sqrt(summ2);
                rsn = 1. / sqrt((double)n);
                a1 = poly(c1, 6, rsn) - a[1] / ssumm2;

                /* Normalize a[] */
                int i1 = 2;
                if (n > 5) {
                    i1 = 3;
                    a2 = -a[2] / ssumm2 + poly(c2, 6, rsn);
                    fac = sqrt((summ2 - 2 * (a[1] * a[1]) - 2 * (a[2] * a[2]))
                            / (1. - 2 * (a1 * a1) - 2 * (a2 * a2)));
                    a[2] = a2;
                } else {
                    fac = sqrt((summ2 - 2 * (a[1] * a[1])) /
                            ( 1.  - 2 * (a1 * a1)));
                }
                a[1] = a1;
                for (int i = i1; i <= n2; ++i)
                    a[i] /= - fac;
            }

        ///*    If W is input as negative, calculate significance level of -W */
        //    if (w < 0) {
        //        w1 = 1. + w;
        //        w = 1 - w1;
        //        return true;
        //    }

            xx = x[0] / range;
            sx = xx;
            sa = -a[1];
            for (int i = 1, j = n-1; i < n; --j) {
                double xi = x[i] / range;
                if (xx - xi > smallDouble) return -1;
                sx += xi;
                ++i;
                if (i != j)
                    sa += SGN(i - j) * a[MIN_(i,j)];
                xx = xi;
            }


            /*    Calculate W statistic as squared correlation
                between data and coefficients */

            double ssx = 0, sax = 0, ssa = 0, ssassx = 0;

            sa /= n; sx /= n;
            for (int i = 0, j = n-1; i < n; ++i, --j) {
                if (i != j)
                    asa = SGN(i - j) * a[1+MIN_(i,j)] - sa;
                else
                    asa = -sa;
                double xsx = x[i] / range - sx;
                ssa += asa * asa;
                ssx += xsx * xsx;
                sax += asa * xsx;
            }

            ssassx = sqrt(ssa * ssx);
            w1 = (ssassx - sax) * (ssassx + sax) / (ssa * ssx);
            w = 1. - w1;

            /************************
             * calculate significance
             ************************/

            if (n == 3) {/* exact P value : */
                const static double pi6 = 1.90985931710274;/* = 6/pi, was  1.909859f */
                const static double stqr= 1.04719755119660;/* = asin(sqrt(3/4)), was 1.047198f */
                double pw = pi6 * (asin(sqrt(w)) - stqr);
                if(pw < 0.) pw = 0.;
                return pw;
            }

            double y = log(1 - w), m,s;
            xx = log(static_cast<double>(n));

            if (n <= 11) {
                double gamma = poly(g, 2, n);
                if (y >= gamma) {
                    return 1e-99;
                }
                y = -log(gamma - y);
                m = poly(c3, 4, n);
                s = exp(poly(c4, 4, n));
            } else { /* n >= 12 */
                m = poly(c5, 4, xx);
                s = exp(poly(c6, 3, xx));
            }

            return ( 1 - mrutils::stats::normsdist( (y - m) / s ) );
        }

        double qnorm(const double p, double mu, double sigma) {
            double q = p -0.5, r, val;

        /*-- use AS 241 --- */
        /* double ppnd16_(double *p, long *ifault)*/
        /*      ALGORITHM AS241  APPL. STATIST. (1988) VOL. 37, NO. 3

                Produces the normal deviate Z corresponding to a given lower
                tail area of P; Z is accurate to about 1 part in 10**16.

                (original fortran code used PARAMETER(..) for the coefficients
                 and provided hash codes for checking them...)
        */
            if (fabs(q) <= .425) {/* 0.075 <= p <= 0.925 */
                r = .180625 - q * q;
            val =
                    q * (((((((r * 2509.0809287301226727 +
                               33430.575583588128105) * r + 67265.770927008700853) * r +
                             45921.953931549871457) * r + 13731.693765509461125) * r +
                           1971.5909503065514427) * r + 133.14166789178437745) * r +
                         3.387132872796366608)
                    / (((((((r * 5226.495278852854561 +
                             28729.085735721942674) * r + 39307.89580009271061) * r +
                           21213.794301586595867) * r + 5394.1960214247511077) * r +
                         687.1870074920579083) * r + 42.313330701600911252) * r + 1.);
            }
            else { /* closer than 0.075 from {0,1} boundary */

            /* r = min(p, 1-p) < 0.075 */
            if (q > 0) r = 0.5-p+0.5;
            else r = p;

            r = sqrt(- (
            (false && ((true && q <= 0) || (!true && q > 0))) ?
                    p : /* else */ log(r)));
                /* r = sqrt(-log(r))  <==>  min(p, 1-p) = exp( - r^2 ) */

                if (r <= 5.) { /* <==> min(p,1-p) >= exp(-25) ~= 1.3888e-11 */
                    r += -1.6;
                    val = (((((((r * 7.7454501427834140764e-4 +
                               .0227238449892691845833) * r + .24178072517745061177) *
                             r + 1.27045825245236838258) * r +
                            3.64784832476320460504) * r + 5.7694972214606914055) *
                          r + 4.6303378461565452959) * r +
                         1.42343711074968357734)
                        / (((((((r *
                                 1.05075007164441684324e-9 + 5.475938084995344946e-4) *
                                r + .0151986665636164571966) * r +
                               .14810397642748007459) * r + .68976733498510000455) *
                             r + 1.6763848301838038494) * r +
                            2.05319162663775882187) * r + 1.);
                }
                else { /* very close to  0 or 1 */
                    r += -5.;
                    val = (((((((r * 2.01033439929228813265e-7 +
                               2.71155556874348757815e-5) * r +
                              .0012426609473880784386) * r + .026532189526576123093) *
                            r + .29656057182850489123) * r +
                           1.7848265399172913358) * r + 5.4637849111641143699) *
                         r + 6.6579046435011037772)
                        / (((((((r *
                                 2.04426310338993978564e-15 + 1.4215117583164458887e-7)*
                                r + 1.8463183175100546818e-5) * r +
                               7.868691311456132591e-4) * r + .0148753612908506148525)
                             * r + .13692988092273580531) * r +
                            .59983220655588793769) * r + 1.);
                }

            if(q < 0.0)
                val = -val;
                /* return (q >= 0.)? r : -r ;*/
            }
            return mu + sigma * val;
        }

        double uniformTest(int n, double *x) { 
            int i;
            double t,z=0;
            for(i=0;i<n;i++)   {
                t=x[i]*(1.-x[n-1-i]);
                z=z-(i+i+1)*log(t);
            }
            return ( AD(n,-n+z/n) );
        }

        double rpoisson(double mu) {
            static const double one_7 = 0.1428571428571428571;
            static const double one_12 = 0.0833333333333333333;
            static const double one_24 = 0.0416666666666666667;
            static const double a0 = -0.5;
            static const double a1 =  0.3333333;
            static const double a2 = -0.2500068;
            static const double a3 =  0.2000118;
            static const double a4 = -0.1661269;
            static const double a5 =  0.1421878;
            static const double a6 = -0.1384794;
            static const double a7 =  0.1250060;
            static const double M_1_SQRT_2PI = 0.398942280401432677939946059934;


            /* Factorial Table (0:9)! */
            const static double fact[10] =
            {
            1., 1., 2., 6., 24., 120., 720., 5040., 40320., 362880.
            };

            /* These are static --- persistent between calls for same mu : */
            static int l, m;

            static double b1, b2, c, c0, c1, c2, c3;
            static double pp[36], p0, p, q, s, d, omega;
            static double big_l;/* integer "w/o overflow" */
            static double muprev = 0., muprev2 = 0.;/*, muold     = 0.*/

            /* Local Vars  [initialize some for -Wall]: */
            double del, difmuk= 0., E= 0., fk= 0., fx, fy, g, px, py, t, u= 0., v, x;
            double pois = -1.;
            int k, kflag, big_mu, new_big_mu = 0;

            if (mu <= 0.) return 0.;

            big_mu = mu >= 10.;
            if(big_mu)
            new_big_mu = 0;

            if (!(big_mu && mu == muprev)) {/* maybe compute new persistent par.s */

            if (big_mu) {
                new_big_mu = 1;
                /* Case A. (recalculation of s,d,l    because mu has changed):
                 * The poisson probabilities pk exceed the discrete normal
                 * probabilities fk whenever k >= m(mu).
                 */
                muprev = mu;
                s = sqrt(mu);
                d = 6. * mu * mu;
                big_l = floor(mu - 1.1484);
                /* = an upper bound to m(mu) for all mu >= 10.*/
            }
            else { /* Small mu ( < 10) -- not using normal approx. */

                /* Case B. (start new table and calculate p0 if necessary) */

                /*muprev = 0.;-* such that next time, mu != muprev ..*/
                if (mu != muprev) {
                muprev = mu;
                m = (int)(1 > mu ?1 : mu);
                l = 0; /* pp[] is already ok up to pp[l] */
                q = p0 = p = exp(-mu);
                }

                for(;;) {
                /* Step U. uniform sample for inversion method */
                u = runif();
                if (u <= p0)
                    return 0.;

                /* Step T. table comparison until the end pp[l] of the
                   pp-table of cumulative poisson probabilities
                   (0.458 > ~= pp[9](= 0.45792971447) for mu=10 ) */
                if (l != 0) {
                    for (k = (u <= 0.458) ? 1 : (l < m ?l :m);  k <= l; k++)
                    if (u <= pp[k])
                        return (double)k;
                    if (l == 35) /* u > pp[35] */
                    continue;
                }
                /* Step C. creation of new poisson
                   probabilities p[l..] and their cumulatives q =: pp[k] */
                l++;
                for (k = l; k <= 35; k++) {
                    p *= mu / k;
                    q += p;
                    pp[k] = q;
                    if (u <= q) {
                    l = k;
                    return (double)k;
                    }
                }
                l = 35;
                } /* end(repeat) */
            }/* mu < 10 */

            } /* end {initialize persistent vars} */

        /* Only if mu >= 10 : ----------------------- */

            /* Step N. normal sample */
            g = mu + s * rnorm();/* norm_rand() ~ N(0,1), standard normal */

            if (g >= 0.) {
            pois = floor(g);
            /* Step I. immediate acceptance if pois is large enough */
            if (pois >= big_l)
                return pois;
            /* Step S. squeeze acceptance */
            fk = pois;
            difmuk = mu - fk;
            u = runif(); /* ~ U(0,1) - sample */
            if (d * u >= difmuk * difmuk * difmuk)
                return pois;
            }

            /* Step P. preparations for steps Q and H.
               (recalculations of parameters if necessary) */

            if (new_big_mu || mu != muprev2) {
                /* Careful! muprev2 is not always == muprev
               because one might have exited in step I or S
               */
                muprev2 = mu;
            omega = M_1_SQRT_2PI / s;
            /* The quantities b1, b2, c3, c2, c1, c0 are for the Hermite
             * approximations to the discrete normal probabilities fk. */

            b1 = one_24 / mu;
            b2 = 0.3 * b1 * b1;
            c3 = one_7 * b1 * b2;
            c2 = b2 - 15. * c3;
            c1 = b1 - 6. * b2 + 45. * c3;
            c0 = 1. - b1 + 3. * b2 - 15. * c3;
            c = 0.1069 / mu; /* guarantees majorization by the 'hat'-function. */
            }

            if (g >= 0.) {
            /* 'Subroutine' F is called (kflag=0 for correct return) */
            kflag = 0;
            goto Step_F;
            }


            for(;;) {
            /* Step E. Exponential Sample */

            E = rexponential();    /* ~ Exp(1) (standard exponential) */

            /*  sample t from the laplace 'hat'
                (if t <= -0.6744 then pk < fk for all mu >= 10.) */
            u = 2 * runif() - 1.;
            t = 1.8 + fsign(E, u);
            if (t > -0.6744) {
                pois = floor(mu + s * t);
                fk = pois;
                difmuk = mu - fk;

                /* 'subroutine' F is called (kflag=1 for correct return) */
                kflag = 1;

              Step_F: /* 'subroutine' F : calculation of px,py,fx,fy. */

                if (pois < 10) { /* use factorials from table fact[] */
                px = -mu;
                py = pow(mu, pois) / fact[(int)pois];
                }
                else {
                /* Case pois >= 10 uses polynomial approximation
                   a0-a7 for accuracy when advisable */
                del = one_12 / fk;
                del = del * (1. - 4.8 * del * del);
                v = difmuk / fk;
                if (fabs(v) <= 0.25)
                    px = fk * v * v * (((((((a7 * v + a6) * v + a5) * v + a4) *
                              v + a3) * v + a2) * v + a1) * v + a0)
                    - del;
                else /* |v| > 1/4 */
                    px = fk * log(1. + v) - difmuk - del;
                py = M_1_SQRT_2PI / sqrt(fk);
                }
                x = (0.5 - difmuk) / s;
                x *= x;/* x^2 */
                fx = -0.5 * x;
                fy = omega * (((c3 * x + c2) * x + c1) * x + c0);
                if (kflag > 0) {
                /* Step H. Hat acceptance (E is repeated on rejection) */
                if (c * fabs(u) <= py * exp(px + E) - fy * exp(fx + E))
                    break;
                } else
                /* Step Q. Quotient acceptance (rare case) */
                if (fy - u * fy <= py * exp(px - fx))
                    break;
            }/* t > -.67.. */
            }
            return pois;
        }

        /** taken from: http://www.dreamincode.net/code/snippet1446.htm */
        double rnorm(double mu, double sigma) {
            static bool deviateAvailable=false;    //    flag
            static double storedDeviate;            //    deviate from previous calculation
            double polar, rsquared, var1, var2;
            
            //    If no deviate has been stored, the polar Box-Muller transformation is 
            //    performed, producing two independent normally-distributed random
            //    deviates.  One is stored for the next round, and one is returned.
            if (!deviateAvailable) {
                
                //    choose pairs of uniformly distributed deviates, discarding those 
                //    that don't fall within the unit circle
                do {
                    var1=2.0*( double(rand())/double(RAND_MAX) ) - 1.0;
                    var2=2.0*( double(rand())/double(RAND_MAX) ) - 1.0;
                    rsquared=var1*var1+var2*var2;
                } while ( rsquared>=1.0 || rsquared == 0.0);
                
                //    calculate polar tranformation for each deviate
                polar=sqrt(-2.0*log(rsquared)/rsquared);
                
                //    store first deviate and set flag
                storedDeviate=var1*polar;
                deviateAvailable=true;
                
                //    return second deviate
                return var2*polar*sigma + mu;
            }
            
            //    If a deviate is available from a previous call to this function, it is
            //    returned, and the flag is set to false.
            else {
                deviateAvailable=false;
                return storedDeviate*sigma + mu;
            }
        }

        void cumulativeDist(std::vector<double> &x, std::vector<double> &cx, std::vector<double> &freq) {
            if (x.empty()) return;
            sort(x.begin(), x.end());
            cx.push_back(x.front());

            int size = x.size();
            double last = x[0];
            for (int i = 1; i < size; ++i) {
                if (x[i] != last) {
                    freq.push_back(i/size);
                    cx.push_back(x[i]);
                    last = x[i];
                }
            }
            freq.push_back(1);
        }

    }
}

/*************************************************************
 * Fourier Transforms
 *************************************************************/

namespace mrutils {
    namespace fourier {
        void plotTransform(std::complex<double> * out, const unsigned N) {
            std::vector<double> reals; 
            std::vector<double> f;
            reals.reserve(N+1); f.reserve(N+1);

            const double sf = 1./sqrt((double)N);

            // negative part
            for (unsigned i = N/2; i < N; ++i) {
                reals.push_back(std::real(out[i]) * sf);
                f.push_back(-1. * (N - i)/(double)N);
            }

            // positive part
            for (unsigned i = 0; i <= N/2; ++i) {
                reals.push_back(std::real(out[i]) * sf);
                f.push_back(i/(double)N);
            }

            mrutils::Gnuplot g;
            g.setLabels("Fourier Transform","frequency","H(f)");
            g.plot(f,reals);
            g.lines().plot(f,reals);
            g.lines().vline(0);
        }

        void psd(std::complex<double> * out, const unsigned N) {
            std::vector<double> psd; 
            std::vector<double> f;
            psd.reserve(N/2); f.reserve(N/2);

            f.push_back(0);
            psd.push_back(2 * SQ(std::abs(out[0])));

            for (unsigned i = 1; i <= N/2; ++i) {
                f.push_back(i/(double)N);

                const double pos = std::abs(out[i]);
                const double neg = std::abs(out[N-i]);

                psd.push_back((pos*pos + neg*neg)/N);
            }

            mrutils::Gnuplot g;
            g.setLabels("PSD","frequency","|H(f)|^2 + |H(-f)|^2");
            g.plot(f,psd);
            g.lines().plot(f,psd);
        }

        bool fft(std::complex<double> * x, const unsigned nn, int isign) {
            if (nn < 2) return false;

            // check that nn is a power of 2
            if (0 != (nn & (nn-1))) return false;

            // x is a series of doubles real, imag, real, imag
            double * data = (double*)x - 1;

            unsigned n,mmax,m,j,istep,i;
            double wtemp,wr,wpr,wpi,wi,theta;
            double temp,tempi;

            n=nn << 1;
            j=1;
            for (i=1;i<n;i+=2) { /* This is the bit-reversal section of the routine. */
                if (j > i) {
                    SWAP(data[j],data[i]); /* Exchange the two complex numbers. */
                    SWAP(data[j+1],data[i+1]);
                }
                m=nn;
                while (m >= 2 && j > m) {
                    j -= m;
                    m >>= 1;
                }
                j += m;
            }

            mmax=2;
            while (n > mmax) { /* Outer loop executed log2 nn times. */
                istep=mmax << 1;
                theta=isign*(6.28318530717959/mmax); /* Initialize the trigonometric recurrence. */
                wtemp=sin(0.5*theta);
                wpr = -2.0*wtemp*wtemp;
                wpi=sin(theta);
                wr=1.0;
                wi=0.0;
                for (m=1;m<mmax;m+=2) { /* Here are the two nested inner loops. */
                    for (i=m;i<=n;i+=istep) {
                        j=i+mmax; /* This is the Danielson-Lanczos formula. */
                        temp=wr*data[j]-wi*data[j+1];
                        tempi=wr*data[j+1]+wi*data[j];
                        data[j]=data[i]-temp;
                        data[j+1]=data[i+1]-tempi;
                        data[i] += temp;
                        data[i+1] += tempi;
                    }
                    wr=(wtemp=wr)*wpr-wi*wpi+wr; /* Trigonometric recurrence. */
                    wi=wi*wpr+wtemp*wpi+wi;
                }
                mmax=istep;
            }

            return true;
        }
    }
}

bool mrutils::fourier::fft(double * data, const unsigned n, int isign) {
    if (n < 2) return false;

    // check that n is a power of 2
    if (0 != (n & (n-1))) return false;

    unsigned i,i1,i2,i3,i4,np3;
    double c1=0.5,c2,h1r,h1i,h2r,h2i;
    double wr,wi,wpr,wpi,wtemp,theta;

    theta=PI/(double) (n>>1);
    if (isign == 1) {
        c2 = -0.5;
        fft((std::complex<double>*)data,n>>1,1);
    } else {
        c2=0.5;
        theta = -theta;
    }

    --data;

    wtemp=sin(0.5*theta);
    wpr = -2.0*wtemp*wtemp;
    wpi=sin(theta);
    wr=1.0+wpr;
    wi=wpi;
    np3=n+3;
    for (i=2;i<=(n>>2);i++) {
        i4=1+(i3=np3-(i2=1+(i1=i+i-1)));
        h1r=c1*(data[i1]+data[i3]);
        h1i=c1*(data[i2]-data[i4]);
        h2r = -c2*(data[i2]+data[i4]);
        h2i=c2*(data[i1]-data[i3]);
        data[i1]=h1r+wr*h2r-wi*h2i;
        data[i2]=h1i+wr*h2i+wi*h2r;
        data[i3]=h1r-wr*h2r+wi*h2i;
        data[i4] = -h1i+wr*h2i+wi*h2r;
        wr=(wtemp=wr)*wpr-wi*wpi+wr;
        wi=wi*wpr+wtemp*wpi+wi;
    }
    if (isign == 1) {
        data[1] = (h1r=data[1])+data[2];
        data[2] = h1r-data[2];
    } else {
        data[1]=c1*((h1r=data[1])+data[2]);
        data[2]=c1*(h1r-data[2]);
        fft((std::complex<double>*)++data,n>>1,-1);
    }

    return true;
}

/*************************************************************
 * Vectors & Matrices
 *************************************************************/

namespace mrutils {

    void houseTransform(std::vector<double> &M, int rows, int cols, std::vector<double> &diags, double &s, int maxCols, bool returnR) {
        double nrm;

        for (int k = 0; k < cols-1; k++) {
            // Compute 2-norm of k-th column without under/overflow.
            nrm = 0;
            for (int i = k; i < rows; i++)
                nrm = PYTHAG(nrm,M[i*maxCols+k]);

            if (nrm != 0.0) {
                // Form k-th Householder vector.
                if (M[k*maxCols+k] < 0) nrm = -nrm;
                for (int i = k; i < rows; i++) M[i*maxCols+k] /= nrm;
                M[k*maxCols+k] += 1;

                // Apply transformation to remaining columns.
                for (int j = k+1; j < cols-1; j++) {
                    s = 0.0; 
                    for (int i = k; i < rows; i++) s += M[i*maxCols+k]*M[i*maxCols+j];
                    s /= M[k*maxCols+k];
                    for (int i = k; i < rows; i++) M[i*maxCols+j] -= s*M[i*maxCols+k];
                }

                int j = maxCols-1;
                s = 0.0; 
                for (int i = k; i < rows; i++) s += M[i*maxCols+k]*M[i*maxCols+j];
                s /= M[k*maxCols+k];
                for (int i = k; i < rows; i++) M[i*maxCols+j] -= s*M[i*maxCols+k];

                if (returnR) for (int i = k+1; i < MIN_(maxCols,rows); ++i) M[i*maxCols+k] = 0;
            }

            diags[k] = -nrm;
            if (returnR) M[k*maxCols+k] = diags[k];
        }

        int k = maxCols - 1;
        nrm = 0;
        for (int i = cols-1; i < rows; i++)
            nrm = PYTHAG(nrm,M[i*maxCols+k]);
        /*if (nrm != 0.0) {
        // Form k-th Householder vector.
        if (M[(cols-1)*maxCols+k] < 0) nrm = -nrm;
        for (int i = cols-1; i < rows; i++) M[i*maxCols+k] /= nrm;
        M[(cols-1)*maxCols+k] += 1;
        }*/

        diags[k] = -nrm;
        if (returnR) M[(cols-1)*maxCols+k] = diags[k];
        if (returnR) for (int i = cols; i < MIN_(maxCols,rows); ++i) M[i*maxCols+maxCols-1] = 0;
    }

}


/*************************************************************
 * Laplace Transforms
 *************************************************************/

namespace mrutils {
    namespace laplace {
        double invertPostWidder(laplace::laplaceFunc fs, double t) {
            int i, j, k, n, nn;
            double g[1000], e, sum, fun;
            double r, u, h, wt;
            std::complex<double> s;

            nn = 6;
            e = 8.0;
            fun = 0.0;

            for (i=1; i <= nn; ++i) {
                n = 10*i;
                r = pow(10.0,-e/(2*n));
                u = (n+1)/(2*t*n*pow(r,(double)n));
                h = PI/n;
                sum = 0.0;

                for (j = 1; j <= n-1; ++j) {
                    s = (n+1.)*(1.-r*exp(I*h*static_cast<double>(j)))/t;           
                    sum += pow(-1.0,(double)j) * real(fs(s));
                }
                sum = real(fs((n+1)*(1.0-r)*J/t)) + 2*sum + pow(-1.0,(double) n)*
                    real(fs((n+1)*(r+1)*J/t));
                g[i] = u*sum;
            }

            for (k = 1; k <= nn; ++k) {
                wt = pow(-1.0,(double)(nn-k))*
                    pow((double)k,(double)nn)/(factorial(k)
                            *factorial (nn-k));
                fun += wt*g[k];
            }

            return (fun);
        }       

        double invertEuler(laplace::laplaceFunc fs, double t, double* errt) {
            double a,u,x,y,h,avsu,avsu1,sum;
            int ntr,n,k,j;
            static double su[14];
            static double c[] = {-1.0,1.0,11.0,55.0,165.0,330.0,462.0,
                462.0,330.0,165.0,55.0,11.0,1.0};


            a = 19.1;
            ntr = 2000;
            u = exp(a/2)/t;
            x = a/(2*t);
            h = PI/t;
            avsu = avsu1 = 0.0;

            sum = real(fs(J*x))/2.0;
            for (n = 1; n <= ntr; ++n) {
                y = n*h;
                sum += pow(-1.0,(double)n) * real(fs(J*x + I*y));
            }

            su[1] = sum;
            for (k = 1; k <= 12; ++k) {
                n = ntr+k;
                y = n*h;
                su[k+1]=su[k] + pow(-1.0,(double)n) * real(fs(J*x + I*y));
            }

            for(j = 1; j <= 12; ++j) {
                avsu += c[j]*su[j];
                avsu1 += c[j]*su[j+1];
            }

            avsu  = u*avsu/2048;
            avsu1 = u*avsu1/2048;

            if (errt != NULL) *errt = abs(avsu-avsu1)/2.0;

            return (avsu1);
        }
    }
}


/*************************************************************
 * Option Greeks
 *************************************************************/

namespace {
    struct impVol_t {
        impVol_t(const double S, const double X, const double r, const double t, const bool isPut)
        : S(S), X(X), r(r), t(t), isPut(isPut)
        {}

        inline double bsov(double sigma, void *) const {
            return mrutils::financial::blackScholes(S,X,r,t,sigma,isPut);
        }

        const double S, X, r, t; const bool isPut;
    };

    struct impS_t {
        impS_t(const double X, const double r, const double t, const double sigma, const bool isPut)
        : X(X), r(r), t(t), sigma(sigma), isPut(isPut)
        {}

        inline double bsov(double S, void*) const {
            return mrutils::financial::blackScholes(S,X,r,t,sigma,isPut);
        }

        const double X, r, t, sigma; const bool isPut;
    };

}

namespace mrutils {
    namespace financial {

        double impVol(const double OV, const double S, const double X, const double r, const double t, const bool isPut) throw (mrutils::ExceptionNumerical) {
            if (isPut) {
                if (OV < X-S) throw ExceptionNumerical("impVol: option value less than intrinsic!");
            } else if (OV < S-X) throw ExceptionNumerical("impVol: option value less than intrinsic!");
            
            impVol_t d(S,X,r,t,isPut);
            oneDFunc f = fastdelegate::MakeDelegate(&d,&impVol_t::bsov);
            return invertMonotonic(f, NULL, OV, 0, 2);
        }

        double impDelta(const double OV, const double S, const double X, const double r, const double t, const bool isPut) throw (mrutils::ExceptionNumerical) {
            impVol_t d(S,X,r,t,isPut); oneDFunc f = fastdelegate::MakeDelegate(&d,&impVol_t::bsov);
            return optionDelta(S,X,r,t,invertMonotonic(f, NULL, OV, 0, 2),isPut);
        }

        double impS(const double OV, const double X, const double r, const double t, const double sigma, const bool isPut) throw (mrutils::ExceptionNumerical) {
            impS_t d(X,r,t,sigma,isPut); oneDFunc f = fastdelegate::MakeDelegate(&d,&impS_t::bsov);
            return invertMonotonic(f, NULL, OV, X/10, X*10);
        }
    }
}


/**************************************************************
 * Random walk drawdown function 
 **************************************************************/

namespace {
    static double drawdownProbRW_L(const double mu, const double sigma, const double h, const double T) {
        const double cutoff = sigma*sigma/h;

        if (FZYLT(mu,cutoff)) {
            return 0;
        } else if (FZYEQ(mu,cutoff)) {
            return (3*(1-exp(mu*mu*T/(2*sigma*sigma)))/_E);
        } else {
            const double sigma2 = sigma*sigma;
            const double sigma4 = sigma2*sigma2;
            const double h2 = h*h; 
            const double mu2 = mu*mu; 

            const double mult = mu*h/sigma2;
            double eta = mult;

            // smooth functions: guaranteed rapid convergence
            static const double tol = 1e-7;
            for (double d = 1; ABS_(d) > tol;) 
                eta += (d = tanh(eta)*mult - eta);

            const double eta2 = eta*eta;

            return (2*sigma4*eta*sinh(eta)*exp(-mult)/
                (sigma4*eta2-mu2*h2+sigma2*mu*h)
                * (1-exp(-0.5*mu2*T/sigma2+0.5*sigma2*eta2*T/h2)));
        }
    }

}

namespace mrutils {
    namespace financial {
        double drawdownProbRW(double mu, double sigma, double h, const double T) {
            static const double tol = 1e-6;

            if (h < 0) return 1;
            if (FZYEQ(sigma,0)) return (mu*T > h ? 1 : 0);

            // renormalize units st sigma=1
            mu /= sigma; h /= sigma; sigma = 1;

            if (FZYEQ(mu,0)) {
                const double x = h / (PI * sigma * sqrt(T));
                const double x2 = x*x;
                double g = 0;

                for (double n = 0, diff = 1;ABS_(diff) > tol;n += 2) {
                    double a = n + 0.5; double a2 = a*a;
                    diff = (1-exp(-0.5*a2/x2))/a;
                    ++a; a2 = a*a;
                    diff -= (1-exp(-0.5*a2/x2))/a;
                    g += diff;
                }

                return (2*g/PI);
            } else {
                if (FZYEQ(sigma*sigma,mu*h)) {
                    const double t = 1+1e-3;
                    const double delta = t*h/(sigma*sigma) - mu;
                    return (0.5*(drawdownProbRW(mu + delta,sigma,h,T) + drawdownProbRW(mu - delta,sigma,h,T)));
                }

                const double sigma2 = sigma*sigma;
                const double sigma4 = sigma2*sigma2;
                const double mu2 = mu*mu; 
                const double h2 = h*h;
                const double mult = sigma2/(mu*h);

                const double L = drawdownProbRW_L(mu,sigma,h,T);

                const double a = mu*h*(mu*h-sigma2);
                const double b = 2*sigma4*exp(-mu*h/sigma2);
                const double c = exp(-0.5*mu2*T/sigma2);
                const double d = 0.5*sigma2*T/h2;

                double G = 0;

                double upper = PI/2;
                unsigned n = mult < 1 ? 1 : 0;

                for (double diff = 1; ABS_(diff) > tol;) {
                    diff = 0;

                    for (unsigned i = 0; i < 2; ++i, ++n) {

                        const double thetaBase = (n+0.5)*PI;

                        for (double lower = 0; upper-lower > 1e-6; ) {
                            const double mid = (upper+lower)*0.5;
                            const double theta = thetaBase - mid;
                            if (tan(theta) < mult * theta) upper = mid;
                            else lower = mid;
                        }

                        const double theta = thetaBase - upper;

                        diff += theta*sin(theta)/(sigma4*theta*theta+a)*(1-c*exp(-d*theta*theta));
                    }

                    G += diff;
                } 

                return (b*G + L);
            }
        }

    }
}
