#ifndef _MR_LIB_EXP_CIRCBUF_H
#define _MR_LIB_EXP_CIRCBUF_H

#include "mr_exports.h"
#include "mr_circbuf.h"

namespace mrutils {
    
    /**
     * ExpCircBuf
     * ==========
     * This is a different implementation than that used before - no
     * complex multiple circular buffers. This just reallocates the
     * whole buffer & ensures everything is contiguous. It's slower on
     * expansion (possibly way slower) but the iterators & all the
     * operations are faster and simpler
     */
    template<class T>
    class _API_ ExpCircBuf : public CircBuf<T> {
        /* These are necessary since CircBuf is a template */

        public:
            ExpCircBuf(const unsigned size)
            : CircBuf<T>(size)
            {}

        public:
            inline friend ExpCircBuf<T>& operator<<(ExpCircBuf<T>& cb, const T& elem) 
            { return cb.add(elem); }

            inline ExpCircBuf<T>& add(const T& elem) {
                if (CircBuf<T>::count == CircBuf<T>::alloc) expand();
                new(CircBuf<T>::tail) T(elem);
                if (++CircBuf<T>::tail == CircBuf<T>::end_) CircBuf<T>::tail = CircBuf<T>::data;
                ++CircBuf<T>::count; return *this;
            }

        protected:
           /**
            * only called when the buffer is completely full, so 
            * head = tail 
            * Note: the const_casts are because CircBuf doesn't have any
            * reason to think that data, end_, alloc, etc. will change
            *
            * Also: this doesn't use realloc since the destructor has to 
            * be called if the memory is moved rather than just expanded...
            */
            inline void expand() {
                fputs("Warning: had to expand() an ExpCircBuf\n",stderr); fflush(stderr);

                T * dataNew = (T*)malloc(sizeof(T)*CircBuf<T>::alloc*2);

                if (CircBuf<T>::tail == CircBuf<T>::data) {
                    for (T* itNew = dataNew, *it = CircBuf<T>::data; it != CircBuf<T>::end_; ++it, ++itNew) 
                    { new(itNew) T(*it); it->~T(); }

                    CircBuf<T>::head = dataNew; CircBuf<T>::tail = dataNew + CircBuf<T>::count;
                } else {
                    T * itNew = dataNew, *it = CircBuf<T>::data;

                    for (;it != CircBuf<T>::tail; ++it, ++itNew) 
                    { new(itNew) T(*it); it->~T(); }

                    CircBuf<T>::tail = itNew; 
                    itNew += CircBuf<T>::alloc;
                    CircBuf<T>::head = itNew;

                    for (;it != CircBuf<T>::end_; ++it, ++itNew) 
                    { new(itNew) T(*it); it->~T(); }
                }

                const unsigned& alloc = CircBuf<T>::alloc;
                const T* const& data = CircBuf<T>::data;
                const T* const& end_ = CircBuf<T>::end_;

                free(CircBuf<T>::data);
                *const_cast<unsigned*>(&alloc) *= 2;
                *const_cast<T**>(&data) = dataNew;
                *const_cast<T**>(&end_) = CircBuf<T>::data + CircBuf<T>::alloc;
            }
    };

}

#endif
