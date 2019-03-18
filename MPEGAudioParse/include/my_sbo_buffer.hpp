#pragma once
// my_sbo_buffer.hpp

namespace my {
namespace io {

    namespace detail {
        using byte_type = char;
        static inline constexpr int BUFFER_GUARD = 8;

        template <typename T, size_t SBO_SIZE = 1024> class sbo_buffer {
            static constexpr size_t SBO_SZ_I = SBO_SIZE + BUFFER_GUARD;
            byte_type m_sbo_buf[SBO_SIZE + BUFFER_GUARD] = {0};
            byte_type* m_dyn_buf = {nullptr};
            size_t m_size = {0};
            static inline size_t m_capacity = SBO_SZ_I;
            static_assert(std::is_trivially_copyable_v<T> && sizeof(T) == 1
                    && std::is_trivially_constructible_v<T>,
                "sbo_buffer: bad type for T. Must be 1 byte wide and trivially "
                "assignable");

            byte_type* end_internal() { return begin() + m_capacity; }

            public:
            sbo_buffer() noexcept : m_dyn_buf(nullptr) { resize(0); }
            sbo_buffer(const sbo_buffer& rhs) = delete;
            sbo_buffer& operator=(const sbo_buffer&) = delete;
            sbo_buffer(const byte_type* const pdata, size_t cb) noexcept : sbo_buffer() {
                append(pdata, cb);
            }
            ~sbo_buffer() noexcept { free(m_dyn_buf); }

            // just like vector, clear() does not free any memory
            void clear() { m_size = 0; }
            size_t size() const noexcept { return m_size; }
            size_t capacity() const noexcept { return m_capacity; }
            byte_type* cdata() const noexcept {
                if (m_dyn_buf)
                    return m_dyn_buf;
                else
                    return m_sbo_buf;
            }
            byte_type* data() noexcept {
                if (m_dyn_buf) {
                    return m_dyn_buf;
                }
                { return m_sbo_buf; }
            }
            byte_type* data_begin() noexcept { return data(); }
            byte_type* data_end() noexcept { return data() + m_size; }
            byte_type* begin() noexcept { return data(); }
            byte_type* end() noexcept { return data() + m_size; }
            byte_type* cbegin() const noexcept { return cdata(); }
            byte_type* cend() const noexcept { return cdata() + m_size; }
            byte_type operator[](size_t where) noexcept { return *(begin() + where); }
            byte_type operator[](size_t where) const noexcept {
                return *(cdata() + where);
            }

            void size_set(size_t sz) noexcept { m_size = sz; }

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
                        m_dyn_buf
                            = static_cast<byte_type*>(malloc(new_size + BUFFER_GUARD));

                        if (m_dyn_buf == nullptr) {
                            fprintf(stderr,
                                "malloc failed [ %lu ]\n -- not enough memory @ "
                                "%s:%ul",
                                static_cast<unsigned long>(new_size), __FILE__, __LINE__);
                            exit(-7);
                        }
                        memcpy(m_dyn_buf, m_sbo_buf, old_size);
                        memset(m_sbo_buf, 0, SBO_SIZE);
                    } else {
                        auto* pnew = reinterpret_cast<byte_type*>(
                            realloc(m_dyn_buf, new_size + BUFFER_GUARD));
                        if (pnew == nullptr) {
                            fprintf(stderr,
                                "realloc failed [ %lu ]\n -- not enough memory @ "
                                "%s:%ul",
                                static_cast<unsigned long>(new_size), __FILE__, __LINE__);
                        }
                        m_dyn_buf = pnew;
                    }
                }

                if (new_size > m_capacity || new_size == 0) {
                    if (new_size > m_capacity) {
                        m_capacity = new_size;
                    }

                    // this is allowed, because we over-allocate by BUFFER_GUARD
                    memcpy(end_internal() - 8, "BADF00D", 8);
                } else {
                    const char* ps = end_internal() - 8;
                    assert(memcmp(ps, "BADF00D", 8) == 0);
                }

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
        };
    } // namespace detail
} // namespace io
} // namespace my
