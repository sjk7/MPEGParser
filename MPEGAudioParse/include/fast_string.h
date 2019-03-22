// fast_string.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <iosfwd>
#include <ostream>
#include <cstring>
#include <functional>
#include <string>
#include <cassert>
#include "my_operators.h"
#define SBO_BUFFER_CAN_COPY 1
#include "../MPEGParser/MPEGAudioParse/include/my_io.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

namespace my {
inline int random(int min, int max) {
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static char mybuf[1024];

static inline int random_string(int nchars_min, int nchars_max, char* buf) {
    static int first = 0;
    if (!first) {
        first++;
        std::string s(1023, 'x');
        memcpy(mybuf, s.c_str(), 1024);
    }

    auto const len = nchars_max;
    memcpy(buf, mybuf, len);

    buf[len] = '\0';
    return len;
}
} // namespace my
namespace my {

template <typename T, size_t SZ> using sbo_buf_type = my::io::detail::sbo_buffer<T, SZ>;

template <typename T = char, size_t MAX_CAPACITY = 256>
class fast_string : public sbo_buf_type<T, MAX_CAPACITY>,
                    public my::relationable_impl<fast_string<T, MAX_CAPACITY>> {

    using impl_type = my::relationable_impl<sbo_buf_type<T, MAX_CAPACITY>>;
    using base = sbo_buf_type<T, MAX_CAPACITY>;
    using base::m_size;
    static_assert(
        sizeof(T) == 1, "fast_string: templated type T must be of size 1 byte.");

    constexpr fast_string(const T* ptr, size_t sz, int dummy) : base(ptr, sz) {}

    public:
    using iterator_type = T*;
    using size_type = int;
    using value_type = T;
    using ME = fast_string;
    fast_string(const T* t, size_t len) : fast_string(t, len, 0) {}
    fast_string() : fast_string(nullptr, 0, 0) {}
    fast_string(const char* str) : fast_string(str, str ? strlen(str) : 0, 0) {
        // puts("Undesribale constructor (has to use strlen)");
    }
    template <typename T>
    explicit fast_string(const T& s, typename T::size_type sz = 0)
        : fast_string(s.c_str(), s.size(), 1) {}

    template <typename T, size_t N>
    constexpr fast_string(T (&x)[N]) : fast_string(x, N - 1, 1) {}

    fast_string& operator=(const char* p) {
        base::clear();
        base::append(p, strlen(p));
        return *this;
    }
    template <typename T, size_t N> fast_string& operator=(T (&x)[N]) {
        base::clear();
        base::append(x, N);
        return *this;
    }

    static constexpr size_t capacity() { return (MAX_CAPACITY * sizeof(T)) + sizeof(T); }

    // fast_string(fast_string&& rhs) { puts("move construct"); }
    void clear() {
        base::clear();
        terminate_string(0);
    }
    void reserve(size_t sz) const { base::reserve(sz); }
    // substitute for std::string

    private:
    void terminate_string(size_t sz = -1) {
        int64_t i = CAST(int64_t, sz);
        const auto where = i >= 0 ? sz : base::m_size;
        base::operator[](sz) = '\0';
    }

    public:
    // size_t size() const { return m_size; }
    size_t size_in_bytes() const { return base::size() * sizeof(T); }

    const T* c_str() const noexcept { return base::cbegin(); }
    friend std::ostream& operator<<(std::ostream& out, fast_string& me) {
        // output something
        out << me.c_str();
        return out;
    }

    bool equals(const std::string& other) const {
        if (other.size() == m_size) {
            return compare(other) == 0;
        }
        return false;
    }

    template <typename X> bool equals(const X& other) const {
        if (m_size == other.size()) {
            return compare(other) == 0;
        }
        return false;
    }

    template <typename X> bool equals(const X* other) const {
        if (other) {
            return compare(other) == 0;
        }
        return false;
    }

    int compare(const char* rhs) const {

        assert(rhs);
        const auto rs = strlen(rhs);
        const auto ls = m_size;
        const auto sz = (std::min)(m_size, rs);
        const int ans = memcmp(base::cbegin(), rhs, sz);
        if (ans != 0) {
            return (ans);
        }

        if (ls < rs) {
            return (-1);
        }

        if (ls > rs) {
            return (1);
        }

        return 0;
    }

    template <typename X>
    int compare(const X& rhs,
        typename std::enable_if_t<!std::is_pointer<X>::value>* dummy = 0) const {
        const auto ls = base::size();
        const auto rs = rhs.size();
        const auto sz = (std::min)(ls, rs);

        const int ans = memcmp(base::cbegin(), rhs.c_str(), sz);
        if (ans != 0) {
            return (ans);
        }

        if (ls < rs) {
            return (-1);
        }

        if (ls > rs) {
            return (1);
        }

        return 0;
    }

    template <typename T, size_t N> bool operator==(T (&other)[N]) {
        return equals(other);
    }

    bool operator==(const char* other) const { return equals(other); }
    bool operator==(const fast_string& fs) const { return equals(fs); }
    bool operator==(const std::string& other) const { return equals(other); }
    template <typename X> bool operator==(const X& other) const { return equals(other); }

    bool operator<(const char* other) { return compare(other) < 0; }
    bool operator<(const fast_string& fs) const { return (compare(fs) < 0); }
    bool operator<(const std::string& other) const { return compare(other) < 0; }
    template <typename X> bool operator<(const X& other) const {
        return compare(other) < 0;
    }

    template <typename T, size_t N> bool operator<(T (&other)[N]) const {
        return equals(other);
    }

    T& operator[](unsigned int i) { return base::cbegin()[i]; }
    const T& operator[](unsigned int i) const { return base::cbegin()[i]; }
    const size_t size() const noexcept { return base::size(); }
    template <typename U> void append(const U& u) noexcept {
        const auto new_size = size() + u.size();
        terminate_string(new_size);
        memcpy(&base::cbegin()[m_size], &u[0], u.size());
    }

    void append(const char* ptr, size_t len = 0) { base::append(ptr, len); }
    template <typename X, size_t N> void append(X (&x)[N]) { base::append(x, N - 1); }
    template <typename U> T& operator+=(const U& u) {
        const size_t new_size = u.size() + size();
        if (new_size > capacity()) {
            assert("string will be truncated : too big" == 0);
            return *this;
        }
        append(u);
        return *this;
    }
    fast_string& operator+=(const T& v) {
        const auto newsize = m_size + 1;
        terminate_string(newsize);
        base::cbegin()[m_size] = v;
        m_size++;
        return *this;
    }

}; // namespace my

/*/
bool operator!=(const char* a, fast_string<>& b) noexcept {
    return !b.equals(a);
}

bool operator!=(fast_string<>& b, const char* a) noexcept {
    return !b.equals(a);
}

template <typename A> bool operator!=(const A& a, const fast_string<>& b) noexcept
{ return !b.equals(a);
}
template <typename B> bool operator!=(const fast_string<>& a, const B& b) noexcept
{ return b != a;
}
/*/
} // namespace my
