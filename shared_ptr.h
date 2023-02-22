
#include <cstddef>

namespace POC {
    template<class T>
        class shared_ptr {
            public:
                /* let's call a shared_ptr owning nothing empty, pointing nothing null
                 * empty shared_ptr <=> pcb->owning is null
                 * null shared_ptr  <=> stored_ptr is null
                 */
                shared_ptr(): stored_ptr(nullptr), pcb(nullptr) {}  // empty and null

                explicit shared_ptr(std::nullptr_t): stored_ptr(nullptr), pcb(nullptr) {}  // empty and null

                explicit shared_ptr(T* p):stored_ptr(p), pcb(new control_block(p)) {
                    pcb->acquire();
                }

                shared_ptr(const shared_ptr& other): stored_ptr(other.stored_ptr), pcb(other.pcb) {
                    if (pcb) pcb->acquire();
                }

                // aliasing constructor
                template<class U>
                    shared_ptr(const shared_ptr& other, U* p): stored_ptr(p), pcb(other.pcb) {
                        pcb->acquire();
                    }

                ~shared_ptr() {
                    stored_ptr = nullptr;
                    if (pcb) {
                        pcb->release();
                        if (0 == pcb->use_count()) {
                            delete pcb;
                        }
                    } 
                    pcb = nullptr;
                }

                shared_ptr& operator=(const shared_ptr& other) {
                    reset();
                    stored_ptr = other.stored_ptr;
                    pcb = other.pcb;
                    if (pcb) pcb->acquire();
                    return *this;
                }

                T* get() {return stored_ptr;}

                void reset() {
                    stored_ptr = nullptr;
                    if (pcb) {
                        pcb->release();
                        if (0 == pcb->use_count()) delete pcb;
                    }
                    pcb = nullptr;
                }

                void reset(T* p) {
                    reset();
                    stored_ptr = p;
                    pcb = new control_block(p);
                }

                long use_count() {return pcb ? pcb->use_count() : 0;}

                T* operator->() {return stored_ptr;}

                T& operator*() {return *stored_ptr;}

            private:
                class control_block {
                    public:
                        explicit control_block(T* p): cnt(0), owned_ptr(p) {}

                        ~control_block() {
                            if (0 == cnt && owned_ptr) {
                                delete owned_ptr;
                                owned_ptr = nullptr;
                            }
                        }

                        long use_count() {return cnt;}

                        void acquire() {++cnt;}

                        void release() {--cnt;}

                    private:
                        long cnt;
                        T* owned_ptr;
                };


            private:
                T* stored_ptr;
                control_block* pcb;
        };
}
