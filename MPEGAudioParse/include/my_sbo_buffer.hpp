#pragma once
// my_sbo_buffer.hpp
#include <cstring> // memset, memcpy, memmove, size_t
#include <cassert> // assert
#include <type_traits> // is_trivially_copiable
#include <cstddef> // required
#include <cstdlib> // malloc
#include <cstdio> // stderr
#include "my_macros.hpp"

namespace my {
namespace io {

    namespace detail {
        static inline int sbo_count = 0;
        using byte_type = char;
        static inline constexpr size_t BUFFER_GUARD = 8;

        template <typename T, size_t SBO_SIZE = 1024> class sbo_buffer {
            static constexpr size_t SBO_SZ_INTERNAL = SBO_SIZE + BUFFER_GUARD;
            byte_type m_sbo_buf[SBO_SZ_INTERNAL] = {0};
            byte_type* m_dyn_buf = {nullptr};

            size_t m_capacity = SBO_SIZE;
            size_t m_dyn_size = {0};
            static_assert(std::is_trivially_copyable_v<T> && sizeof(T) == 1
                    && std::is_trivially_constructible_v<T>,
                "sbo_buffer: bad type for T. Must be 1 byte wide and trivially "
                "assignable");

            byte_type* end_internal() noexcept { return begin() + m_capacity; }

            protected:
            size_t m_size = {0};

            public:
#ifndef SBO_BUFFER_CAN_COPY

            sbo_buffer(const sbo_buffer& rhs) = delete;
            sbo_buffer& operator=(const sbo_buffer&) = delete;
            sbo_buffer(sbo_buffer&& rhs) = delete;
            sbo_buffer& operator=(sbo_buffer&&) = delete;
#else
#error "fixme: implement special members for a copyable sbo_buffer"
#endif
            int m_sbo_index = -1;
            sbo_buffer() noexcept : m_dyn_buf(nullptr) {
                resize(0);
                m_sbo_index = sbo_count++;
            }
            sbo_buffer(const byte_type* const pdata, size_t cb) noexcept {
                if (!pdata) {
                    assert(cb == 0);
                    return;
                }
                if (cb) {
                    assert(pdata);
                }

                append_data(pdata, cb);
            }

#ifdef _MSC_VER
#pragma warning(disable : 26408)
#endif

            ~sbo_buffer() noexcept {
                if (m_dyn_buf) {
                    free(m_dyn_buf);
                    m_dyn_buf = nullptr;
                }
            }

            // just like vector, clear() does not free any memory
            void clear() noexcept { m_size = 0; }
            constexpr size_t size() const noexcept { return m_size; }
            constexpr int size_i() const noexcept {
                const int i = {CAST(const int, m_size)};
                return i;
            }
            constexpr size_t capacity() const noexcept {
                return m_capacity - BUFFER_GUARD > 0 ? m_capacity - BUFFER_GUARD
                                                     : 0;
            }
            constexpr int capacity_i() const noexcept {
                return m_capacity - BUFFER_GUARD > 0
                    ? static_cast<int>(m_capacity)
                        - static_cast<int>(BUFFER_GUARD)
                    : (int)0;
            }
            const byte_type* cdata() const noexcept {
                if (m_dyn_buf) {
                    return m_dyn_buf;
                }
                { return &m_sbo_buf[0]; }
            }

            byte_type* data() noexcept {
                return m_dyn_buf ? m_dyn_buf : &m_sbo_buf[0];
            }
            byte_type* data_begin() noexcept { return data(); }
            byte_type* data_end() noexcept { return data() + m_size; }
            byte_type* begin() noexcept { return data(); }
            byte_type* end() noexcept { return data() + m_size; }
            const byte_type* cbegin() const noexcept { return cdata(); }
            const unsigned char* cbeginc() const noexcept {
                return reinterpret_cast<const unsigned char*>(cbegin());
            }
            const unsigned char* cendc() const noexcept {
                return reinterpret_cast<const unsigned char*>(cend());
            }
            const byte_type* cend() const noexcept { return cdata() + m_size; }
            byte_type& operator[](size_t where) noexcept {
                return *(begin() + where);
            }
            const byte_type& operator[](size_t where) const noexcept {
                return *(cdata() + where);
            }

            void swap(sbo_buffer& l, sbo_buffer& r) {
                using std::swap;
                swap(l.m_sbo_buf, r.m_sbo_buf);
                swap(l.m_size, r.m_size);
                swap(l.m_capacity, r.m_capacity);
                swap(l.m_dyn_buf, r.m_dyn_buf);
            }
            void mv(sbo_buffer& other) {

                if (m_dyn_buf) {
                    free(m_dyn_buf);
                    m_dyn_buf = nullptr;
                }
                if (!other.m_dyn_buf) {
                    memcpy(m_sbo_buf, other.m_sbo_buf, SBO_SIZE);
                    m_dyn_buf = nullptr;
                } else {
                    m_dyn_buf = other.m_dyn_buf;
                }
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                other.m_size = 0;
            }

            void reserve(size_t sz) {
                // make the buffer if you need to,
                // but this isn't a real resize, so set the m_size = 0;
                resize(sz);
                m_size = 0;
            }

            void size_set(size_t sz) noexcept {
                m_size = sz;
                const auto cap = capacity();
                assert(m_size <= cap && "size cannot be bigger than capacity");
            }

            friend void swap(sbo_buffer& first, sbo_buffer& second) noexcept {
                using std::swap;

                swap(first.m_size, second.m_size);
                swap(first.m_capacity, second.m_capacity);
                swap(first.m_sbo_buf, second.m_sbo_buf);
                swap(first.m_dyn_buf, second.m_dyn_buf);
            }
#ifdef _MSC_VER
//#pragma warning(disable : 26408)
//#pragma warning(disable : 6386)
#endif
            void resize(size_t newsz) noexcept {
                const size_t old_size = m_size;
                const size_t new_size = newsz;
                const auto cap = m_capacity;
                // if (newsz == 1045 && old_size == 1037) {
                //    puts("checkme\n");
                //}

                if (new_size > cap || m_dyn_buf) {
                    if (m_dyn_buf == nullptr) {

                        if (m_dyn_size == 0) {
                            assert(new_size >= old_size);
                            printf("call to alloc() : %zu\n", new_size);
                            m_dyn_buf = static_cast<byte_type*>(
                                malloc(new_size + BUFFER_GUARD));
                            if (m_dyn_buf == nullptr) {
                                errno = ENOMEM;
                                perror("Out of memory line 179");
                                exit(-1);
                            }
                            memset(m_dyn_buf, 0, new_size + BUFFER_GUARD);
                            memcpy(m_dyn_buf, &m_sbo_buf[0], old_size);
                        }
                        m_dyn_size = new_size + BUFFER_GUARD;
                    } else {
                        auto* pnew = begin();
                        const auto new_actual = new_size + BUFFER_GUARD;

                        if (new_actual > m_dyn_size) {
                            const size_t mysize = new_actual;
                            printf("call to realloc() : %zu\n", new_size);
                            auto ptr = realloc(m_dyn_buf, mysize);
                            pnew = CAST(byte_type*, ptr);
                            if (pnew == nullptr) {
                                fprintf(stderr,
                                    "realloc failed [ %lu ]\n -- not enough "
                                    "memory @ "
                                    "%s:%ul",
                                    static_cast<unsigned long>(new_size),
                                    __FILE__, __LINE__);
                            }
                            m_dyn_buf = pnew;

                            if (m_dyn_size == 0 && m_dyn_buf) {
                                memcpy(m_dyn_buf, &m_sbo_buf[0], old_size);
                            }
                            m_dyn_size = new_size + BUFFER_GUARD;
                        } else {
                            pnew = m_dyn_buf;
                        }
                    }
                }

                if (new_size > capacity() || old_size == 0) {
                    if (new_size > m_capacity) {
                        m_capacity = new_size + BUFFER_GUARD;
                    }

                    // this is allowed, because we over-allocate by BUFFER_GUARD
#ifndef NDEBUG
                    // puts("wrote buf guard\n");
                    memcpy(
                        end_internal() - BUFFER_GUARD, "BADF00D", BUFFER_GUARD);
#endif
                }

#ifndef NDEBUG
                const char* ps = end_internal() - 8;
                // if (old_size != 0u) {
                const auto mr = memcmp(ps, "BADF00D", BUFFER_GUARD);
                assert(mr == 0);
                // }
#endif

                m_size = new_size;
            }

#ifdef _MSC_VER
#pragma warning(default : 6386)
#endif
            void append_data(
                const byte_type* const data, size_t cb = 0) noexcept {

                if (data && cb == 0) {
                    fprintf(stderr,
                        "sbo_buffer::append(), inefficent use for value:\n%s\n "
                        "-- can't "
                        "you send the "
                        "length?\n",
                        (const char*)data);
                    cb = strlen(data);
                }

                size_t new_size = m_size + cb;
                const size_t old_size = m_size;
                const auto cap = capacity();

                resize(new_size);
                assert(old_size + cb <= capacity());
                bool dyn = m_dyn_buf != nullptr;

                if (dyn) {
                    auto* ptr = m_dyn_buf + old_size;
                    assert(ptr < end());
                    memcpy(ptr, data, cb);
                } else {
                    // printf("%d\n", (int)SBO_SIZE);
                    assert(new_size <= capacity());
                    auto ptr = &this->m_sbo_buf[old_size];
                    if (old_size == 0) {
                        assert(ptr <= end());
                    } else {
                        assert(ptr < end());
                    }

                    memcpy(ptr, data, cb);
                }

                assert(m_size == old_size + cb);
                assert(m_capacity >= m_size);
            }
        };
    } // namespace detail
} // namespace io
} // namespace my
