#ifndef _MR_CPPLIB_ALLOCATOR_H
#define _MR_CPPLIB_ALLOCATOR_H

#include <vector>
#include <iostream>

namespace mrutils {

/**
  * Implemented as a doubly-linked list
  * with a std::vector backing to allow
  * random removal without iterating
  * and fixed memory allocation created 
  * up front
  */
template<class T>
class Allocator {
    public:
        struct Entry {
            Entry(int n)
            : n(n), next(NULL), prev(NULL)
            {}

            Entry(const Entry& other) 
            : data(other.data), next(NULL), prev(NULL)
             ,n(other.n)
            {}

            T data;
            Entry* next, *prev;
            int n;
        };

    public:
        /**
          * The zero arg is just to allow arrays 
          * of allocators that are later assigned
          * using operator =.
          */
        Allocator()
        : count(0), lastPtr(NULL)
         ,head(NULL), tail(NULL)
        {}

        Allocator(int size) 
        : count(0), lastPtr(NULL)
        {
            if (size <= 1) size = 2;

            entries.resize(size);

            entries[0] = new Entry(0);

            for (int i = 1; i < size; ++i) {
                entries[i] = new Entry(i);
                entries[i]->prev = entries[i-1];
                entries[i-1]->next = entries[i];
            }

            tail = entries[0];
            last = entries[size-1];
            head = tail;
        }

        Allocator(const Allocator<T>& other) 
        : count(other.count), lastPtr(NULL)
        {
            entries.resize(other.entries.size());

            entries[0] = new Entry(*other.entries[0]);

            for (unsigned i = 1; i < entries.size(); ++i) {
                entries[i] = new Entry(*other.entries[i]);
                entries[i]->prev = entries[i-1];
                entries[i-1]->next = entries[i];
            }

            head = entries[other.head->n];
            tail = entries[other.tail->n];
            last = entries[other.last->n];
        }

        Allocator& operator=(const Allocator& other) { 
            if (&other != this) {
                count = other.count;

                // delete existing entries
                for (unsigned i = 0; i < entries.size(); ++i) delete entries[i];

                entries.resize(other.entries.size());

                entries[0] = new Entry(*other.entries[0]);

                for (unsigned i = 1; i < entries.size(); ++i) {
                    entries[i] = new Entry(*other.entries[i]);
                    entries[i]->prev = entries[i-1];
                    entries[i-1]->next = entries[i];
                }

                head = entries[other.head->n];
                tail = entries[other.tail->n];
                last = entries[other.last->n];
            }
            return *this; 
        }

        ~Allocator() {
            for (typename std::vector<Entry*>::iterator it = entries.begin()
                ,itE = entries.end(); it != itE; ++it) delete *it;
        }

    public:
       /********************
        * Iterator
        ********************/

        class iterator {
            public:
                iterator(Allocator<T>* alloc, Entry * e)
                : entry(e), alloc(alloc)
                {}

                inline iterator(const iterator& other)
                : alloc(other.alloc)
                {
                    entry = other.entry;
                }

                inline bool operator==(const iterator& other) 
                { return (entry==other.entry); }

                inline bool operator!=(const iterator& other) 
                { return (entry!=other.entry); }

                // end comparison
                // compare to NULL for NULL lastPtr
                // compare to tail for empty set
                inline bool operator!=(int)
                { return (entry != NULL && entry != alloc->tail); }

                inline bool operator!=(Entry* other) 
                { return (entry!=other); }

                inline T& operator*() 
                { return entry->data; }

                inline T* operator->() 
                { return &entry->data; }

                inline iterator& operator=(const iterator& other) {
                    // self assignment OK
                    entry = other.entry;
                    alloc = other.alloc;
                    return *this;
                }

                // prefix ++it
                inline iterator& operator++() {
                    entry = entry->next;
                    return *this;
                }

                // postfix it++
                inline iterator operator++(int) {
                    iterator old(entry);
                    entry = entry->next;
                    return old;
                }

                inline Entry * getEntry() {
                    return entry;
                }

            private:
                Entry * entry;
                Allocator<T> * alloc;
        };

        inline iterator begin() 
        { return iterator(this, head); }

