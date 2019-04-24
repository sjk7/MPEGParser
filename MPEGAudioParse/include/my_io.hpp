#pragma once
#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include "my_sbo_buffer.hpp"
#include "my_macros.hpp"

namespace my {
namespace io {
    static constexpr int NO_MORE_DATA = -7654;
    namespace detail {
        static constexpr int BUFFER_CAPACITY = 1024;

        struct seek_t {

            enum class seek_value_type : int {
                seek_invalid = -1,
                seek_from_begin,
                seek_from_cur,
                seek_from_end
            };
            using value_type = seek_value_type;
            using utype = std::underlying_type_t<seek_value_type>;
            seek_t(int64_t pos = -1,
                seek_value_type sk = seek_value_type::seek_invalid) noexcept
                : position(pos), seek(sk) {}
            seek_t(const seek_t& rhs) = default;
            seek_t(seek_t&& rhs) = default;
            seek_t& operator=(const seek_t& rhs) = default;
            seek_t& operator=(seek_t&& rhs) = default;
            int64_t position = 0;
            // seek_t seek = seek_t::seek_from_cur;
            seek_value_type seek = seek_value_type::seek_from_cur;
            // seek_value_type::seek_from_cur;
            explicit operator seek_value_type() const noexcept { return seek; }
        };

        template <typename CRTP, size_t CAPACITY = BUFFER_CAPACITY>
        class buffer_guts : public my::io::detail::sbo_buffer<byte_type, CAPACITY> {

            public:
            buffer_guts(const buffer_guts&) = delete;
            buffer_guts& operator=(const buffer_guts&) = delete;

            protected:
            CRTP& m_crtp;
            buffer_guts(CRTP& c) noexcept : m_crtp(c) {}
        };
    } // namespace detail

    using seek_type = detail::seek_t;
    using seek_value_type = detail::seek_t::seek_value_type;
    template <typename CRTP> using buffer_guts_type = detail::buffer_guts<CRTP>;
    using byte_type = my::io::detail::byte_type;

    template <typename T, size_t SZ> using sbo_buf = my::io::detail::sbo_buffer<T, SZ>;

} // namespace io
} // namespace my
