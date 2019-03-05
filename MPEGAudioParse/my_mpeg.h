#pragma once
// my_mpeg.h
#include <cassert>

#include <cstdint>
#include <limits>

#ifndef NO_COPY_AND_ASSIGN
#define NO_COPY_AND_ASSIGN(TypeName)                                                     \
    TypeName(const TypeName&) = delete;                                                  \
    void operator=(const TypeName&) = delete;
#endif
#ifndef NO_MOVE_AND_ASSIGN
#define NO_MOVE_AND_ASSIGN(TypeName)                                                     \
    TypeName(TypeName&&) = delete;                                                       \
    void operator=(TypeName&&) = delete;
#endif

#include <functional>
namespace my {
namespace mpeg {

    using byte = unsigned char;
    namespace detail {
        static constexpr int BUFFER_CAPACITY = 1024;
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

    template <size_t CAPACITY = detail::BUFFER_CAPACITY> struct buffer {

        byte data[CAPACITY] = {0};
        int written = 0;
        int read = 0;
        int unread = 0;
        void clear() {
            written = 0;
            read = 0;
            unread = 0;
        }
        constexpr int capacity() const { return static_cast<int>(CAPACITY); }
        const byte* const end() const { return (const byte*)(data + written); }
        const byte* begin() const { return (const byte*)(data); }
        const byte* const data_end() const { return begin() + written; }
        bool empty() const { return written == 0 || read == written; }

        struct seek_t {
            enum seek_type { seek_from_begin, seek_from_cur, seek_from_end };
            seek_t(int64_t pos, seek_type sk = seek_from_begin)
                : position(pos), seek(sk) {}
            seek_t(const seek_t& rhs) = default;
            seek_t(seek_t&& rhs) = default;
            seek_t& operator=(const seek_t& rhs) = default;
            seek_t& operator=(seek_t&& rhs) = default;
            int64_t position = 0;
            seek_type seek = seek_from_cur;
            operator int() const { return seek; }
            operator seek_type() const { return seek; }
        };
    };
    using buffer_t = buffer<>;

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
            if (value < 0) value = -value;
            if (value < 0 || value >= sz) {
                return std::string();
            }

            return values[value];
        }
    };

    struct frame_header {
        static constexpr int8_t MPEG_HEADER_SIZE = 4;
        // The actual header itself:
        byte header_bytes[MPEG_HEADER_SIZE] = {0};
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
        bool valid{0};
        size_t size() const {
            return hdr.payload_size + sizeof(hdr.header_bytes) + props.padding;
        }
        void clear() {
            hdr = frame_header{0};
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
        typedef struct {
            char id[3];
            char title[30];
            char artist[30];
            char album[30];
            char year[4];
            char comment[30];
            char genre;
        } ID3V1;

        struct id3v2Header {
            char id[3] = {0};
            byte version[2] = {0};
            byte flags[1] = {0};
            byte size[4] = {0};
            // ---> convenient stuff on end: not part of real struct.
            uint32_t tagsize_inc_header{0};
            int appended = false;
            uint32_t total_size() const { return tagsize_inc_header; }
        };
#pragma pack(pop)

        inline bool id3_is_valid(const id3v2Header& h) {
            return h.tagsize_inc_header > 10 && (h.tagsize_inc_header < ID3V2_MAX_SIZE);
        }

        uint32_t DecodeSyncSafe(char* pb) {
            /// For i As Integer = 0 To UBound(arBytes)
            // usum = usum Or (arBytes(i) And 127) << ((3 - i) * 7)
            // Next
            uint32_t retval = 0;
            if (!pb) return 0;

            for (int i = 0; i < 4; i++) {
                retval = retval | (pb[i] & 127) << ((3 - i) * 7);
            }
            return retval;
        }

        template <typename CB> int fill_buffer(buffer_t& buf, CB&& cb) {
            assert(0);
            return 0;
        }

        inline error get_id3v2_tag(buffer_t& buf, id3v2Header& id3) {

            if (buf.empty()) return error::need_more_data;
            if (buf.written < 10) return error::buffer_too_small;

            memcpy(&id3, buf.begin(), ID3V2_HEADER_SIZE);
            int found = -1;
            const byte* p = buf.begin();
            if (found = memcmp(p, "ID3", 3) != 0) {
                return error::no_id3v2_tag;
            }

            if (found == 0) {
                /*/
                    ID3v2/file identifier	"ID3"				0, 1, 2		3
                    ID3v2 version			$04 00 / $03 00		3, 4		2
                    ID3v2 flags				%abcd0000			5			1
                    ID3v2 size			4 *	%0xxxxxxx			6,7,8,9		4
                    a:	Unsynchronizaation.
                    b:	Has extended header (is 10 bytes longer than expected).
                    c:	Experimental.
                    d:	Footer present.
                /*/

                uint32_t usize = detail::DecodeSyncSafe((char*)(&id3)->size);
                if (usize) usize += ID3V2_HEADER_SIZE;
                if (usize > detail::ID3V2_MAX_SIZE) {
                    assert("MAX ID3 SIZE EXCEEDED!" == 0);
                    id3.tagsize_inc_header = 0;
                } else {
                    id3.tagsize_inc_header = usize;
                }

                return error::none;
            }
            return 0;
        }

        int find_next_frame(const frame& prev, frame& next, const buffer_t& buf) {
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
            frame.props.crc = (static_cast<unsigned char>(data[1] & 0x01) == 0);
            static constexpr int MAX_MPEG_VERSIONS = 3;
            unsigned char version_index
                = (static_cast<unsigned char>(data[1]) >> 3) & 0x03;
            static constexpr unsigned char MPEGVersions[MAX_MPEG_VERSIONS]
                = {3, /*-1,*/ 2, 1};

            const int ver_out_of_box = version_index;
            if (ver_out_of_box == 1) {
                e = error::bad_mpeg_version;
                return e;
            }
            if (version_index == 2) version_index = 1;
            if (version_index == 3) version_index = 2;
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
                    >= MAX_MPEG_BITRATES - 1 // -1 coz all values @ 16 -1 are OK for us,
                                             // 16 itself is -1, which is not.
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

            if ((int)frame.props.version == 1)
                frame.props.samplerate = MPEG1SampleRates[samplerate_index];
            else if ((int)frame.props.version == 2)
                frame.props.samplerate = MPEG2SampleRates[samplerate_index];
            else if (frame.props.version > 2)
                frame.props.samplerate = MPEG2Point5SampleRates[samplerate_index];

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
            frame.props.copyright = ((static_cast<unsigned char>(data[3]) & 0x08) != 0);
            frame.props.padding = ((static_cast<unsigned char>(data[2]) & 0x02) != 0);

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
            if (e) return e;
            e = frame_layer(frame);
            if (e) return e;
            e = frame_bitrate(frame);
            if (e) return e;
            e = frame_samplerate(frame);
            if (e) return e;
            e = frame_emph_copyright_etc(frame);
            return e;
        }
    } // namespace detail

