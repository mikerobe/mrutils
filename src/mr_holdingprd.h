#ifndef _MR_HOLDINGPRD_H
#define _MR_HOLDINGPRD_H

#include "mr_exports.h"
#include "mr_numerical.h"

namespace mrutils {


class _API_ HoldingPeriod {
    public:
        HoldingPeriod();

        void add(double time, int qty);

        void reset();

        inline int getPosition() const { return NQ; }

        inline double get(double now) const {
            if (maxQ == 0 && NQ == 0) return 0;

            if (NQ < 0) {
                const int mQ = FZY_INT(maxQ) - NQ;
                return std::abs(n*(maxQ/mQ) + NQ * (now - timeOffset) / mQ);
            } else {
                return std::abs(n + NQ*(now-timeOffset)/maxQ);
            }
        }

        inline double max(double now) const {
            if (NQ == 0) return maxTime;

            const double thisTime = now - startHold;
            return (thisTime > maxTime ?thisTime :maxTime);
        }

    private:
        int NQ;
        double n;
        double maxQ;
        double timeOffset;

        // these are for computing longest holding prd
        double startHold;
        double maxTime;
};

}

#endif
