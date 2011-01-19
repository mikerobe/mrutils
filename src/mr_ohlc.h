#ifndef _MR_CPPLIB_OHLC_H
#define _MR_CPPLIB_OHLC_H

#include <set>
#include "mr_expcircbuf.h"

namespace mrutils {

   /**
    * Given an interval, will provide the open, high, low and close
    * over the previous period of that interval from the last update
    * time
    *
    * Implementation uses a set and an expanding circular buffer whose
    * initial size is passed in the constructor
    */
    class OHLC {
        friend class price_t;

        private:
            struct price_t;

            struct priceHighComp {
                inline bool operator()(const price_t& lhs, const price_t& rhs) const;
            };
            
            struct priceLowComp {
                inline bool operator()(const price_t& lhs, const price_t& rhs) const;
            };

            struct price_t {
                price_t() : ohlc(NULL) {}
                ~price_t();

                double price;
                unsigned milliTime;
                OHLC* ohlc;

                std::set<price_t, priceHighComp>::iterator highIt;
                std::set<price_t, priceLowComp>::iterator lowIt;

                inline bool operator<(const price_t& other) const {
                    return (milliTime < other.milliTime);
                }
            } priceCmp;

        public:
            OHLC(const unsigned interval = 5000u, const unsigned initialSize = 1024)
            : interval(interval)
             ,open(0), high(0), low(0), close(0)
             ,good(false)
             ,lastUpdate(0), lastPrice(0)
             ,buf(initialSize)
            {}

            ~OHLC() {
                buf.clear();
            }

           /**
            * Sets the current time to milliTime & inserts a new price
            * point
            */
            inline void add(const unsigned milliTime, const double price) {
                if (milliTime == lastUpdate && price == lastPrice) return;

                priceCmp.milliTime = milliTime - interval;

                mrutils::ExpCircBuf<price_t>::iterator it = std::upper_bound(buf.begin(), buf.end(), priceCmp);
                if (it != buf.begin()) { good = true; buf.popHead(--it); }

                price_t & spot = buf.next();

                spot.milliTime = milliTime;
                spot.price = price;
                spot.highIt = highs.insert(highs.begin(),spot);
                spot.lowIt = lows.insert(lows.begin(),spot);
                spot.ohlc = this;

                open = buf.head->price;
                high = highs.begin()->price;
                low = lows.begin()->price;
                close = price;

                lastUpdate = milliTime; lastPrice = price;
            }

           /**
            * No new price update, just updates the time 
            */
            inline void update(const unsigned milliTime) {
                if (milliTime == lastUpdate) return;

                priceCmp.milliTime = milliTime - interval;

                mrutils::ExpCircBuf<price_t>::iterator it = std::upper_bound(buf.begin(), buf.end(), priceCmp);
                if (it != buf.begin()) buf.popHead(--it);

                if (!buf.empty()) {
                    open = buf.head->price;
                    high = highs.begin()->price;
                    low = lows.begin()->price;
                    close = buf.tail->price;
                }

                lastUpdate = milliTime; lastPrice = 0;
            }

            inline void print(std::ostream& os = std::cout) {
                os << "OHLC\n";
                os << "    open:  " << open << "\n";
                os << "    high:  " << high << "\n";
                os << "    low:   " << low << "\n";
                os << "    close: " << close << "\n";
            }

            inline friend std::ostream& operator<<(std::ostream& os, const OHLC& ohlc) {
                return os << "OHLC (" << ohlc.open << ", " << ohlc.high << ", " << ohlc.low << ", " << ohlc.close << ")";
            }
        public:
            const unsigned interval;
            double open, high, low, close;
            bool good;

        private:
            unsigned lastUpdate; double lastPrice;

            mrutils::ExpCircBuf<price_t> buf;
            std::set<price_t, priceHighComp> highs;
            std::set<price_t, priceLowComp> lows;
    };

    inline OHLC::price_t::~price_t() {
        if (ohlc != NULL) {
            ohlc->highs.erase(highIt);
            ohlc->lows.erase(lowIt);
        }
    }

    inline bool OHLC::priceHighComp::operator()(const OHLC::price_t& lhs, const OHLC::price_t& rhs) const {
        if (lhs.price == rhs.price) return (&lhs < &rhs);
        return (lhs.price > rhs.price);
    }

    inline bool OHLC::priceLowComp::operator()(const OHLC::price_t& lhs, const OHLC::price_t& rhs) const {
        if (lhs.price == rhs.price) return (&lhs < &rhs);
        return (lhs.price < rhs.price);
    }

}


#endif
