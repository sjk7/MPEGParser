#pragma once
#include <cstdint>
#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

// I know, they are evil ... :-(
#ifndef CAST
#define CAST(type, expr) static_cast<type>(expr)
#endif

#ifndef NO_COPY_AND_ASSIGN
#define NO_COPY_AND_ASSIGN(TypeName)                                           \
    TypeName(const TypeName&) = delete;                                        \
    void operator=(const TypeName&) = delete;
#endif
#ifndef NO_MOVE_AND_ASSIGN
#define NO_MOVE_AND_ASSIGN(TypeName)                                           \
    TypeName(TypeName&&) = delete;                                             \
    void operator=(TypeName&&) = delete;
#endif
namespace my {
namespace io {
    static constexpr int NO_MORE_DATA = -1;
    namespace detail {
        static constexpr int BUFFER_CAPACITY = 1024;
        using byte = unsigned char;
        struct seek_t {

            enum class seek_value_type : int {
                seek_from_begin,
                seek_from_cur,
                seek_from_end
            };
            using value_type = seek_value_type;
            using utype = std::underlying_type_t<seek_value_type>;
            seek_t(int64_t pos = 0,
                seek_value_type sk = seek_value_type::seek_from_cur)
                : position(pos), seek(sk) {}
            seek_t(const seek_t& rhs) = default;
            seek_t(seek_t&& rhs) = default;
            seek_t& operator=(const seek_t& rhs) = default;
            seek_t& operator=(seek_t&& rhs) = default;
            int64_t position = 0;
            // seek_t seek = seek_t::seek_from_cur;
            seek_value_type seek = seek_value_type::seek_from_cur;
            // seek_value_type::seek_from_cur;
            explicit operator seek_value_type() const { return seek; }
        };

        template <typename CRTP, size_t CAPACITY = BUFFER_CAPACITY>
        class buffer_guts {
            public:
            constexpr int capacity() const noexcept {
                return static_cast<int>(CAPACITY);
            }

            protected:
            byte data[CAPACITY] = {0};
            int m_size = 0;
            int m_read = 0;
            int m_unread = 0;
            CRTP& m_crtp;
            void clear() {
                m_size = 0;
                m_read = 0;
                m_unread = 0;
            }

            constexpr int size() const noexcept { return m_size; }

            int unread() const noexcept { return m_unread; }
            int read() const noexcept {
                assert(1);
                return m_read;
            }
            void unread(int new_unread) noexcept {
                m_unread = new_unread;
                assert(m_unread >= new_unread);
                m_size = m_unread;
            }
            void read_set(int new_read) noexcept {
                m_read = new_read;
                m_unread -= read;
            }

            int available_write() noexcept {
                const byte* target = data + m_read;
                return end() - target;
            }
            // returns where to write the next data,
            // and the available space.
            // Returns nullptr if there is no room right now
            char* writeptr(int& available_space) noexcept {

                assert(m_unread >= 0 && m_unread < CAPACITY);
                if (m_unread >= 0 && m_unread >= CAPACITY) {
                    return nullptr;
                }
                available_space = this->available_write();
                return reinterpret_cast<char*>(data + m_read);
            }
            const byte* const data_begin() const noexcept {
                return data + m_read;
            }
            const byte* const data_end() const noexcept {
                return begin() + m_size;
            }
            const byte* begin() const noexcept {
                return CAST(const byte*, data);
            }
            const byte* end() const noexcept {
                return CAST(const byte*, data + CAPACITY);
            }
            bool empty() const noexcept {
                return m_size == 0 || m_read == m_size;
            }
            buffer_guts(CRTP& c) : m_crtp(c) {}
            buffer_guts(const buffer_guts&) = delete;
            buffer_guts& operator=(const buffer_guts&) = delete;
        };
    } // namespace detail

    using seek_type = detail::seek_t;
    using seek_value_type = detail::seek_t::seek_value_type;
    template <typename CRTP> using buffer_guts_type = detail::buffer_guts<CRTP>;
    using byte_type = detail::byte;

