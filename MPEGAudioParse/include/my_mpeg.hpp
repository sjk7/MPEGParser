// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

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

        [[maybe_unused]] static inline const char* Channel_String(
            ChannelEnum ce) {
            switch (ce) {
                assert(ce >= CHANNELS_STEREO && ce <= CHANNELS_SINGLE_CHANNEL);
                case CHANNELS_STEREO: return "Stereo";
                case CHANNELS_JOINT_STEREO: return "Double Mono";
                case CHANNELS_DUAL_CHANNEL: return "Joint Stereo";
                case CHANNELS_SINGLE_CHANNEL: return "Single Channel mono";
            };
        }

        enum EmphasisEnum {
            EMPHASIS_NONE = 0,
            EMPHASIS_FIFTY_FIFTEEN_MS = 1,
            EMPHASIS_RESERVED = 2,
            EMPHASIS_CCIT_J17 = 3
        };

        [[maybe_unused]] static inline const char* Emphasis_String(
            EmphasisEnum ce) {
            assert(ce >= EMPHASIS_NONE && ce <= EMPHASIS_CCIT_J17);
            switch (ce) {
                case EMPHASIS_NONE: return "No Emphasis";
                case EMPHASIS_FIFTY_FIFTEEN_MS:
                    return "15 Millisecond Emphasis";
                case EMPHASIS_RESERVED: return "Reserved (BAD) Emphasis";
                case EMPHASIS_CCIT_J17:
                    return "CCIT_J17 Emphasis";
                    // default: return "unknown emphasis";
            };
        }
        static constexpr int MIN_MPEG_PAYLOAD = 512;
    } // namespace detail

    struct error {
        enum class error_code : int {
            none = 0,
            unknown = 1,
            no_more_data = 2,
            buffer_full = 3,
            no_id3v2_tag = 4,
            bad_mpeg_version = 5,
            bad_mpeg_layer = 6,
            bad_mpeg_bitrate = 7,
            bad_mpeg_samplerate = 8,
            lost_sync = 9,
            need_more_data = 10,
            bad_mpeg_channels = 11,
            bad_mpeg_emphasis = 12,
            tiny_file = 13
        };

        // static constexpr int buffer_too_small = -6;

        error_code value = error_code::none;
        // use to_int() here instead. otherwise operator bool() gets called ffs
        // explicit operator int() const { return CAST(int, value); }
        operator int() const = delete;
        operator bool() const {
            return value == error_code::none ? false : true;
        }
        int to_int() const { return CAST(int, value); }
        error() = default;
        error(const error_code rhs) : value(rhs) {}
        bool operator==(const error_code rhs) { return value == rhs; }
        bool operator==(const error rhs) { return value == rhs.value; }
        bool is_errno() const { return to_int() < 0; }

        const std::string to_string() {
            static const char* const values[]
                = {"none", "unknown", "no more data", "buffer full",
                    "no id3v2 tag", "bad mpeg version", "bad mpeg layer",
                    "bad mpeg bitrate", "bad samplerate", "lost sync",
                    "need_more_data", "bad mpeg channels", "bad mpeg emphasis",
                    "file payload too small to contain any meaningful audio"};

            if (is_errno()) {
                return strerror(-to_int());
            }
            // See:
            // https://stackoverflow.com/questions/9522760/find-the-number-of-strings-in-an-array-of-strings-in-c
            static constexpr int sz = sizeof(values) / sizeof(values[0]);

            int val = to_int();
            if (val < 0 || val >= sz) {
                return std::string("Unknown error");
            }
            return values[val];
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
        // using base::read_set;
        // using base::unread;

        template <typename RCB>
        static buffer<RCB> make_buffer(string_view svuri, RCB reader) {
            return buffer<RCB>(reader, 0, svuri);
        }

        buffer(string_view uri, READER_CALLBACK reader)
            : buffer(reader, 0, uri) {}

        const std::string& uri() const { return m_uri; }
        using base::capacity;
        using base::size;

        // return 0 on success, <0 for an errno, >0 for a custom error.
        error get(int& how_much, const seek_type& sk, char* const buf_into,
            bool /*peek*/ = false) {

            char* bi = buf_into;
            assert(bi);
            int avail_space = how_much;
            char* ptr = nullptr;
            if (buf_into == nullptr) {

                // ptr = buf_into != nullptr ? buf_into :
                // this->writeptr(avail_space);
            } else {
                ptr = buf_into;
            }

            if (ptr == nullptr) {
                puts("NOWHERE TO WRITE PTR, [get()]");
                how_much = 0;
                return error::error_code::buffer_full;
            }

            how_much = (std::min)(how_much, avail_space);

            assert(how_much > 0);
            error ret;
            int rv = m_cb(ptr, how_much, sk);
            if (rv == my::io::NO_MORE_DATA) {
                ret = error::error_code::no_more_data;
                return ret;
            }

            if (rv < 0) {
                how_much = 0;
                return error(static_cast<error::error_code>(rv));
            }
            return ret;
        }
    };

    using byte_type = my::io::byte_type;

    template <typename CB> using buffer_type = my::io::buffer_guts_type<CB>;

    static constexpr int8_t MPEG_HEADER_SIZE = 4;
    /*/
        The absolute theoretical maximum frame size is 2881 bytes: MPEG 2.5
    Layer II, 8000 Hz @ 160 kbps, with a padding slot. (Such a frame is
    unlikely, but it was a useful exercise to compute all possible frame sizes.)
    Add to this an 8 byte MAD_BUFFER_GUARD, and the minimum buffer size you
    should be streaming to libmad in the general case is 2889 bytes.

        Theoretical frame sizes for Layer III range from 24 to 1441 bytes, but
    there is a "soft" limit imposed by the standard of 960 bytes. Nonetheless
    MAD can decode frames of any size as long as they fit entirely in the buffer
    you pass, not including the MAD_BUFFER_GUARD bytes. So, if you don't want to
    dynamically allocate memory in the MPEG Frames, use a static buffer size of
    1024 * 3 = 3072. It may be more efficient to use buffer sizes of 1024 *4,
    though = 4096 bytes. If you are feeling adventurous, you could use a small
    buffer optimization strategy, and only dynamically allocate when the frame
    is more than 1024 bytes, which will not be very often if the majority of the
    files are in fact proper, layer III mp3 files.
    /*/
    static constexpr size_t MAX_STATIC_MPEG_PAYLOAD_SIZE = 1024;
    static constexpr size_t MAX_DYNAMIC_MPEG_PAYLOAD_SIZE = 4096;

    // static constexpr int versions[] = {1, 2, 3};
    // static constexpr int bitrates[] = {88, 77};
    // static constexpr int samplerates[] = {44100, 48000, 196000};
    struct mpeg_properties {
        uint8_t version{0};
        uint8_t layer{0};
        int16_t padda{0};
        int samplerate{0};
        int bitrate{0};
        uint8_t padding{0};
        uint8_t crc{0};
        uint8_t copyright{0};
        uint8_t emphasis{0};
        uint8_t channelmode{0};
        char cpad[3] = {0};
    };

    struct frame_base {
        my::io::sbo_buf<byte_type, MAX_STATIC_MPEG_PAYLOAD_SIZE> m_sbo;
        int64_t file_position = {-1};
        // The amount of data after the header and before the end of the frame
        int payload_size = {0};
        mpeg_properties props = mpeg_properties{0};
        const unsigned char* const frame_header = (const unsigned char* const)m_sbo.cbegin();
        mutable bool valid = {false};
        char pad[7];
        const unsigned char* header_bytes;
        // represents any valid data we hold in the buffer after this frame:
        struct extra_data_type {
            const byte_type* ptr{nullptr};
            int64_t cb{0};
        };
        extra_data_type extra_data = {};

        const mpeg_properties& props_const() const { return props; }

        // This is the TOTAL length, in bytes, of the frame, INCLUDING the 4
        // header bytes.
        int length_in_bytes() const {
            const auto len = frame_get_length();
            if (len > 0) {
                valid = true;
            } else {
                valid = false;
            }
            return len;
        }
        void set_file_position(int64_t file_pos = -1) {
            valid = false;
            file_position = file_pos;
            header_bytes = reinterpret_cast<const unsigned char*>(m_sbo.begin());
        }
        void clear(bool keep_extra_data = false) {
            valid = false;
            if (!keep_extra_data) {
                extra_data = extra_data_type();
            }
        }

        private:
        int frame_get_length() const {
            int sz = 0;
            if (props.layer == 1) {
                sz = ((12 * (props.bitrate) / props.samplerate + props.padding)
                    * 4);
            } else {
                if (props.layer == 2) {
                    // they are all the same:
                    sz = (144 * props.bitrate / props.samplerate)
                        + props.padding;
                    // dFrameLength = ();
                } else {
                    //'MPEG2 and 2.5 are different, but only in layer3:
                    if (props.version != 1) {
                        //'nobody told me this! - this is DEFINITELY right for
                        // Version2,
                        // layer 3 files. 'and most readers do get this wrong!
                        // (including v3Mpeg.dll!)
                        sz = (72 * (props.bitrate) / props.samplerate
                            + props.padding);
                    } else {
                        sz = (144 * (props.bitrate) / props.samplerate
                            + props.padding);
                    }
                }
            }
            assert(sz);
            return sz;
        }
    }; // namespace mpeg

    struct frame : frame_base {
        bool operator!() const { return !frame_base::valid; }
        operator bool() const { return frame_base::valid; }
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
            return h.tagsize_inc_header > 10
                && (h.tagsize_inc_header < ID3V2_MAX_SIZE);
        }

        uint32_t DecodeSyncSafe(const char* pb) {
            /// For i As Integer = 0 To UBound(arBytes)
            // usum = usum Or (arBytes(i) And 127) << ((3 - i) * 7)
            // Next
            uint32_t retval = 0;
            if (pb == nullptr) {
                return 0;
            }

            for (uint32_t i = 0; i < 4; i++) {
                retval
                    = retval | CAST(uint32_t, (pb[i] & 127) << ((3 - i) * 7));
            }
            return retval;
        }

        static constexpr size_t NUM_MPEG_HEADERS = 4;
        using frames_t = frame[NUM_MPEG_HEADERS];
        template <typename F, int N> inline void init_frames(F (&frames)[N]) {
            for (int i = 0; i < N; ++i) {
                frames[i].clear();
            }
        }

        inline error frame_version(frame& frame) {
            error e;

            const auto& data = frame.header_bytes;
            frame.props.crc = static_cast<uint8_t>(
                static_cast<unsigned char>(data[1] & 0x01) == 0);
            static constexpr int MAX_MPEG_VERSIONS = 3;
            unsigned char version_index
                = (static_cast<unsigned char>(data[1]) >> 3) & 0x03;
            static constexpr unsigned char MPEGVersions[MAX_MPEG_VERSIONS]
                = {3, 2, 1};

            const int ver_out_of_box = version_index;
            if (ver_out_of_box == 1) {
                e = error::error_code::bad_mpeg_version;
                return e;
            }
            if (version_index == 2) {
                version_index = 1;
            }
            if (version_index == 3) {
                version_index = 2;
            }
            frame.props.version = MPEGVersions[version_index];
            return e;
        }
        inline error frame_layer(frame& frame) noexcept {
            error e;
            static constexpr int MAX_MPEG_LAYERS = 4;
            static constexpr int MPEGLayers[MAX_MPEG_LAYERS] = {-1, 3, 2, 1};
            const unsigned char layer_index
                = (static_cast<unsigned char>(frame.header_bytes[1]) >> 1)
                & 0x03;
            if (layer_index == 0) {
                e = error::error_code::bad_mpeg_layer;
                return e;
            }
            frame.props.layer = CAST(uint8_t, MPEGLayers[layer_index]);
            assert(frame.props.layer);
            if (frame.props.layer == 0u) {
                e = error::error_code::bad_mpeg_layer;
            }

            return e;
        }
        error frame_bitrate(frame& frame) {

            error e;
            static constexpr int MAX_MPEG_BITRATES = 16;
            static int V1L1BitRates[MAX_MPEG_BITRATES] = {0, 32, 64, 96, 128,
                160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1};
            static constexpr int V1L2BitRates[MAX_MPEG_BITRATES] = {0, 32, 48,
                56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, -1};
            static constexpr int V1L3BitRates[MAX_MPEG_BITRATES] = {0, 32, 40,
                48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1};
            static constexpr int V2L1BitRates[MAX_MPEG_BITRATES] = {0, 32, 48,
                56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, -1};
            static constexpr int V2L2L3BitRates[MAX_MPEG_BITRATES] = {0, 8, 16,
                24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, -1};

            const unsigned int bitrate_index
                = (static_cast<unsigned int>(frame.header_bytes[2]) >> 4)
                & 0x0F;

            static constexpr unsigned char BAD_BITRATE_INDEX = 0;
            if (bitrate_index >= MAX_MPEG_BITRATES
                        - 1 // -1 coz all values @ 16 -1 are OK for
                            // us, 16 itself is -1, which is not.
                || bitrate_index == BAD_BITRATE_INDEX) {
                e = error::error_code::bad_mpeg_bitrate;
            }

            assert(frame.props.version);
            assert(frame.props.layer);
            if (frame.props.version == 1) {
                switch (frame.props.layer) {
                    case 3: {
                        frame.props.bitrate
                            = V1L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 2: {
                        frame.props.bitrate
                            = V1L2BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 1: {
                        frame.props.bitrate
                            = V1L1BitRates[bitrate_index] * 1000;
                        break;
                    }
                    default: return error::error_code::bad_mpeg_bitrate;
                };

            } else {
                switch (frame.props.layer) {
                    case 3: {
                        frame.props.bitrate
                            = V2L2L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 2: {
                        frame.props.bitrate
                            = V2L2L3BitRates[bitrate_index] * 1000;
                        break;
                    }
                    case 1: {
                        frame.props.bitrate
                            = V2L1BitRates[bitrate_index] * 1000;
                        break;
                    }
                    default:
                        // same fo v2 abd v2.5 here:
                        frame.props.bitrate
                            = V2L2L3BitRates[bitrate_index] * 1000;
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
                = (static_cast<unsigned char>(frame.header_bytes[2]) >> 2)
                & 0x03;

            if (samplerate_index == 3) {
                e = error::error_code::bad_mpeg_samplerate;
                return e;
            }

            if (static_cast<int>(frame.props.version) == 1) {
                frame.props.samplerate = MPEG1SampleRates[samplerate_index];
            } else if (static_cast<int>(frame.props.version) == 2) {
                frame.props.samplerate = MPEG2SampleRates[samplerate_index];
            } else if (frame.props.version > 2) {
                frame.props.samplerate
                    = MPEG2Point5SampleRates[samplerate_index];
            }

            assert(frame.props.samplerate && "no samplerate");
            return e;
        }

        error frame_emph_copyright_etc(frame& frame) {
            error e;
            static constexpr int MAX_MPEG_CHANNEL_MODE = 4;
            const unsigned int cmodeindex
                = ((static_cast<unsigned char>(frame.header_bytes[3]) >> 6)
                    & 0x03);

            static constexpr int ChannelMode[MAX_MPEG_CHANNEL_MODE]
                = {CHANNELS_STEREO, CHANNELS_JOINT_STEREO,
                    CHANNELS_DUAL_CHANNEL, CHANNELS_SINGLE_CHANNEL};

            assert(cmodeindex < MAX_MPEG_CHANNEL_MODE);

            if (cmodeindex >= MAX_MPEG_CHANNEL_MODE) {
                e = error::error_code::bad_mpeg_channels;
                return e;
            }
            frame.props.channelmode = CAST(uint8_t, ChannelMode[cmodeindex]);

            auto data = frame.header_bytes;
            // phead->origin = ((static_cast<unsigned char>(data[3]) & 0x04) !=
            // 0);
            frame.props.copyright = static_cast<uint8_t>(
                (static_cast<unsigned char>(data[3]) & 0x08) != 0);
            frame.props.padding = static_cast<uint8_t>(
                (static_cast<unsigned char>(data[2]) & 0x02) != 0);

            static constexpr int MAX_EMPH = 4;
            unsigned int emphasis
                = ((static_cast<unsigned int>(data[3]) & 0x03));

            static constexpr EmphasisEnum emph_lookup[MAX_EMPH]
                = {EmphasisEnum::EMPHASIS_NONE,
                    EmphasisEnum::EMPHASIS_FIFTY_FIFTEEN_MS,
                    EmphasisEnum::EMPHASIS_RESERVED,
                    EmphasisEnum::EMPHASIS_CCIT_J17};
            frame.props.emphasis = emph_lookup[emphasis];
            frame.valid = true;
            return e;
        }

        template <typename IO>
        const unsigned char* find_sync(IO& buf, const int buf_position) {

            const unsigned char*  e  = reinterpret_cast<const unsigned char*>(buf.data_end());
            const unsigned char* p = reinterpret_cast<const unsigned char*>(buf.begin());
            const auto sz = e - p;
            assert(sz);
            p += buf_position;
            while (e - p >= 4) {
                if (*p == 255 && *(p + 1) >= 224) {
                    return reinterpret_cast<const unsigned char*>(p);
                }
                ++p;
            }
            return nullptr;
        }

        inline error parse_frame_header(frame& frame) {
            error e = frame_version(frame);
            if (e) {
                return e;
            }
            e = frame_layer(frame);
            if (e) {
                return e;
            }
            e = frame_bitrate(frame);
            if (e) {
                return e;
            }
            e = frame_samplerate(frame);
            if (e) {
                return e;
            }
            e = frame_emph_copyright_etc(frame);
            return e;
        }

        template <typename IO>
        [[maybe_unused]] static error read_io(IO&& io, int& how_much, char* const your_buf,
            my::io::seek_type sk = my::io::seek_type(),
            bool peek = false) {

            assert(your_buf);

            auto& myio = std::forward<IO&>(io);
            error e = myio.get(how_much,
                std::forward<const my::io::seek_type&>(sk), your_buf, peek);
            if (e) {
                fprintf(stderr, "%s %s %s %s %i\n\n",
                    "Error reading io device:", e.to_string().c_str(),
                    "\n For:", io.uri().c_str(), e.to_int());
                assert("Error reading iodevice" == nullptr);
            }
            myio.size_set(how_much);
            return e;
        }

        template <typename CB> using _buffer_type = buffer_type<CB>;
        template <typename IO>
        inline error get_id3v2_tag(IO&& io, id3v2Header& id3) {

            seek_type sk(0, io::seek_value_type::seek_from_begin);
            int how_much = detail::ID3V2_HEADER_SIZE;
            char* const p = reinterpret_cast<char* const>(&id3);
            id3.tagsize_inc_header = 0;

            error e = my::mpeg::detail::read_io(io, how_much,p, sk);
            if (e) {
                return e;
            }
            const int found = memcmp(p, "ID3", 3);
            if (found != 0) {
                return error::error_code::no_id3v2_tag;
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

                uint32_t usize = detail::DecodeSyncSafe(
                    reinterpret_cast<char*>((&id3)->size));
                if (usize != 0u) {
                    usize += ID3V2_HEADER_SIZE;
                }
                if (usize > detail::ID3V2_MAX_SIZE) {
                    assert("MAX ID3 SIZE EXCEEDED!" == nullptr);
                    id3.tagsize_inc_header = 0;
                } else {
                    id3.tagsize_inc_header = usize;
                }

                return error::error_code::none;
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
        error single_frame(
            IO&& io, int io_pos, frame& f, int64_t filepos, my::io::seek_type sk = my::io::seek_type()) {

            (void) io_pos;
            auto& frame = f;
            int cap = static_cast<int>(frame.m_sbo.capacity());
            auto p =  static_cast<char*>(frame.m_sbo.begin());

            error e = detail::read_io(
                io, cap, p, sk);
            if (e) {
                return e;
            }
                f.m_sbo.size_set(CAST(size_t,cap));
            const byte_type* psync = nullptr;

            while (!psync) {

                psync = (const byte_type*)detail::find_sync(f.m_sbo, 0);
                if (psync == nullptr) {
                    e = error::error_code::lost_sync;
                    return e; // return unconditionally since psync is NULL.
                }

                f.set_file_position(filepos);
                e = detail::parse_frame_header(f);

                if (e == error::error_code::none) {
                    break;
                }
                if (e) {
                    assert("Could not parse frame header" == nullptr);
                    filepos++;
                }
            }; // while (!psync)

            const auto fsz_total = f.length_in_bytes();
            auto* ptr = psync + fsz_total;
            // finish reading all the data for the frame:
            if (ptr >= f.m_sbo.end()) {

                int how_much = static_cast<int>(ptr - f.m_sbo.end());

                const auto old_size = f.m_sbo.size();
                f.m_sbo.resize( CAST(size_t, how_much) + old_size);
                auto pwhere = f.m_sbo.data() + old_size;
                e = detail::read_io(io, how_much, static_cast<char*>(pwhere));

                if (e) {
                    return e;
                }

            } else {
                 // fits in sbo
            }
            return e;
        }

        template <typename IO> error get_id3v1(IO&& io, detail::ID3V1& v1tag) {

            error e;
            seek_t sk(128, seek_value_type::seek_from_end);
            v1tag = detail::ID3V1();
            int how_much = io::detail::BUFFER_CAPACITY;
            io.clear();

            e = detail::read_io(
                io, how_much, seek_t::seek_value_type::seek_from_end);
            if (e) return e;

            if (!e && how_much >= 128) {
                auto const v = memcmp(io.begin(), "TAG", 3);
            }
            return e;
        }
        template <typename IO>
        error get_id3(
            IO&& io, detail::id3v2Header& v2Header, detail::ID3V1& v1Tag) {

            error e = error::error_code::need_more_data;
            memset(&v1Tag, 0, 3);
            int how_much = 128;
            seek_t sk(128, seek_value_type::seek_from_end);

            io.clear();
            e = detail::read_io(
                io, how_much, reinterpret_cast<char*>(&v1Tag), sk);
            if (e) {
                return e;
            }
            e = detail::get_id3v2_tag(io, v2Header);
            return e;
        }

        static bool id3v1_valid(const my::mpeg::detail::ID3V1& tag) {
            return memcmp(&tag, "TAG", 3) == 0;
        }

        template <typename IO>
        error find_next_frame(IO&& buf, int buf_pos, const frame& prev_frame,
            frame& next_frame) {

            error e = single_frame(buf, buf_pos, next_frame, prev_frame.file_position);
            assert(e == error::error_code::none);

            return e;
        }

        template <typename IO>
        error find_first_frame(
            IO&& io, int buf_pos, int64_t file_pos, frame& f) {
            const auto sk = my::io::seek_type(
                file_pos, seek_value_type::seek_from_begin);
            error e = single_frame(io, buf_pos, f, file_pos, sk);
            return e;
        }

        template <typename IO> error find_first_frames(IO&& io) {
            detail::init_frames(m_frames);

            error e;
            e = get_id3(io, m_id3v2Header, m_id3v1Tag);

            if (e == error::error_code::no_id3v2_tag) {

            } else {
                if (e) {
                    return e;
                }
            }

            e = error::error_code::none;
            auto headers_size = id3v1_valid(m_id3v1Tag) ? 128 : 0;
            headers_size += m_id3v2Header.tagsize_inc_header;
            this->m_payload_size = this->file_size - headers_size;
            if (m_payload_size <= mpeg::detail::MIN_MPEG_PAYLOAD) {
                e = error::error_code::tiny_file;
                return e;
            }

            io.clear();
            int64_t start_where = m_id3v2Header.tagsize_inc_header
                ? m_id3v2Header.tagsize_inc_header
                : 0;

            int cur_frame_idx = 0;
            int next_frame_idx = 1;
            int buf_pos = 0;
            int64_t file_pos = -1;
            int n_confirmed_frames = 0;
            my::io::seek_type sk(start_where, seek_value_type::seek_from_begin);

            while (true) {

                e = single_frame(io, 0, m_frames[0], start_where, sk);

                while (true) {

                    if (e) {
                        if (e == error::error_code::need_more_data) {
                            assert(0);
                        } else {
                            assert(0);
                        }
                    } else {
                        n_confirmed_frames++;
                        file_pos = m_frames[cur_frame_idx].file_position;
                        buf_pos = 0;
                        e = find_next_frame(io, buf_pos,
                            m_frames[cur_frame_idx], m_frames[next_frame_idx]);

                        if (e == error::error_code::need_more_data) {
                            assert(0);
                        }else{
                            if (e == error::error_code::none){
                                cur_frame_idx++;
                                if (cur_frame_idx == detail::NUM_MPEG_HEADERS) {
                                    cur_frame_idx = 0;
                                }
                            }
                        }
                    }
                }
            }

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
        int64_t m_payload_size = 0;

        // IO& m_buf;

        parser(int dum, string_view file_path, uintmax_t file_size)
            : file_size(CAST(int64_t, file_size)), filepath(file_path) {
            (void)dum;
            // puts("parser private construct");
        }

        public:
        parser(const parser&) = delete;
        parser& operator=(const parser&) = delete;
        using seek_value_type = my::io::seek_value_type;

        parser(string_view file_path, uintmax_t file_size)
            : parser(0, file_path, file_size) {}

        template <typename IO> mpeg::error parse(IO&& myio) {

            mpeg::error e;
            e = find_first_frames(myio);
            if (e) {
                assert(0);
            }

            assert(e == error::error_code::none);
            return e;
        }
    };

} // namespace mpeg
} // namespace my
