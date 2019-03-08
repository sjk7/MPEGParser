#pragma once
// my_mpeg.h
#include "my_io.hxx"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <functional>

namespace my {
namespace mpeg {

    namespace detail {

        static constexpr int ID3V2_HEADER_SIZE = 10;
        static constexpr uint32_t ID3V2_MAX_SIZE = 1024U * 1024U; // 1 meg

        enum ChannelEnum {
            CHANNELS_STEREO = 0,
            CHANNELS_JOINT_STEREO = 1,
            CHANNELS_DUAL_CHANNEL = 2,
            CHANNELS_SINGLE_CHANNEL = 3
        };

        static inline const char* Channel_String(ChannelEnum ce) {
            switch (ce) {
                case CHANNELS_STEREO: return "Stereo";
                case CHANNELS_JOINT_STEREO: return "Double Mono";
                case CHANNELS_DUAL_CHANNEL: return "Joint Stereo";
                case CHANNELS_SINGLE_CHANNEL: return "Single Channel mono";
                default: return "unknown channel mode";
            };
        }

        enum EmphasisEnum {
            EMPHASIS_NONE = 0,
            EMPHASIS_FIFTY_FIFTEEN_MS = 1,
            EMPHASIS_RESERVED = 2,
            EMPHASIS_CCIT_J17 = 3
        };

        static inline const char* Emphasis_String(EmphasisEnum ce) {
            switch (ce) {
                case EMPHASIS_NONE: return "No Emphasis";
                case EMPHASIS_FIFTY_FIFTEEN_MS: return "15 Millisecond Emphasis";
                case EMPHASIS_RESERVED: return "Reserved (BAD) Emphasis";
                case EMPHASIS_CCIT_J17: return "CCIT_J17 Emphasis";
                default: return "unknown emphasis";
            };
        }
    } // namespace detail

    using seek_type = my::io::seek_type;

    template <typename READER>
    class buffer : public my::io::buffer_guts_type<buffer<READER>> {
        using base = my::io::buffer_guts_type<buffer<READER>>;
        READER m_cb;
        template <typename T> buffer(T reader, int) : base(*this), m_cb(reader) {}

        public:
        static auto make_buffer(READER&& reader) { return buffer<READER>(reader); }
        buffer(READER reader) : buffer(reader, 0) {}

        int get(int& how_much, bool peek, seek_type&& seeker) {

            int avail_space = 0;
            char* ptr = this->writeptr(avail_space);
            if (!ptr) {
                std::cout << "NO PTR, FFS" << std::endl;
            }
            how_much = (std::min)(how_much, avail_space);
            assert(how_much > 0);

            int ret = m_cb(ptr, how_much, std::forward<seek_type&>(seeker));
            if (!peek) {
                base::unread += how_much;
            }
            return ret;
        }
    };

    using byte_type = my::io::byte_type;

    template <typename CB> using buffer_t = my::io::buffer_guts_type<CB>;
    template <typename CB> using buffer_type = my::io::buffer_guts_type<CB>;

    struct error {
        static constexpr int none = 0;
        static constexpr int unknown = -1;
        static constexpr int no_more_data = -2;
        static constexpr int buffer_too_small = -4;
        static constexpr int no_id3v2_tag = -5;
        static constexpr int bad_mpeg_version = -6;
        static constexpr int bad_mpeg_layer = -7;
        static constexpr int bad_mpeg_bitrate = -8;
        static constexpr int bad_mpeg_samplerate = -9;
        static constexpr int lost_sync = 10;
        static constexpr int need_more_data = 11;
        static constexpr int bad_mpeg_channels = -12;
        static constexpr int bad_mpeg_emphasis = -13;
        // static constexpr int buffer_too_small = -6;

        int value = none;
        operator int() const { return value; }
        error() : value(none){};
        error(const int rhs) : value(rhs) {}
        bool operator==(const int rhs) { return value == rhs; }
        bool operator==(const error rhs) { return value == rhs.value; }

