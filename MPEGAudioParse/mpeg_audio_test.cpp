// mpeg_audio_test.cpp

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <iostream>
#include <cstdio>
#include <cassert>
#include <fstream>
#include <string.h>
#include <cerrno>
#include "./include/my_files_enum.hpp"
#include "./include/my_mpeg.hpp"

using namespace std;

void test_all_mp3() {
    const std::string my_extn = ".mp3";

    auto files_found_callback = [&](const auto& item) {
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
    finder.start([&](const auto& item, const auto& u8path, const auto& extn) {
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
int read_file(char* ptr, int& how_much, const seek_t& seek, std::fstream& f) {
    int way = CAST(int, seek.seek);

    if (!f && seek.seek == seek.seek_from_cur) {
        return -1; // already bad, probably eos last time.
    }

    if (seek.seek == seek.seek_from_cur && f.eof()) {
        how_much = 0;
        return my::io::NO_MORE_DATA;
    }
    f.clear();
    errno = 0;
    if (way != std::ios::cur) {
        f.seekg(seek.position, way);
    }

    int retval = 0;

    if (!f) {
        const auto e = errno;
        retval = e > 0 ? -e : -1;
    }
    assert(f);

    const std::streamsize can_read = how_much;

    f.read(ptr, can_read);

    bool eof = false;
    if (!f) {
        assert(f.eof());
        eof = true;
        // don't return: we could well have read data before we hit eof
    } else {
        assert(f);
    }
    const auto this_read = static_cast<int>(f.gcount());
    how_much = this_read;
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
    my::mpeg::buffer buf(path, [&](char* ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    });
#else
    auto lam = [&](char* ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    };
    using lam_t = decltype(lam);
    auto buf = my::mpeg::buffer<lam_t>::make_buffer(path, lam);
#endif
    int how_much = 1024;
    unsigned long total_read_size = 0;
    int result = 0;
    const my::mpeg::seek_t sk(0, my::io::seek_value_type::seek_from_begin);
    while (how_much > 0) {
        result = buf.get(how_much, sk, true);
        total_read_size += how_much;
        if (result < 0) break;
    }
    assert(result == my::io::NO_MORE_DATA);
    const int64_t diff = total_read_size - fsz;
    // assert(diff == 0 && "file size disagreement based on data read");
    return total_read_size;
}

int main(int argc, char** argv) {

#ifdef _WIN32
    _set_error_mode(_OUT_TO_MSGBOX);
#endif
    const std::string path("./ztest_files/Chasing_Pirates.mp3");

    /*/
    uint64_t grand_tot = 0;

    cout << endl;

    for (int i = 0; i < 1; ++i) {
        grand_tot += test_buffer(path);
        if (i % 10 == 0) {
            cout << ".";
        }
    }

    cout << endl;
    cout << endl;
         cout << "grand tot: " << grand_tot << endl;
    /*/
    const auto file_size = my::fs::file_size(path);
    fstream file(path.c_str(), std::ios_base::binary | std::ios_base::in);
    if (!file) {
        perror(strerror(errno));
        assert("File open error." == nullptr);
        return -1;
    }

    int64_t my_file_size = CAST(int64_t, file_size);

    using seek_t = my::io::seek_type;
    using seek_value_type = my::io::seek_value_type;

    // NOTE: MUST HAVE COMPILER SWITCH
    // /Zc:__cplusplus
    // To get correctly reported __cplusplus version
    cout << "Using c++ version: " << __cplusplus << endl;
    cout << "------------------------------------------\n";
    cout << __cplusplus << endl;
#if __cplusplus >= 201703L
    my::mpeg::buffer buf(path, [&](char* ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    });
#else
    auto lam = [&](char* ptr, int& how_much, const seek_t& seek) {
        return read_file(ptr, how_much, seek, file);
    };
    using lam_t = decltype(lam);
    auto buf = my::mpeg::buffer<lam_t>::make_buffer(path, lam);
#endif

    my::mpeg::parser p(path, file_size);
    auto e = p.parse(buf);

    // p.parse([&](buf_t& buf, seek_t seek = seek_t{0}) {

    return 0;
}
