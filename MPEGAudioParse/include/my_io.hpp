#pragma once
#include <cstdint>
#include <cassert>

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

            constexpr int capacity() const {
                return static_cast<int>(CAPACITY);
            }
            constexpr int size() const { return m_size; }
            const byte* const data_begin() const {
                return CAST(const byte*, (data + m_unread));
            }
            // returns where to write the next data,
            // and the available space.
            // Returns nullptr if there is no room right now
            char* writeptr(int& available_space) {

                assert(m_unread >= 0 && m_unread < CAPACITY);
                if (m_unread >= 0 && m_unread >= CAPACITY) {
                    return nullptr;
                }
                byte* target = data + m_unread;
                available_space = CAPACITY - m_unread;
                return reinterpret_cast<char*>(target);
            }
            const byte* const data_end() const { return begin() + m_size; }
            const byte* begin() const { return CAST(const byte*, data); }
            const byte* end() const {
                return CAST(const byte*, data + CAPACITY);
            }
            bool empty() const { return m_size == 0 || m_read == m_size; }
            buffer_guts(CRTP& c) : m_crtp(c) {}
        };
    } // namespace detail

    using seek_type = detail::seek_t;
    using seek_value_type = detail::seek_t::seek_value_type;
    template <typename CRTP> using buffer_guts_type = detail::buffer_guts<CRTP>;
    using byte_type = detail::byte;
} // namespace io
} // namespace my