        const std::string to_string() {
            static const char* const values[] = {"none", "unknown", "no more data",
                "need more data", "buffer too small", "no id3v2 tag", "bad mpeg version",
                "bad mpeg layer", "bad mpeg bitrate", "bad samplerate", "lost sync",
                "need_more_data", "bad mpeg channels", "bad mpeg emphasis"};

            static constexpr int sz = sizeof(values);
            if (value < 0) {
                value = -value;
            }
            if (value < 0 || value >= sz) {
                return std::string();
            }
            return values[value];
        }
    };

    struct frame_header {
        static constexpr int8_t MPEG_HEADER_SIZE = 4;
        // The actual header itself:
        my::io::byte_type header_bytes[MPEG_HEADER_SIZE] = {0};
        bool valid = {false};
        int64_t file_position{-1};
        // The amount of data after the header and before the end of the frame
        int payload_size{0};
    };
    static constexpr int versions[] = {1, 2, 3};
    static constexpr int bitrates[] = {88, 77};
    static constexpr int samplerates[] = {44100, 48000, 196000};
    struct mpeg_properties {
        uint8_t version{0};
        uint8_t layer{0};
        int samplerate{0};
        int bitrate{0};
        uint8_t padding{0};
        uint8_t crc{0};
        uint8_t copyright{0};
        uint8_t emphasis{0};
        uint8_t channelmode{0};
    };
    struct frame_base {
        frame_header hdr;
        mpeg_properties props;
        bool valid = {false};
        std::size_t size() const {
            return hdr.payload_size + sizeof(hdr.header_bytes) + props.padding;
        }
        void clear() {
            hdr = frame_header();
            props = mpeg_properties{0};
            valid = false;
        }
    };
    struct frame : frame_base {

        bool operator!() const { return !frame_base::hdr.valid; }
        operator bool() const { return frame_base::hdr.valid; }
    };

    namespace detail {
        static constexpr uint8_t MPEG_HEADER_SIZE = 4;
#pragma pack(push, 1)
        struct ID3V1 {
            char id[3] = {0};
            char title[30] = {0};
            char artist[30] = {0};
            char album[30] = {0};
            char year[4] = {0};
            char comment[30] = {0};
            char genre = {0};
        };

        struct id3v2Header {
            char id[3] = {0};
            byte_type version[2] = {0};
            byte_type flags[1] = {0};
            byte_type size[4] = {0};
            // ---> convenient stuff on end: not part of real struct.
            uint32_t tagsize_inc_header{0};
            int appended = 0;
            uint32_t total_size() const { return tagsize_inc_header; }
        };
#pragma pack(pop)

        inline bool id3_is_valid(const id3v2Header& h) {
            return h.tagsize_inc_header > 10 && (h.tagsize_inc_header < ID3V2_MAX_SIZE);
        }

        uint32_t DecodeSyncSafe(const char* pb) {
            /// For i As Integer = 0 To UBound(arBytes)
            // usum = usum Or (arBytes(i) And 127) << ((3 - i) * 7)
            // Next
            uint32_t retval = 0;
            if (pb == nullptr) {
                return 0;
            }

            for (int i = 0; i < 4; i++) {
                retval = retval | (pb[i] & 127) << ((3 - i) * 7);
            }
            return retval;
        }

        template <typename T>
        int find_next_frame(const frame& prev, frame& next, const buffer_t<T>& buf) {
            next.clear();
            return 0;
        }

        using frames_t = frame[2];
        inline void init_frames(frames_t& frames) {
            frames[0].clear(), frames[1].clear();
        }

