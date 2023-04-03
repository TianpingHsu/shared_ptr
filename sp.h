
#pragma once

#include <memory>
#include <atomic>

namespace POC {
    /**
     * interfaces of control block
     */
    class control_block_base {
        public:
            virtual void acquire() = 0;
            virtual void acquire_weak() = 0;
            virtual void release() = 0;
            virtual void release_weak() = 0;

            virtual long long use_count() const noexcept = 0;
            virtual long long weak_use_count() const noexcept = 0; 
            virtual bool unique() const noexcept = 0;
            virtual bool expired() const noexcept = 0;

            virtual void* get_deleter() = 0;
    };

    // forword declaration
    template<class T> 
        class weak_ptr;

    template<class T>
        class shared_ptr {
            public:
                using element_type = typename std::remove_extent<T>::type;

                template<class U> friend class weak_ptr;

                /*========================================================
                 * constructors
                 * https://cplusplus.com/reference/memory/shared_ptr/shared_ptr/
                 * =======================================================*/

                /**
                 * default constructor
                 * let's call a shared_ptr owning nothing empty, pointing nothing null
                 * empty shared_ptr <=> _pcb->_owned_ptr is null
                 * null shared_ptr  <=> _stored_ptr is null
                 */
                constexpr shared_ptr() noexcept
                    : _pcb(nullptr), _stored_ptr(nullptr)  // both empty and null
                    {}  

                // from null pointer
                explicit constexpr shared_ptr(std::nullptr_t p) noexcept
                    : shared_ptr()   // delegating constructor
                    {}

                // from pointer
                template<class U> 
                    explicit shared_ptr(U* p)
                    : _stored_ptr(p), _pcb(new control_block<U>(p)) {
                    }

                // with deleter
                template<class U, class D>
                    explicit shared_ptr(U* p, D deleter)
                    : _stored_ptr(p), _pcb(new control_block<U, D>(p, std::move(deleter))) {
                    }

                // with deleter
                template<class D>
                    explicit shared_ptr(std::nullptr_t p, D deleter)
                    : _stored_ptr(p), _pcb(new control_block<std::nullptr_t, D>{p, std::move(deleter)})
                    {}

                // copy constructor
                //shared_ptr (const shared_ptr& x) noexcept;
                //template <class U> shared_ptr (const shared_ptr<U>& x) noexcept;
                explicit shared_ptr(const shared_ptr& other) noexcept
                    : _stored_ptr(other._stored_ptr), _pcb(other._pcb) {
                        if (_pcb) _pcb->acquire(); 
                    }

                template<class U>
                    shared_ptr(const shared_ptr<U> &x) noexcept
                    : _stored_ptr(x._stored_ptr), _pcb(x._pcb){
                        if (_pcb) _pcb->acquire();
                    }

                // from weak_ptr
                template<class U>
                    explicit shared_ptr(const weak_ptr<U> &wp): _stored_ptr(wp._stored_ptr), _pcb(wp._pcb) {
                        if (wp.expired()) {
                            throw std::bad_weak_ptr{};
                        } else {
                            _pcb->acquire();
                        }
                    }

                // move constructor
                template<class U> 
                    shared_ptr(shared_ptr&& other) noexcept
                    : _stored_ptr(other._stored_ptr), _pcb(other._pcb) {
                        other._stored_ptr = nullptr;
                        _pcb = nullptr;
                    }


                // aliasing constructor
                template<class U>
                    shared_ptr(const shared_ptr<U> &x, element_type* p) noexcept 
                    : _stored_ptr(p), _pcb(x._pcb) {
                        if (_pcb) _pcb->acquire();
                    }

                // destructor
                ~shared_ptr() {
                    if (use_count() >= 1) {
                        /* if use_count == 1, then `_pcb->_owned_ptr` should be deleted by specific deleter
                         * if use_count >  1, then just decrease the shared count
                         */
                        _pcb->release();
                    } else {
                        // do nothing
                    }
                }

                // assignment operator
                shared_ptr& operator=(const shared_ptr& other) {
                    shared_ptr tmp(other);
                    swap(tmp);
                    return *this;
                }

                // move assignment
                shared_ptr& operator=(shared_ptr && other) {
                    shared_ptr{std::move(other)}.swap(*this);
                    return *this;
                }

                // https://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
                void swap(shared_ptr& sp) noexcept {
                    using std::swap;  // enable ADL (not necessary here although, but good practice)
                    swap(_stored_ptr, sp._stored_ptr);
                    swap(_pcb, sp._pcb);
                }

                element_type* get() { return _stored_ptr; }

                void reset() noexcept {
                    shared_ptr{}.swap(*this);
                }

                template<class U>
                    void reset(U* p) noexcept {
                        shared_ptr{p}.swap(*this);
                    }

                const long long use_count() const noexcept { return _pcb ? _pcb->use_count() : 0; }

                explicit operator bool() const noexcept { return _stored_ptr ? true : false; }