    class parser {

        uint32_t nframes;
        int64_t file_size;
        std::string filepath;
        my::mpeg::error err;
        frame m_first;
        detail::id3v2Header m_id3v2Header = {0};
        detail::ID3V1 m_v1 = {0};
        buffer_t m_buf;

        template <typename CB> error get_id3(CB&& cb) {
            int n = 0;
            cb(m_buf);
            error e = get_id3v2_tag(m_buf, m_id3v2Header);
            if (e) return e;

            buffer_t::seek_t sk(-128, buffer_t::seek_t::seek_from_end);
            n = cb(m_buf, sk);
            int id3v1_size = 0;
            if (memcmp(&m_buf, "TAG", 3) == 0) {
                memcpy(&m_v1, &m_buf, 128);
            }
            return e;
        }

        byte* find_sync(const buffer_t& buf, const int start_position) {

            const auto e = (unsigned char*)buf.end();
            unsigned char* p = (unsigned char*)buf.begin();
            p += start_position;
            while (e - p >= 4) {
                if (*p == 255 && *(p + 1) >= 224) {
                    return (byte*)p;
                }
                ++p;
            }
            return nullptr;
        }

        error single_frame(buffer_t buf, int start_where, frame& f) {
            error e;
            f.clear();
            if (buf.unread < frame_header::MPEG_HEADER_SIZE) return e.need_more_data;
            byte* psync = find_sync(buf, 0);
            if (!psync) {
                e = error::lost_sync;
                return e;
            }
            memcpy(&f.hdr.header_bytes, psync, 4);
            e = detail::parse_frame_header(f);
            if (e) return e;
            return e;
        }
        template <typename CB>
        error find_first_frames(CB&& cb, detail::frames_t& frames) {
            detail::init_frames(frames);
            error e = get_id3(cb);
            if (e) return e;

            buffer_t::seek_t sk(m_id3v2Header.total_size());
            m_buf.clear();
            int read = cb(m_buf, sk);
            m_buf.unread += read;
            int start_where = 0;
            e = single_frame(m_buf, start_where, frames[0]);
            return e;
        }

        public:
        NO_COPY_AND_ASSIGN(parser);
        NO_MOVE_AND_ASSIGN(parser);
        parser(int64_t file_size = 0) : nframes(0), file_size(file_size) {}

        template <typename CB> mpeg::error parse(CB&& cb) {
            detail::frames_t frames;
            mpeg::error err = find_first_frames(cb, frames);
            if (err < 0) return err;

            return err;
        }
    };
    namespace detail {}
} // namespace mpeg
} // namespace my