        // to get the compiler to replace end()
        // calls with 0
        inline static const int end() 
        { return 0; }

    public:
       /********************
        * Fns for memory allocation
        ********************/

        /**
          * Returns a pointer to the tail of
          * the data set, increments size
          * This is assumed to be used to 
          * speed up memory allocation
          */
        inline T& next(int* n) {
            if (tail == last) expand();

            T& result = tail->data;
            *n = tail->n;

            if (count > 0) tail->prev->next = tail;
            tail = tail->next;
            tail->prev->next = lastPtr;

            ++count;
            return result;
        }

        /**
          * Puts an entry at the specified position
          * appending it to the end of the iteration list
          *
          * Note that no checking is done -- make sure that
          * the spot n is not occupied or the whole data
          * structure will get messed up
          */
        inline T& nextAt(int n) {
            if (n+1 >= entries.size()) expand(n+2);
            Entry * entry = entries[n];
            if (entry == tail) return next(&n); 
            entry->prev->next = entry->next;
            entry->next->prev = entry->prev;
            
            if (count == 0) {
                head = entry;
            } else {
                tail->prev->next = entry;
                entry->prev = tail->prev;
            }

            tail->prev = entry;
            entry->next = lastPtr;

            ++count;
            return entry->data;
        }

        inline void release(int n) 
        { release(entries[n]); }

        inline iterator release(iterator it) 
        { return iterator(this,release(it.getEntry())); }

    public:
       /********************
        * Utilizing LL directly
        ********************/

        inline Entry* release(Entry* entry) {

            if (head == entry) {
                if (entry->next == lastPtr) {
                    head = tail;
                } else {
                    head = entry->next;
                    entry->next->prev = entry->prev;
                }
            } else if(entry->next == lastPtr) {
                entry->prev->next = lastPtr;
                tail->prev = entry->prev;
            } else {
                entry->prev->next = entry->next;
                entry->next->prev = entry->prev;
            }

            --count;
            last->next = entry;
            entry->prev = last;
            last = entry;

            return entry->next;
        }

        inline Entry* append() {
            if (tail == last) expand();

            Entry * ret = tail;

            if (count > 0) tail->prev->next = tail;
            tail = tail->next;
            tail->prev->next = lastPtr;

            ++count;
            return ret;
        }

        /**
          * Inserts so that in the iteration process
          * the new entry (returned) comes immediately
          * before the parameter entry.
          */
        inline Entry* nextAt(Entry* entry) {
            if (tail == last) expand();

            Entry * ret = tail;

            tail = tail->next;
            ret->prev->next = lastPtr;

            if (entry == head) {
                head = ret;
            } else {
                entry->prev->next = ret;
                ret->prev = entry->prev;
            }

            entry->prev = ret;
            ret->next = entry;

            ++count;
            return ret;
        }


    public:
       /********************
        * Basic functions
        ********************/

        inline T& operator[](int n) 
        { return entries[n]->data; }

        inline void clear() {
            tail = head;
            count = 0;
        }

        inline int size() { return count; }
        int count;

        // for faster iteration
        Entry* head, *tail;

        // for quick random access starting point
        std::vector<Entry*> entries;

        /**
          * This is used to have the last entry 
          * point to this passed entry in the case
          * of linked allocators
          */
        inline void setLastPtr(Entry* entry) {
            lastPtr = entry;

            if (count > 0) {
                tail->prev->next = lastPtr;
            }
        }

    private:
        inline void expand(size_t size_ = 0) {
            if (entries.size() == 0) {
                fprintf(stderr,"Called expand() on empty allocator! not allowed.\n");
                ::exit(1);
            }

            int first = entries.size();

            entries.resize(std::max(size_,2*entries.size()));

            entries[first] = new Entry(first);
            entries[first]->prev = last;
            last->next = entries[first];

            for (unsigned i = first+1; i < entries.size(); ++i) {
                entries[i] = new Entry(i);
                entries[i]->prev = entries[i-1];
                entries[i-1]->next = entries[i];
            }
            
            last = entries[entries.size()-1];
        }

    private:
        Entry *last;
        Entry *lastPtr;
};


}
#endif
