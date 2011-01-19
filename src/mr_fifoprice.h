#ifndef _MR_FIFOPRICE_H
#define _MR_FIFOPRICE_H

#include "mr_exports.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include "mr_numerical.h"

namespace mrutils {

    template <class T = int>
    class FifoPrice_ {
        private:
            struct abscomp {
                inline bool operator()(T q1, T q2) const {
                    if (q1 < 0 || q2 < 0) {
                        return (q1 > q2);
                    } else return (q1 < q2);
                }
            } abscomp;

        public:
            FifoPrice_()
            : position(0), o(0), b(0)
            {
                csM.reserve(1024);
                csQ.reserve(1024);
            }

            inline void add(T q, double p) {
                if (SIGN(position+q) != SIGN(position)) {
                    clear_();
                    q += position;
                    position = 0;
                }

                if (q == 0) return;

                if (position == 0 || SGN(position) == SGN(q)) {
                    position += q;
                    csM.push_back((csM.empty()?0:csM.back()) + q*p);
                    csQ.push_back((csQ.empty()?0:csQ.back()) + q);
                } else {
                    position += q;
                    o -= q;
                    b = std::lower_bound(csQ.begin()+b,csQ.end(),o,abscomp) - csQ.begin();
                }
            }

            inline double getPrice() const {
                if (position == 0) return 0;
                return ( (
                    csM.back() - csM[b] + 
                    (csQ[b] - o)/static_cast<double>(csQ[b] - (b>0?csQ[b-1]:0))
                      * (csM[b] - (b>0?csM[b-1]:0))
                    ) / position );
            }

            inline void clear() {
                clear_(); position = 0;
            }

            T position;

        private:
            inline void clear_() {
                csM.clear();
                csQ.clear();
                b = 0; o = 0; 
            }

        private:
            std::vector<double> csM;
            std::vector<T> csQ;
            T o,b;
    };

    typedef FifoPrice_<int> FifoPrice;
    typedef FifoPrice_<double> FifoPriceDouble;
}

#endif