    template <size_t SBO_SIZE = 1024> class sbo_buffer {

        byte_type m_sbo_buf[SBO_SIZE] = {0};
        byte_type* m_dyn_buf;
        size_t m_size = {0};
        size_t m_capacity = SBO_SIZE;

        public:
        sbo_buffer() noexcept : m_dyn_buf(nullptr) {}
        sbo_buffer(const sbo_buffer& rhs) = delete;
        sbo_buffer& operator=(const sbo_buffer&) = delete;
        sbo_buffer(const byte_type* const pdata, size_t cb) noexcept
            : sbo_buffer() {
            append(pdata, cb);
        }
        ~sbo_buffer() noexcept { free(m_dyn_buf); }
        size_t size() const noexcept { return m_size; }
        size_t capacity() const noexcept { return m_capacity; }
        byte_type* cdata() const noexcept {
            if (m_dyn_buf)
                return m_dyn_buf;
            else
                return m_sbo_buf;
        }
        byte_type* data() noexcept {
            if (m_dyn_buf)
                return m_dyn_buf;
            else
                return m_sbo_buf;
        }
        byte_type* data_end() noexcept { return data() + m_size; }
        byte_type* begin() noexcept { return data(); }
        byte_type* end() noexcept { return data() + m_size; }
        byte_type* cbegin() const noexcept { return cdata(); }
        byte_type* cend() const noexcept { return cdata() + m_size; }
        byte_type operator[](size_t where) noexcept {
            return *(begin() + where);
        }
        byte_type operator[](size_t where) const noexcept {
            return *(cdata() + where);
        }

        void size_set(size_t sz) { m_size = sz; }

        friend void swap(sbo_buffer& first, sbo_buffer& second) noexcept {
            using std::swap;

            swap(first.m_size, second.m_size);
            swap(first.m_capacity, second.m_capacity);
            swap(first.m_sbo_buf, second.m_sbo_buf);
            swap(first.m_dyn_buf, second.m_dyn_buf);
        }

        void resize(size_t newsz) {
            size_t old_size = size();
            size_t new_size = newsz;

            if (new_size > capacity() || m_dyn_buf) {
                if (m_dyn_buf == nullptr) {
                    m_dyn_buf = (byte_type*)malloc(new_size);

                    if (!m_dyn_buf) {
                        fprintf(stderr,
                            "malloc failed [ %lu ]\n -- not enough memory @ "
                            "%s:%u",
                            new_size, __FILE__, __LINE__);
                        exit(-7);
                    }
                    memcpy(m_dyn_buf, m_sbo_buf, old_size);
                    memset(m_sbo_buf, 0, SBO_SIZE);
                } else {
                    auto* pnew = (byte_type*)realloc(m_dyn_buf, new_size);
                    if (!pnew) {
                        fprintf(stderr,
                            "realloc failed [ %lu ]\n -- not enough memory @ "
                            "%s:%u",
                            new_size, __FILE__, __LINE__);
                    }
                    m_dyn_buf = pnew;
                }
            }

            m_capacity = new_size;
            m_size = new_size;
        }
        void append(const byte_type* const data, size_t cb) noexcept {

            size_t new_size = m_size + cb;
            const size_t old_size = m_size;

            resize(new_size);
            assert(old_size + cb <= capacity());
            bool dyn = m_dyn_buf != nullptr;

            if (dyn) {
                auto* ptr = m_dyn_buf + old_size;
                assert(ptr < end());
                memcpy(ptr, data, cb);
                m_size += cb;
            } else {
                assert(new_size <= SBO_SIZE);
                auto ptr = &this->m_sbo_buf[old_size];
                assert(ptr < end());
                memcpy(ptr, data, cb);
                m_size += cb;
            }
            assert(m_size == old_size + cb);
            assert(m_capacity >= m_size);
        }

    }; // namespace io
} // namespace io
} // namespace my
