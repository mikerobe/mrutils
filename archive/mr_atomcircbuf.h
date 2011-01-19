#ifndef _MR_LIB_ATOMCIRCBUF_H
#define _MR_LIB_ATOMCIRCBUF_H

#include "mr_exports.h"
#include <iostream>
#include "mr_threads.h"

namespace mrutils {

template<class T>
class AtomCircBuf {

    public:
        AtomCircBuf(int size)
        : head(0), tail(0), count(0), size_(size)
        {
            alloc = new T[size];
        }

        ~AtomCircBuf() {
            delete[] alloc;
        }

    public:
       /*****************
        * Main functions
        *****************/

        inline void push(const T& entry) {
            alloc[tail] = entry;
            if (++tail == size_) tail = 0;
            ATOMIC_INCREMENT(count);

            if (count == size_) {
                fprintf(stderr,"Error: AtomCircBuf overflow");
            }

        }

        inline int spaceLeft() 
        { return (size_ - count); }

        inline bool empty() 
        { return (count == 0); }

        inline const T& peek() {
            return alloc[head];
        }

        inline const T& pop() { 
            ATOMIC_DECREMENT(count);
            const T& r = alloc[head];
            if (++head == size_) head = 0;
            return r; 
        }

        ATOMIC_INT count;

    private:
        T* alloc; int size_;
        int head; 
        int tail;

};
}

#endif
