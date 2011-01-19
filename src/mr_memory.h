#ifndef _MR_CPPLIB_MEMORY_H
#define _MR_CPPLIB_MEMORY_H

namespace mrutils {

   /**
    * scope_ptr_array is a very simple, small wrapper. 
    * Used for, e.g., scoped char * buffers: 
    * scope_ptr_array<char> buffer(new char[256])
    */
    template<class T>
    struct scope_ptr_array {
        scope_ptr_array(T* ptr)
        : ptr(ptr)
        {}
        ~scope_ptr_array() { delete[] ptr; }

        T* ptr;

        inline operator T* () { return ptr; }
        inline T& operator*() const { return *ptr; }
        inline T* operator->() { return ptr; }

        private:
            scope_ptr_array(const scope_ptr_array&);
            inline scope_ptr_array& operator=(const scope_ptr_array&);
    };

    template<class T>
    inline std::ostream& operator<<(std::ostream& os, const scope_ptr_array<T>& arr) {
        return (os << arr.ptr);
    }
}

#endif
