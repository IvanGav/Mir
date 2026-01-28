#pragma once

#include "prelude.h"

namespace mem {
    template <typename T>
    T* alloc(usize size) {
        return (T*) ::malloc(sizeof(T) * size);
    };

    template <typename T>
    void free(T* ptr) { 
        ::free(ptr);
    };
    
    template <typename T>
    T* realloc(T* ptr, usize new_size) {
        return (T*) ::realloc((void*) ptr, sizeof(T) * new_size);
    };

    template <typename T>
    void copy(T* to, T* from, usize size) {
        ::memcpy(to, from, sizeof(T) * size);
    }

    template <typename T>
    void swap(T* l1, T* l2) {
        T temp;
        mem::copy(&temp, l1, 1);
        mem::copy(l1, l2, 1);
        mem::copy(l2, &temp, 1);
    }

    // Arena handles must be stored on a stack OR freed manually
    struct Arena {
        u8* data;
        u8* cur;
        u8* end_ptr;

        ~Arena() {
            mem::free(data);
        }
        
        void reset() {
            cur = data;
        }

        static Arena create(usize size) {
            u8* ptr = mem::alloc<u8>(size);
            return Arena { .data = ptr, .cur = ptr, .end_ptr = ptr+size };
        }

        template <typename T>
        T* alloc(usize size) {
            void* ptr = cur;
            usize size_left = end_ptr-cur;
            if (std::align(alignof(T), sizeof(T) * size, ptr, size_left) == nullptr) {
                std::cout << "Fatal: Could not allocate memory" << std::endl;
                exit(1);
            }
            cur = (u8*) ptr + sizeof(T) * size;
            return (T*) ptr;
        };

        template <typename T>
        T* push(T item) {
            void* ptr = this->alloc<T>(1);
            T* tptr = (T*) ptr;
            *tptr = item;
            return tptr;
        };
        
        template <typename T>
        T* realloc(T* ptr, usize last_size, usize new_size) {
            if(last_size >= new_size) {
                // printf("----shrink from %ld to %ld\n", last_size, new_size);
                // Shrink
                return ptr;
            }
            // printf("----cur=%p, ptr=%p, sizeof(T)=%ld, last_size=%ld, expr=%p \n", cur, ptr, sizeof(T), last_size, ((u8*) ptr + sizeof(T)*last_size));
            if(((u8*) ptr + sizeof(T)*last_size) == cur) {
                // printf("----extanding pointer at %p from %ld to %ld\n", ptr, last_size, new_size);
                // no new allocations were made
                cur += (new_size - last_size) * sizeof(T);
                return ptr;
            }
            // reallocate and copy memory
            T* out = this->alloc<T>(new_size);
            mem::copy(out, ptr, last_size);
            // printf("----reallocating and copying at %p (size=%ld) to %p (size=%ld)\n", ptr, last_size, out, new_size);
            return out;
        };

        template <typename T>
        T* clone(T* ptr, usize size) {
            T* cloned = this->alloc<T>(size);
            // ::memcpy(cloned, ptr, sizeof(T) * size);
            mem::copy(cloned, ptr, size);
            return cloned;
        }
    };
};

mem::Arena default_arena = mem::Arena::create(10 MB); // alloc 10 MB