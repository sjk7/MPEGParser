// mpeg_audio_test.cpp

#include <iostream>
#include <cstdio>
#include <cassert>
#include <fstream>
#include "my_files_enum.h"
#include "my_mpeg.h"

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

using buf_t = my::mpeg::buffer_t;
using seek_t = buf_t::seek_t;
int read_file(buf_t& buf, seek_t seek, std::fstream& f) {
    int way = seek == seek_t::seek_type::seek_from_begin ? std::ios_base::beg
                                                         : std::ios_base::end;

    f.clear();

    f.seekg(seek.position, way);
    assert(f);
    if (!f) return -1;

    char* p = (char*)buf.begin() + buf.unread;
    std::streamsize can_read
        = std::streamsize(buf.capacity()) - std::streamsize(buf.unread);

    f.read(p, can_read);

    if (!f) {
        assert(f.eof());
    } else {
        assert(f);
    }
    const auto this_read = static_cast<int>(f.gcount());
    buf.written += this_read;
    return this_read;
}

int main(int argc, char** argv) {

    const std::string path("./Chasing_Pirates.mp3");
    const auto file_size = my::fs::file_size(path);
    my::mpeg::parser p(file_size);

    fstream f(path.c_str(), std::ios_base::binary | std::ios_base::in);
    if (!f) {
        assert("Cannot find file." == 0);
        return -7;
    }

    p.parse([&](buf_t& buf, seek_t seek = seek_t{0}) {
        if (buf.unread) {
            memmove(buf.data, buf.begin() + buf.unread, buf.unread);
        }

        assert(buf.capacity()
            > buf.unread); // why is buffer full? We can't read anything ffs
        return read_file(buf, seek, f);
    });
    return 0;
}