        inline error frame_version(frame& frame) {
            error e;
            const auto& data = frame.hdr.header_bytes;
            frame.props.crc
                = static_cast<uint8_t>(static_cast<unsigned char>(data[1] & 0x01) == 0);
            static constexpr int MAX_MPEG_VERSIONS = 3;
            unsigned char version_index
                = (static_cast<unsigned char>(data[1]) >> 3) & 0x03;
            static constexpr unsigned char MPEGVersions[MAX_MPEG_VERSIONS] = {3, 2, 1};

            const int ver_out_of_box = version_index;
            if (ver_out_of_box == 1) {
                e = error::bad_mpeg_version;
                return e;
            }
            if (version_index == 2) {
                version_index = 1;
            }
            if (version_index == 3) {
                version_index = 2;
            }
            if (version_index >= MAX_MPEG_VERSIONS) {
                e = error::bad_mpeg_version;
                return e;
            }
            frame.props.version = MPEGVersions[version_index];
            return e;
        }
        inline error frame_layer(frame& frame) {
            error e;
            static constexpr int MAX_MPEG_LAYERS = 4;
            static constexpr int MPEGLayers[MAX_MPEG_LAYERS] = {-1, 3, 2, 1};
            const unsigned char layer_index
                = (static_cast<unsigned char>(frame.hdr.header_bytes[1]) >> 1) & 0x03;
            if (layer_index >= MAX_MPEG_LAYERS || layer_index == 0) {
                e = error::bad_mpeg_layer;
                return e;
            }
            frame.props.layer = MPEGLayers[layer_index];
            assert(frame.props.layer);
            return e;
        }
        error frame_bitrate(frame& frame) {

            error e;
            static constexpr int MAX_MPEG_BITRATES = 16;
            static int V1L1BitRates[MAX_MPEG_BITRATES] = {
                0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1};
            static constexpr int V1L2BitRates[MAX_MPEG_BITRATES]
                = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, -1};
            static constexpr int V1L3BitRates[MAX_MPEG_BITRATES]
                = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1};
            static constexpr int V2L1BitRates[MAX_MPEG_BITRATES]
                = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, -1};
            static constexpr int V2L2L3BitRates[MAX_MPEG_BITRATES]
                = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, -1};

            const unsigned int bitrate_index
                = (static_cast<unsigned int>(frame.hdr.header_bytes[2]) >> 4) & 0x0F;

            static constexpr unsigned char BAD_BITRATE_INDEX = 0;
            if (bitrate_index
                    >= MAX_MPEG_BITRATES - 1 // -1 coz all values @ 16 -1 are OK for
                                             // us, 16 itself is -1, which is not.
                || bitrate_index == BAD_BITRATE_INDEX) {
                e = error::bad_mpeg_bitrate;
            }

            assert(frame.props.version);
            assert(frame.props.layer);
            if (frame.props.version == 1) {
                switch (frame.props.layer) {
                    case 3: {
                        frame.props.bitrate = V1L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 2: {
                        frame.props.bitrate = V1L2BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 1: {
                        frame.props.bitrate = V1L1BitRates[bitrate_index] * 1000;
                        break;
                    }
                    default: return error::bad_mpeg_bitrate;
                };

            } else {
                switch (frame.props.layer) {
                    case 3: {
                        frame.props.bitrate = V2L2L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 2: {
                        frame.props.bitrate = V2L2L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 1: {
                        frame.props.bitrate = V2L1BitRates[bitrate_index] * 1000;
                        break;
                    }
                    default:
                        // same fo v2 abd v2.5 here:
                        frame.props.bitrate = V2L2L3BitRates[bitrate_index] * 1000;
                };
            }
            assert(frame.props.bitrate > 0 && "bitrate <= 0");
            return e;
        }

        error frame_samplerate(frame& frame) {

            error e;
            static constexpr int MAX_MPEG_SAMPLERATES = 4;
            static constexpr int MPEG1SampleRates[MAX_MPEG_SAMPLERATES]
                = {44100, 48000, 32000, -1};
            static constexpr int MPEG2SampleRates[MAX_MPEG_SAMPLERATES]
                = {22050, 24000, 16000, -1};
            static constexpr int MPEG2Point5SampleRates[MAX_MPEG_SAMPLERATES]
                = {11025, 12000, 8000, -1};

            const unsigned int samplerate_index
                = (static_cast<unsigned char>(frame.hdr.header_bytes[2]) >> 2) & 0x03;

            if (samplerate_index >= MAX_MPEG_SAMPLERATES || samplerate_index == 3) {
                e = error::bad_mpeg_samplerate;
                return e;
            }

            if (static_cast<int>(frame.props.version) == 1) {
                frame.props.samplerate = MPEG1SampleRates[samplerate_index];
            } else if (static_cast<int>(frame.props.version) == 2) {
                frame.props.samplerate = MPEG2SampleRates[samplerate_index];
            } else if (frame.props.version > 2) {
                frame.props.samplerate = MPEG2Point5SampleRates[samplerate_index];
            }

            assert(frame.props.samplerate && "no samplerate");
            return e;
        }

        error frame_emph_copyright_etc(frame& frame) {
            error e;
            static constexpr int MAX_MPEG_CHANNEL_MODE = 4;
            const unsigned int cmodeindex
                = ((static_cast<unsigned char>(frame.hdr.header_bytes[3]) >> 6) & 0x03);

            static constexpr int ChannelMode[MAX_MPEG_CHANNEL_MODE] = {CHANNELS_STEREO,
                CHANNELS_JOINT_STEREO, CHANNELS_DUAL_CHANNEL, CHANNELS_SINGLE_CHANNEL};

            if (cmodeindex >= MAX_MPEG_CHANNEL_MODE) {
                e = error::bad_mpeg_channels;
                return e;
            }

            frame.props.channelmode = ChannelMode[cmodeindex];

            auto data = frame.hdr.header_bytes;
            // phead->origin = ((static_cast<unsigned char>(data[3]) & 0x04) != 0);
            frame.props.copyright
                = static_cast<uint8_t>((static_cast<unsigned char>(data[3]) & 0x08) != 0);
            frame.props.padding
                = static_cast<uint8_t>((static_cast<unsigned char>(data[2]) & 0x02) != 0);

            static constexpr int MAX_EMPH = 4;
            unsigned int emphasis = ((static_cast<unsigned int>(data[3]) & 0x03));
            if (emphasis >= MAX_EMPH) {
                e = error::bad_mpeg_emphasis;
                return e;
            }

            static constexpr EmphasisEnum emph_lookup[MAX_EMPH]
                = {EmphasisEnum::EMPHASIS_NONE, EmphasisEnum::EMPHASIS_FIFTY_FIFTEEN_MS,
                    EmphasisEnum::EMPHASIS_RESERVED, EmphasisEnum::EMPHASIS_CCIT_J17};
            frame.props.emphasis = emph_lookup[emphasis];
            frame.valid = true;
            return e;
        }
        inline error parse_frame_header(frame& frame) {
            error e = frame_version(frame);
            if (e != 0) {
                return e;
            }
            e = frame_layer(frame);
            if (e != 0) {
                return e;
            }
            e = frame_bitrate(frame);
            if (e != 0) {
                return e;
            }
            e = frame_samplerate(frame);
            if (e != 0) {
                return e;
            }
            e = frame_emph_copyright_etc(frame);
            return e;
        }

        template <typename CB> using _buffer_type = buffer_type<CB>;

    } // namespace detail

    template <typename CB> using buffer_type = detail::_buffer_type<CB>;
    using seek_t = my::io::seek_type;

    template <typename CB> class parser {

        private:
        using buffer_t = buffer_type<CB>;
        uint32_t nframes{0};
        int64_t file_size{-1};
        std::string filepath;
        my::mpeg::error err;
        frame m_first;
        detail::id3v2Header m_id3v2Header;
        detail::ID3V1 m_v1;
        int m_id3v1_size = 0;
        CB& m_cb;

        parser(CB& cb, int, std::string_view file_path, uintmax_t file_size)
            : m_cb(cb), file_size(CAST(int64_t, file_size)), filepath(file_path) {}

        public:
        NO_COPY_AND_ASSIGN(parser);
        NO_MOVE_AND_ASSIGN(parser);

        parser(CB& read_callback, std::string_view file_path, uintmax_t file_size)
            : parser(read_callback, 0, file_path, file_size) {

            std::cout << "parser constructor. Filename = " << filepath << std::endl;
        }

        mpeg::error parse() {
            puts("Parse called.\n");
            return 0;
        }
    };

} // namespace mpeg
} // namespace my
