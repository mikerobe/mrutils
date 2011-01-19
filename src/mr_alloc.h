#ifndef _MR_CPPUTILS_ALLOC_H
#define _MR_CPPUTILS_ALLOC_H

#include <vector>

namespace mrutils {
    template <class T> 
    class Alloc;
}

template<class T> 
inline void * operator new(size_t sz, mrutils::Alloc<T>& a);

namespace mrutils {
    template <class T>
    class Alloc {
        friend void * ::operator new<>(size_t sz, Alloc& a);

        private:
            struct Entry {
                Entry *next;
                char elem[sizeof(T)];
            };

        public:
            Alloc(unsigned maxSize = 32)
            : data(maxSize)
             ,head(&data[0])
            {
                // init connections
                data[maxSize-1].next = NULL;
                for (Entry * it = &data[0], * itE = &data[maxSize-1]
                    ;it != itE; ++it) it->next = it + 1;
            }

            inline void release(void * ptr) {
                if (ptr == NULL) return;
                Entry * e = &data[((char*)ptr - (char*)&data[0])/sizeof(Entry)];
                e->next = head; head = e;
                ((T*)&e->elem)->~T(); 
            }

        private:
            inline void * alloc() throw(std::bad_alloc) {
                if (head == NULL) throw std::bad_alloc();   
                void * elem = (void*)&head->elem;
                head = head->next;
                return elem;
            }

        private:
            std::vector<Entry> data;
            Entry * head;
    };

}

template<class T> 
inline void * operator new(size_t sz, mrutils::Alloc<T>& a) {
    return a.alloc();
}


#endif
