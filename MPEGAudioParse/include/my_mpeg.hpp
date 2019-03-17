#pragma once
// my_mpeg.h
#include "my_io.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <functional>
#include "my_string_view.hpp"

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

    struct error {
        static constexpr int none = 0;
        static constexpr int unknown = 1;
        static constexpr int no_more_data = 2;
        static constexpr int buffer_full = 3;
        static constexpr int no_id3v2_tag = 4;
        static constexpr int bad_mpeg_version = 5;
        static constexpr int bad_mpeg_layer = 6;
        static constexpr int bad_mpeg_bitrate = 7;
        static constexpr int bad_mpeg_samplerate = 8;
        static constexpr int lost_sync = 9;
        static constexpr int need_more_data = 10;
        static constexpr int bad_mpeg_channels = 11;
        static constexpr int bad_mpeg_emphasis = 12;
        // static constexpr int buffer_too_small = -6;

        int value = none;
        explicit operator int() const { return value; }
        operator bool() const { return value == 0 ? false : true; }
        int to_int() const { return value; }
        error() : value(none){};
        error(const int rhs) : value(rhs) {}
        bool operator==(const int rhs) { return value == rhs; }
        bool operator==(const error rhs) { return value == rhs.value; }
        bool is_errno() const { return value < 0; }

        const std::string to_string() {
            static const char* const values[] = {"none", "unknown", "no more data",
                "buffer full", "no id3v2 tag", "bad mpeg version", "bad mpeg layer",
                "bad mpeg bitrate", "bad samplerate", "lost sync", "need_more_data",
                "bad mpeg channels", "bad mpeg emphasis"};

            if (is_errno()) {
                return strerror(-value);
            }
            static constexpr int sz = sizeof(values);

            if (value < 0 || value >= sz) {
                return std::string("Unknown error");
            }
            return values[value];
        }
    };
    using seek_type = my::io::seek_type;

    template <typename READER_CALLBACK>
    class buffer : public my::io::buffer_guts_type<buffer<READER_CALLBACK>> {
        using base = typename my::io::buffer_guts_type<buffer<READER_CALLBACK>>;

        READER_CALLBACK m_cb;
        std::string m_uri;

        template <typename T>
        buffer(T reader, int dummy, string_view svuri)
            : base(*this), m_cb(reader), m_uri(svuri) {
            (void)dummy;
        }

        public:
        using base::begin;
        using base::clear;
        using base::data_begin;
        using base::data_end;
        using base::end;
        using base::read_set;
        using base::unread;

        template <typename RCB>
        static buffer<RCB> make_buffer(string_view svuri, RCB reader) {
            return buffer<RCB>(reader, 0, svuri);
        }

        buffer(string_view uri, READER_CALLBACK reader) : buffer(reader, 0, uri) {}

        const std::string& uri() const { return m_uri; }
        using base::capacity;
        using base::size;

        // return 0 on success, <0 for an errno, >0 for a custom error.
        error get(
            int& how_much, const seek_type& sk, char* const buf_into, bool peek = false) {

            char* bi = buf_into;

            int avail_space = how_much;
            char* ptr = nullptr;
            if (!buf_into) {

                ptr = buf_into != nullptr ? buf_into : this->writeptr(avail_space);
            } else {
                ptr = buf_into;
            }

            if (ptr == nullptr) {
                std::cout << "NOWHERE TO WRITE PTR, FFS" << std::endl;
                return error::buffer_full;
            }

            how_much = (std::min)(how_much, avail_space);

            assert(how_much > 0);

            error ret = m_cb(ptr, how_much, sk);
            if (static_cast<int>(ret) != 0) {
                return ret;
            }

            base::unread(base::unread() + how_much);

            return ret;
        }
    };

    using byte_type = my::io::byte_type;

    template <typename CB> using buffer_type = my::io::buffer_guts_type<CB>;

    static constexpr int8_t MPEG_HEADER_SIZE = 4;
    /*/
        The absolute theoretical maximum frame size is 2881 bytes: MPEG 2.5 Layer II,
        8000 Hz @ 160 kbps, with a padding slot. (Such a frame is unlikely, but it was
        a useful exercise to compute all possible frame sizes.) Add to this an 8 byte
        MAD_BUFFER_GUARD, and the minimum buffer size you should be streaming to
        libmad in the general case is 2889 bytes.

        Theoretical frame sizes for Layer III range from 24 to 1441 bytes, but there
        is a "soft" limit imposed by the standard of 960 bytes. Nonetheless MAD can
        decode frames of any size as long as they fit entirely in the buffer you pass,
        not including the MAD_BUFFER_GUARD bytes.
        So, if you don't want to dynamically allocate memory in the MPEG Frames, use a
    static buffer size of 1024 * 3 = 3072. It may be more efficient to use buffer sizes of
    1024 *4, though = 4096 bytes. If you are feeling adventurous, you could use a small
    buffer optimization strategy, and only dynamically allocate when the frame is more
    than 1024 bytes, which will not be very often if the majority of the files are in fact
    proper, layer III mp3 files.
    /*/
    static constexpr size_t MAX_STATIC_MPEG_PAYLOAD_SIZE = 1024;
    static constexpr size_t MAX_DYNAMIC_MPEG_PAYLOAD_SIZE = 4096;

    struct frame_header {
        // The actual header itself:
        my::io::byte_type header_bytes[MPEG_HEADER_SIZE] = {0};
        bool valid = {false};
        int64_t file_position = {-1};
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
        my::io::sbo_buffer<MAX_STATIC_MPEG_PAYLOAD_SIZE> m_sbo;
        frame_header* hdr = (frame_header*)m_sbo.begin();
        mpeg_properties props = mpeg_properties{0};
        bool valid{false};
        std::size_t size() const {
            return hdr->payload_size + sizeof(hdr->header_bytes) + props.padding;
        }
        void set_header_bytes(my::io::byte_type* p, size_t cb) {
            assert(cb == MPEG_HEADER_SIZE);
            m_sbo.append(p, cb);
            assert(m_sbo.size() == MPEG_HEADER_SIZE);
        }
        void clear() { valid = false; }
    }; // namespace mpeg
    struct frame : frame_base {

        bool operator!() const { return !frame_base::hdr->valid; }
        operator bool() const { return frame_base::hdr->valid; }
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
        int find_next_frame(const frame& prev, frame& next, const buffer_type<T>& buf) {
            next.clear();
            return 0;
        }

        using frames_t = frame[2];
        template <typename F, size_t N> inline void init_frames(F (&frames)[N]) {
            for (int i = 0; i < N; ++i) {
                frames[i].clear();
            }
        }

        inline error frame_version(frame& frame) {
            error e;
            const auto& data = frame.hdr->header_bytes;
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
                = (static_cast<unsigned char>(frame.hdr->header_bytes[1]) >> 1) & 0x03;
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
                = (static_cast<unsigned int>(frame.hdr->header_bytes[2]) >> 4) & 0x0F;

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
                = (static_cast<unsigned char>(frame.hdr->header_bytes[2]) >> 2) & 0x03;

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
                = ((static_cast<unsigned char>(frame.hdr->header_bytes[3]) >> 6) & 0x03);

            static constexpr int ChannelMode[MAX_MPEG_CHANNEL_MODE] = {CHANNELS_STEREO,
                CHANNELS_JOINT_STEREO, CHANNELS_DUAL_CHANNEL, CHANNELS_SINGLE_CHANNEL};

            if (cmodeindex >= MAX_MPEG_CHANNEL_MODE) {
                e = error::bad_mpeg_channels;
                return e;
            }

            frame.props.channelmode = ChannelMode[cmodeindex];

            auto data = frame.hdr->header_bytes;
            // phead->origin = ((static_cast<unsigned char>(data[3]) & 0x04) !=
            // 0);
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

        template <typename IO> byte_type* find_sync(IO& buf, const int start_position) {
            const auto e = const_cast<unsigned char*>(buf.data_end());
            auto* p = const_cast<unsigned char*>(buf.begin());
            p += start_position;
            while (e - p >= 4) {
                if (*p == 255 && *(p + 1) >= 224) {
                    return static_cast<byte_type*>(p);
                }
                ++p;
            }
            return nullptr;
        }
        inline error parse_frame_header(frame& frame) {
            error e = frame_version(frame);
            if (static_cast<int>(e) != 0) {
                return e;
            }
            e = frame_layer(frame);
            if (static_cast<int>(e) != 0) {
                return e;
            }
            e = frame_bitrate(frame);
            if (static_cast<int>(e) != 0) {
                return e;
            }
            e = frame_samplerate(frame);
            if (static_cast<int>(e) != 0) {
                return e;
            }
            e = frame_emph_copyright_etc(frame);
            return e;
        }

        template <typename CB> using _buffer_type = buffer_type<CB>;
        template <typename IO> inline error get_id3v2_tag(IO&& io, id3v2Header& id3) {

            seek_type sk(0, seek_value_type::seek_from_begin);
            int how_much = detail::ID3V2_HEADER_SIZE;
            char* const p = reinterpret_cast<char* const>(&id3);
            id3.tagsize_inc_header = 0;

            error e = detail::read_io(io, how_much, sk, p);
            if (e) {
                return e;
            }
            const int found = memcmp(p, "ID3", 3);
            if (found != 0) {
                return error::no_id3v2_tag;
            }

            if (found == 0) {
                /*/
            //    ID3v2/file identifier	"ID3"				0, 1, 2		3
            //    ID3v2 version			$04 00 / $03 00		3, 4		2
            //   ID3v2 flags				%abcd0000			5			1
            //   ID3v2 size			4 *	%0xxxxxxx			6,7,8,9		4
            //    a:	Unsynchronizaation.
            //   b:	Has extended header (is 10 bytes longer than expected).
            //   c:	Experimental.
            //   d:	Footer present.
            /*/

                uint32_t usize
                    = detail::DecodeSyncSafe(reinterpret_cast<char*>((&id3)->size));
                if (usize != 0u) {
                    usize += ID3V2_HEADER_SIZE;
                }
                if (usize > detail::ID3V2_MAX_SIZE) {
                    assert("MAX ID3 SIZE EXCEEDED!" == nullptr);
                    id3.tagsize_inc_header = 0;
                } else {
                    id3.tagsize_inc_header = usize;
                }

                return error::none;
            }
            return e;
        }

        template <typename IO>
        static error read_io(IO&& io, int& how_much,
            my::io::seek_type sk = my::io::seek_type(), char* const your_buf = nullptr,
            bool peek = false) {

            auto& myio = std::forward<IO&>(io);
            error e = myio.get(
                how_much, std::forward<const my::io::seek_type&>(sk), your_buf, peek);
            if (e) {
                fprintf(stderr, "%s %s %s %s %i\n\n",
                    "Error reading io device:", e.to_string().c_str(),
                    "\n For:", io.uri().c_str(), e.to_int());
            }
            return e;
        }
    } // namespace detail

    using seek_t = my::io::seek_type;
    struct io_base : public my::io::buffer_guts_type<io_base> {};
    class parser {

        private:
        detail::frames_t m_frames;

        template <typename IO>
        error single_frame(IO&& buf, int buf_pos, frame& f, int filepos) {

            error e;

            const auto unread = buf.unread();
            if (unread < detail::MPEG_HEADER_SIZE) {
                return my::mpeg::error::need_more_data;
            }

            byte_type* psync = detail::find_sync(buf, 0);
            if (psync == nullptr) {
                e = error::lost_sync;
                return e;
            }

            f.set_header_bytes(psync, 4);
            auto* ptr = f.hdr;
            e = detail::parse_frame_header(f);
            if (e != 0) {
                return e;
            }

            return e;
        }

        template <typename IO> error get_id3v1(IO&& io, detail::ID3V1& v1tag) {

            error e;
            seek_t sk(128, seek_value_type::seek_from_end);
            v1tag = detail::ID3V1();
            int how_much = io::capacity();
            io.clear();

            e = detail::read_io(io, how_much, seek_t::seek_from_end);
            if (e) return e;

            if (e == 0 && how_much >= 128) {
                auto const v = memcmp(io.begin(), "TAG", 3);
            }
            return e;
        }
        template <typename IO>
        error get_id3(IO&& io, detail::id3v2Header& v2Header, detail::ID3V1& v1Tag) {

            error e = error::need_more_data;
            memset(&v1Tag, 0, 3);
            int how_much = 128;
            seek_t sk(128, seek_value_type::seek_from_end);

            io.clear();
            e = detail::read_io(io, how_much, sk, reinterpret_cast<char*>(&v1Tag));
            if (e) {
                return e;
            }
            e = detail::get_id3v2_tag(io, v2Header);
            return e;
        }

        static bool id3v1_valid(const my::mpeg::detail::ID3V1& tag) {
            return memcmp(&tag, "TAG", 3) == 0;
        }
        template <typename IO> error find_first_frames(IO&& io) {
            detail::init_frames(m_frames);

            error e;
            e = get_id3(io, m_id3v2Header, m_id3v1Tag);

            if (e == error::no_id3v2_tag) {

            } else {
                if (e.value != error::none) return e;
            }

            e = 0;
            auto headers_size = id3v1_valid(m_id3v1Tag) ? 128 : 0;
            headers_size += m_id3v2Header.tagsize_inc_header;
            this->m_payload_size = this->file_size - m_payload_size;

            io.clear();
            int start_where
                = m_id3v2Header.tagsize_inc_header ? m_id3v2Header.tagsize_inc_header : 0;
            const auto sk
                = my::io::seek_type(start_where, seek_value_type::seek_from_begin);
            int cap = io::detail::BUFFER_CAPACITY;
            e = detail::read_io(io, cap, sk);
            if (e) return e;

            // find first frame:

            e = single_frame(
                io, 0, m_frames[0], start_where + io::detail::BUFFER_CAPACITY);
            /*/
            int which = 0;
            bool eof_flag = false;

            while (e == error::none) {
                start_where += frames[which].size();
                auto end = io.begin() + frames[which].size();
                if (end >= io.end()) {
                    auto how_much = io.capacity();
                    err = detail::read_io(io, how_much,
                        std::forward<my::io::seek_type&&>(my::io::seek_type()));
                    if (err) {
                        if (err == error::no_more_data) {
                            eof_flag = true;
                        }
                    }
                }
                e = single_frame(io, start_where, frames[which]);
                if (frames[0] == frames[1]) {
                    break;
                }
                which = which == 0 ? 1 : 0;
                /*/
            return e;
        }
        // using buffer_t = buffer_type<IO>;
        uint32_t nframes{0};
        int64_t file_size{-1};
        std::string filepath;
        my::mpeg::error err;
        frame m_first;
        detail::id3v2Header m_id3v2Header;
        detail::ID3V1 m_id3v1Tag;
        int8_t m_id3v1_size = 0;
        int64_t m_payload_size = 0;

        // IO& m_buf;

        parser(int dum, string_view file_path, uintmax_t file_size)
            : file_size(CAST(int64_t, file_size)), filepath(file_path) {
            (void)dum;
            // puts("parser private construct");
        }

        public:
        NO_COPY_AND_ASSIGN(parser);
        NO_MOVE_AND_ASSIGN(parser);
        using seek_value_type = my::io::seek_value_type;

        parser(string_view file_path, uintmax_t file_size)
            : parser(0, file_path, file_size) {}

        template <typename IO> mpeg::error parse(IO&& myio) {

            mpeg::error e = 0;
            e = find_first_frames(myio);
            if (e) {
                assert(0);
            }

            assert(e == 0);
            return e;
        }
    };

} // namespace mpeg
} // namespace my