                template<class U = T, typename = typename std::enable_if<std::is_array<U>::value>::type>
                element_type& operator[](std::ptrdiff_t i) const noexcept {
                    return _stored_ptr[i];
                }

                template<class U = T, typename = typename std::enable_if<!std::is_array<U>::value>::type>
                element_type& operator*() const noexcept {
                    return *_stored_ptr;
                }

                template<class U = T, typename = typename std::enable_if<!std::is_array<U>::value>::type>
                element_type* operator->() const noexcept{
                    return _stored_ptr;
                }

            private:

                template<class U, class Deleter = std::default_delete<T>>
                    class control_block: public control_block_base {
                        public:

                            control_block() = delete;

                            explicit control_block(U* p): owned_ptr(p), _deleter(std::default_delete<T>()) {}

                            explicit control_block(U *p, Deleter d): owned_ptr(p), _deleter(d) {}

                            ~control_block() {  }

                            virtual void acquire() override {
                                _use_count++;
                            }
                            virtual void acquire_weak() override {
                                _weak_use_count++;
                            }
                            virtual void release() override {
                                if (--_use_count == 0) {
                                    if (owned_ptr) _deleter(owned_ptr);
                                    release_weak();
                                }
                            }
                            virtual void release_weak() override {
                                if (--_weak_use_count == 0) delete this;
                            }

                            virtual long long use_count() const noexcept override { return _use_count; }
                            virtual long long weak_use_count() const noexcept override {return _weak_use_count - (_use_count > 0 ? 1 : 0);}
                            virtual bool unique() const noexcept override { return _use_count == 1; }
                            virtual bool expired() const noexcept override { return _use_count == 0; }

                            virtual void* get_deleter() override { return reinterpret_cast<void*>(std::addressof(_deleter)); }

                        private:
                            U*  owned_ptr;
                            std::atomic<long long> _weak_use_count{1}, _use_count{1};
                            Deleter _deleter;
                    };

            private:
                control_block_base *_pcb;
                element_type       *_stored_ptr;
        };

    template<typename T, typename ... Args>
        inline shared_ptr<T> make_shared(Args&& ... args) {
            return shared_ptr<T>(new T{std::forward<Args>(args)...});
        }

    template<class T>
        class weak_ptr {
            using element_type = typename std::remove_extent<T>::type;
            template<class U> 
                friend class shared_ptr;
            public:
                /*========================================================
                 * constructors
                 * https://en.cppreference.com/w/cpp/memory/weak_ptr/weak_ptr
                 * =======================================================*/

                constexpr weak_ptr() noexcept : _pcb(nullptr), _stored_ptr(nullptr) {}

                weak_ptr(const weak_ptr& r) noexcept: _pcb(r._pcb), _stored_ptr(r._stored_ptr) {
                    if (_pcb) _pcb->acquire_weak();
                }

                weak_ptr(weak_ptr&& r) : _pcb(r._pcb), _stored_ptr(r._stored_ptr) {
                    r._pcb = nullptr;
                    r._stored_ptr = nullptr;
                }

                template<class Y>
                explicit weak_ptr(const shared_ptr<Y>& r): _pcb(r._pcb), _stored_ptr(r._stored_ptr) {
                    if (_pcb) _pcb->acquire_weak();
                }

                ~weak_ptr() {
                    if (_pcb) _pcb->release_weak();
                }

                /**
                 * operator=
                 * https://en.cppreference.com/w/cpp/memory/weak_ptr/operator%3D
                 */
                weak_ptr& operator=(const weak_ptr& r) noexcept {
                    weak_ptr{r}.swap(*this);
                    return *this;
                }
                
                template< class Y >
                    weak_ptr& operator=( const weak_ptr<Y>& r ) noexcept {
                        weak_ptr{r}.swap(*this);
                        return *this;
                    }

                template< class Y >
                    weak_ptr& operator=( const shared_ptr<Y>& r ) noexcept {
                        weak_ptr{r}.swap(*this);
                        return *this;
                }

                weak_ptr& operator=( weak_ptr&& r ) noexcept {
                        weak_ptr{std::move(r)}.swap(*this);
                        return *this;
                }
                template< class Y >
                    weak_ptr& operator=( weak_ptr<Y>&& r ) noexcept {
                        weak_ptr{std::move(r)}.swap(*this);
                        return *this;
                }

                void swap(weak_ptr& other) {
                    using std::swap;
                    swap(_pcb, other._pcb);
                    swap(_stored_ptr, other._stored_ptr);
                }

                void reset() {
                    weak_ptr{}.swap(*this);
                }

                // returns the number of shared_ptr objects that manage the object
                long long use_count() const noexcept {
                    return _pcb ? _pcb->use_count() : 0;
                }

                bool expired() const noexcept {
                    return _pcb ? true : _pcb->expired(); 
                }

                shared_ptr<T> lock() const noexcept {
                    return expired() ? shared_ptr<T>{} : shared_ptr<T>{*this};
                }

            private:
                control_block_base *_pcb;
                element_type       *_stored_ptr;
        };
}
