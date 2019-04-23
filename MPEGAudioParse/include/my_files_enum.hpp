#pragma once
// FilesEnum.h
#include <experimental/filesystem>
#include <string>
#include <type_traits>

namespace my {
namespace fs = std::experimental::filesystem;
class files_finder {

    std::string m_spath;
    bool m_recursive = false;
    unsigned long n = 0;
    std::string m_u8path;
    std::string m_u8extn;

    public:
    files_finder(std::string path, bool recursive = false) noexcept
        : m_spath(std::move(path)), m_recursive(recursive){};

    template <typename CB> int start(CB&& cb) {

        fs::recursive_directory_iterator dit(m_spath);
        int stop_now = 0;

        for (auto& p : dit) {
            if (fs::is_regular_file(p)) {

                auto& path = p.path();
                m_u8path = path.u8string();
                m_u8extn = path.extension().u8string();
                stop_now = cb(p, m_u8path, m_u8extn);
                ++n;
                if (stop_now) {
                    break;
                }
            }
        };
        return n;
    }

    const std::string& path() const noexcept { return m_spath; }
    bool is_recursive() const noexcept { return m_recursive; }
    unsigned long count() const noexcept { return n; }
};
} // namespace my
