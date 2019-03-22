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

            size_t m_capacity = SBO_SZ_I;
            static_assert(std::is_trivially_copyable_v<T> && sizeof(T) == 1
                    && std::is_trivially_constructible_v<T>,
                "sbo_buffer: bad type for T. Must be 1 byte wide and trivially "
                "assignable");

            byte_type* end_internal() { return begin() + m_capacity; }

            protected:
            size_t m_size = {0};

            public:
#ifndef SBO_BUFFER_CAN_COPY
            sbo_buffer(const sbo_buffer& rhs) = delete;
            sbo_buffer& operator=(const sbo_buffer&) = delete;
#else

#endif

            sbo_buffer() noexcept : m_dyn_buf(nullptr) { resize(0); }
            sbo_buffer(const byte_type* const pdata, size_t cb) noexcept {
                if (!pdata) {
                    assert(cb == 0);
                    return;
                }
                if (cb) {
                    assert(pdata);
                }

                append(pdata, cb);
            }
            ~sbo_buffer() noexcept { free(m_dyn_buf); }

            // just like vector, clear() does not free any memory
            void clear() { m_size = 0; }
            size_t size() const noexcept { return m_size; }
            size_t capacity() const noexcept { return m_capacity; }
            const byte_type* cdata() const noexcept {
                if (m_dyn_buf)
                    return m_dyn_buf;
                else
                    return m_sbo_buf;
            }

            byte_type* data() noexcept { return m_dyn_buf ? m_dyn_buf : m_sbo_buf; }
            byte_type* data_begin() noexcept { return data(); }
            byte_type* data_end() noexcept { return data() + m_size; }
            byte_type* begin() noexcept { return data(); }
            byte_type* end() noexcept { return data() + m_size; }
            const byte_type* cbegin() const noexcept { return cdata(); }
            const byte_type* cend() const noexcept { return cdata() + m_size; }
            byte_type operator[](size_t where) noexcept { return *(begin() + where); }
            byte_type operator[](size_t where) const noexcept {
                return *(cdata() + where);
            }

            void reserve(size_t sz) {
                // make the buffer if you need to,
                // but this isn't a real resize, so set the m_size = 0;
                resize(sz);
                m_size = 0;
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
                const auto cap = capacity();
                if (new_size > cap || m_dyn_buf) {
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
                        if (old_size) memcpy(m_dyn_buf, m_sbo_buf, old_size);
                        memset(m_sbo_buf, 0, SBO_SZ_I);
                    } else {
                        if (new_size > capacity()) {
                            auto* pnew = reinterpret_cast<byte_type*>(
                                realloc(m_dyn_buf, new_size + BUFFER_GUARD));
                            if (pnew == nullptr) {
                                fprintf(stderr,
                                    "realloc failed [ %lu ]\n -- not enough memory @ "
                                    "%s:%ul",
                                    static_cast<unsigned long>(new_size), __FILE__,
                                    __LINE__);
                            }
                            m_dyn_buf = pnew;
                        }
                    }
                }

                if (new_size > m_capacity || old_size == 0) {
                    if (new_size > m_capacity) {
                        m_capacity = new_size;
                    }

                    // this is allowed, because we over-allocate by BUFFER_GUARD
                    memcpy(end_internal() - 8, "BADF00D", 8);
                } else {
                    const char* ps = end_internal() - 8;
                    if (old_size) assert(memcmp(ps, "BADF00D", 8) == 0);
                }

                m_size = new_size;
            }
            void append(const byte_type* const data, size_t cb = 0) noexcept {

                if (data && cb == 0) {
                    fprintf(stderr,
                        "sbo_buffer::append(), inefficent use for value:\n%s\n -- can't "
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
