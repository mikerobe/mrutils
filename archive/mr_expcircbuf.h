#ifndef _MR_LIB_EXPCIRCBUF_H
#define _MR_LIB_EXPCIRCBUF_H

#include "mr_exports.h"
#include <iostream>
#include <vector>
#include "mr_circbuf.h"

namespace mrutils {

template <class T>
class ExpandingCircBuf {
    typedef mrutils::CircBuf<T>* BufPtr;
    typedef std::vector<BufPtr> Overflow;
    typedef typename Overflow::iterator OverflowIt;
    typedef typename Overflow::const_iterator OverflowCIt;


    public:
        ExpandingCircBuf(int size)
        : ground(size), head(&ground), tail(&ground), size_(size), count(0) 
        {
            overflow.push_back(&ground);
        }

        ~ExpandingCircBuf() {
            for (OverflowIt it = ++(overflow.begin()); 
                 it != overflow.end(); ++it) delete *it;
        }

        inline void add(const T& entry) {
            if (tail->count == size_) {
                std::cerr << "ExpandableCircBuf is full. expanding..." << std::endl;
                BufPtr n = findEmptyBuffer();
                n->seqNum = tail->seqNum+1;
                tail = n;
            }

            tail->elem[tail->tail] = entry;
            if (++(tail->count) > size_) tail->count = size_;
            if (++(tail->tail) == size_) tail->tail = 0;

            ++count;
        }

        inline const T& remove() { 
            if (tail != head && head->count == 1) {
                --count;
                BufPtr o = head;
                head = findBuffer(head->seqNum+1);
                return o->elem[(o->tail+size_-(o->count--)) % size_];
            } else {
                --count;
                return head->elem[(head->tail+size_-(head->count--)) % size_];
            }
        }

        inline void remove(int num) {
            count -= num;

            while(num > 0) {
                if (num < head->count || (head == tail && num <= head->count)) {
                    head->count -= num;
                    return;
                } else {
                    num -= head->count;
                    head->count = 0;
                    head = findBuffer(head->seqNum+1);
                }
            } 
        }

        inline const T& getFromHead(int offset) const {
            if (offset >= head->count) {
                offset -= head->count;
                BufPtr p = findBuffer(head->seqNum + (offset / size_) + 1);
                return p->elem[(p->tail+size_-p->count+(offset%size_)) % size_];
            } else return head->elem[(head->tail+size_-head->count+offset) % size_];
        }

        /**
         * Returns offset elements back from the last 
         * element added.
         * */
        inline const T& getFromTail(int offset) const {
            if (offset < 0) offset *= -1;

            if (offset >= tail->count) {
                offset -= tail->count;
                BufPtr p = findBuffer(tail->seqNum - (offset / size_) - 1);
                return p->elem[(size_+p->tail-1-(offset%size_)) % size_];
            } else return tail->elem[(size_+tail->tail-1-offset) % size_];
        }

        inline int size() const { return count; }
        inline bool empty() const { return (count == 0); }

        inline void clear() {
            for (OverflowIt it = overflow.begin();
                 it != overflow.end(); ++it) (*it)->clear();
            count = 0;
            head = &ground;
            tail = &ground;
        }

        inline friend std::ostream& operator << (std::ostream& os, const ExpandingCircBuf& b) {
            os << "ExpandingCircBuf (" << b.size() << ") ";
            for (int i = 0; i < b.size(); ++i) os << b.getFromHead(i) << " ";
            return os;
        }

    private:

        inline BufPtr findEmptyBuffer() {
            for (OverflowIt it = overflow.begin();
                 it != overflow.end(); ++it) if ((*it)->empty()) return (*it);
            overflow.push_back(new mrutils::CircBuf<T>(size_));
            return overflow.back();
        }
        inline BufPtr findBuffer(unsigned seqNum) const {
            for (OverflowCIt it = overflow.begin();
                 it != overflow.end(); ++it) if ((*it)->seqNum == seqNum) return (*it);
            std::cerr << "ExpandingCirBuf: can't find next buffer" << std::endl;
            return NULL;
        }
        

    private:
        mrutils::CircBuf<T> ground;
        Overflow overflow;

        BufPtr head, tail;

        int size_;
        int count;
};

}

#endif
