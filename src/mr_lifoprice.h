#ifndef _MR_LIFOPRICE_H
#define _MR_LIFOPRICE_H

#include "mr_exports.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include "mr_numerical.h"

namespace mrutils {

    template <class T = int>
    class LifoPrice_ {
        private:
            struct abscomp {
                inline bool operator()(T q1, T q2) const {
                    if (q1 < 0 || q2 < 0) {
                        return (q1 > q2);
                    } else return (q1 < q2);
                }
            } abscomp;

        public:
            LifoPrice_()
            : position(0)
             ,traded(0)
             ,lp(-1)
            {
                csM.reserve(1024);
                csQ.reserve(1024);
            }

        public:
            inline double getPrice() const 
            { return (lp<0?0:csM[lp]/csQ[lp]); }

            inline T getTraded() const 
            { return (traded - (lp<0?0:csQ[lp])); }

            inline void clear() 
            { lp = -1; csQ.clear(); csM.clear(); traded = 0; position = 0; }

            void add(T q, double p) {
                if (q == 0) return;
                traded += (q>0?q:-q); 

                if (lp < 0) {
                    csQ.push_back(q);
                    csM.push_back(p*q);
                    ++lp; position = q; return;
                } 

                if (SGN(q) == SGN(csQ[lp])) {
                    position = csQ[lp] + q;
                    csQ.push_back(position);
                    csM.push_back(csM[lp] + p*q);
                    ++lp; return;
                } 

                int sgn = SIGN(csQ[lp]+q);
                if (sgn == 0) { lp = -1; csM.clear(); csQ.clear(); position = 0; return; }
                q += csQ[lp];
                if (sgn != SGN(csQ[lp])) {
                    csQ.clear(); csM.clear();
                    csQ.push_back(q);
                    csM.push_back(p*q);
                    lp =0; position = q; return;
                }

                lp = std::lower_bound(csQ.begin(),csQ.end(),q,abscomp) - csQ.begin();
                csQ.resize(lp+1); csM.resize(lp+1);
                if (lp > 0) csM[lp] = csM[lp-1] + (csM[lp]-csM[lp-1])/(csQ[lp]-csQ[lp-1]) * (q-csQ[lp-1]);
                else csM[lp] = csM[lp]/csQ[lp] * q;
                csQ[lp] = q; position = q;
            }

            T position;
            T traded;

        private:
            int lp; // size-1
            std::vector<double> csM;
            std::vector<T> csQ;
    };

    typedef LifoPrice_<int> LifoPrice;
    typedef LifoPrice_<double> LifoPriceDouble;
}

#endif
