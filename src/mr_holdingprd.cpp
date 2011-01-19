#include "mr_holdingprd.h"

using namespace mrutils;

HoldingPeriod::HoldingPeriod()
: NQ(0), n(0), maxQ(0), timeOffset(0)
 ,startHold(0), maxTime(0)
{}

void HoldingPeriod::reset() {
    NQ = 0; maxQ = 0; n = 0; 
    timeOffset = 0; 
    startHold = 0; maxTime = 0;
}

void HoldingPeriod::add(double time, int qty) {
    if (NQ != 0 && SIGN(NQ + qty) != SGN(NQ)) {
        const double delta = time - startHold;
        if (delta > maxTime) maxTime = delta;
        startHold = time;
    }

    if (NQ == 0) { timeOffset = time; startHold = time; }
    time -= timeOffset;

    double deltaN = ( ABS_(NQ+qty) - ABS_(NQ) ) * time;
    NQ += qty;

    double oldMQ = maxQ;
    if (qty > 0) maxQ += qty;

    if (maxQ == 0) {
        n += deltaN;
    } else {
        if (maxQ > oldMQ) {
            n = (oldMQ==0?n/maxQ:n*(oldMQ/maxQ)) + deltaN / maxQ;
        } else {
            n += deltaN/maxQ;
        }
    }
}

