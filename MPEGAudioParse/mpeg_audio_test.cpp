// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

// mpeg_audio_test.cpp

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <iostream>
#include <cstdio>
#include <cassert>
#include <fstream>
#include <cstring>
#include <cerrno>
#include "./include/my_files_enum.hpp"
#include "./include/my_mpeg.hpp"

using namespace std;
[[maybe_unused]] void test_all_mp3() {
    const std::string my_extn = ".mp3";

    [[maybe_unused]] const auto files_found_callback = [&](const auto& item) {
        if (my::fs::is_regular_file(item)) {
            const auto& path = item.path();
            const auto u8 = path.u8string();
            auto extn_actual = path.extension().u8string();
            if (extn_actual == my_extn) {
                cout << item << "\n ";
            }
        }
        return 0;
    };

    const std::string searchdir = "H:/audio-root-2018/";
    const bool recursive = true;

    my::files_finder finder(searchdir, recursive);
    int my_count = 0;

    finder.start(
        [&](const auto& /*item*/, const auto& /* u8path*/, const auto& extn) {
            if (extn == ".mp3") {
                ++my_count;
                // read_mp3(u8path);
            }
            return 0;
        });

    cout << "Total real files: " << finder.count() << endl;
    cout << "Total mp3 files:  " << my_count << endl;
}

// using buf_t = my::mpeg::buffer_t;
using seek_t = my::io::seek_type;
using seek_value_type = my::io::seek_value_type;

// return 0 normally, -1 or -errno if some file error
int read_file(
    char* const pdata, int& how_much, const seek_t& seek, std::fstream& f) {

    if (!pdata) {
        return -EINVAL;
    }
    const auto way = CAST(std::ios::seekdir, seek.seek);

    if (!f && seek.seek == seek_t::value_type::seek_from_cur
        && seek.position >= 0) {
        return -1; // already bad, probably eos last time.
    }

    if (seek.seek == seek_t::value_type::seek_from_cur && f.eof()) {
        how_much = 0;
        return my::io::NO_MORE_DATA;
    }
    f.clear();
    errno = 0;

    int64_t pos = seek.position;
    if (way != std::ios::cur) {
        if ((way == std::ios::end)) {
            if (pos > 0) {
                pos = -pos;
            }
        }
    }

    if (seek.seek != seek_value_type::seek_invalid) {
        f.seekg(pos, way);
    }

    int retval = 0;

    if (!f) {
        const auto e = errno;
        retval = e > 0 ? -e : -1;
        return retval;
    }
    assert(f);
    const std::streamsize can_read = how_much;

    f.read(pdata, can_read);
    int errn = 0;
    const auto byte = *pdata;
    CAST(void, byte);

    bool eof = false;
    if (!f) {
        assert(f.eof());
        eof = true;
        const auto err = errno;
        if (err != 0) {
            errn = errno;
        }
        // don't return: we could well have read data before we hit eof
    }
    const auto this_read = f.gcount();
    how_much = CAST(int, this_read);
    if (errn != 0) {
        return -errn;
    }
    return eof ? my::io::NO_MORE_DATA : 0;
}

int64_t test_buffer(const std::string& path) {
    // using cb_type = decltype(buffer_read_callback);
    fstream file(path.c_str(), std::ios_base::binary | std::ios_base::in);
    if (!file) {
        perror(strerror(errno));
        assert("File open error." == nullptr);
        return -1;
    }
    auto fsz = my::fs::file_size(path);

#if __cplusplus >= 201703L
    my::mpeg::buffer buf(
        path, [&](char* ptr, int& how_much, const seek_t& seek) {
            return read_file(ptr, how_much, seek, file);
        });
#else
    auto lam = [&](char* ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    };
    using lam_t = decltype(lam);
    auto buf = my::mpeg::buffer<lam_t>::make_buffer(path, lam);
#endif

    unsigned long total_read_size = 0;
    my::mpeg::error result;
    const my::mpeg::seek_t sk = my::mpeg::seek_t();
    char mybuf[4096] = {};
    int how_much = 4096;

    while (how_much > 0) {
        result = buf.get(how_much, sk, &mybuf[0], true);
        total_read_size += CAST(size_t, how_much);
        if (result) {
            break;
        }
    }
    assert(result == my::mpeg::error::error_code::no_more_data);
    const auto diff = CAST(int64_t, total_read_size - fsz);
    assert(diff == 0 && "file size disagreement based on data read");
    return CAST(int64_t, total_read_size);
}

void test_file_read(const std::string& path) {

    int64_t grand_tot = 0;
    int64_t expected = 0;
    cout << endl;

    for (int i = 0; i < 1; ++i) {
        grand_tot += test_buffer(path);
    }

    if (path == "./ztest_files/Chasing_Pirates.mp3") {
        expected = 6441880;
    } else {
        assert("Give me some size to expect" == nullptr);
    }
    assert(expected == grand_tot);
    cout << endl;
    cout << "test_file_read: grand tot: " << grand_tot << endl;
}

#ifdef _MSC_VER
#pragma warning(disable : 26485) // no decaying arrays
#endif

int main(int /*unused*/, const char* const argv[]) {

    assert(argv);
#ifdef _WIN32
    _set_error_mode(_OUT_TO_MSGBOX);
#endif
    puts("Current directory:");
    puts(argv[0]);
    const std::string path("./ztest_files/Chasing_Pirates.mp3");
    assert(my::fs::exists(path) && "test file does not exist");
    // const std::string path("./ztest_files/128.mp3");

    const auto file_size = my::fs::file_size(path);
    fstream file(path.c_str(), std::ios_base::binary | std::ios_base::in);
    if (!file) {
        perror(strerror(errno));
        assert("File open error." == nullptr);
        return -1;
    }

    // auto my_file_size = CAST(int64_t, file_size);

    using seek_t = my::io::seek_type;
    // using seek_value_type = my::io::seek_value_type;

    // NOTE: MUST HAVE COMPILER SWITCH
    // /Zc:__cplusplus
    static_assert(__cplusplus >= 201101L, "expected you to use c++11 or later");
    // To get correctly reported __cplusplus version
    cout << "Using c++ version: " << __cplusplus << endl;
    cout << "------------------------------------------\n";
    cout << __cplusplus << endl;
#if __cplusplus >= 201703L
    my::mpeg::buffer buf(
        path, [&](char* const ptr, int& how_much, const seek_t& seek) {
            return read_file(ptr, how_much, seek, file);
        });
#else
    auto lam = [&](char* const ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    };
    using lam_t = decltype(lam);
    auto buf = my::mpeg::buffer<lam_t>::make_buffer(path, lam);
#endif

    my::mpeg::parser p(path, file_size);
    const auto e = p.parse(buf);
    (void)e;
    test_file_read(path);
    return 0;
}
