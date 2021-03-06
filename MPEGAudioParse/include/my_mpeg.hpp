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
#include <array>
#ifdef _WIN32
#include <io.h> // access
#else
#include <unistd.h>
#endif
#include "my_string_view.hpp"
#include <iostream>

namespace my {
namespace mpeg {

    namespace detail {

        enum class loglevel_t {

            reserved,
            all,
            error_only,
            critical_only,
            detail,
            everything,

        };
        static constexpr loglevel_t loglevel = loglevel_t::all;

        static constexpr int ID3V2_HEADER_SIZE = 10;
        static constexpr uint32_t ID3V2_MAX_SIZE = 1024U * 1024U; // 1 meg

        enum ChannelEnum {
            CHANNELS_STEREO = 0,
            CHANNELS_JOINT_STEREO = 1,
            CHANNELS_DUAL_CHANNEL = 2,
            CHANNELS_SINGLE_CHANNEL = 3
        };

        [[maybe_unused]] static inline int nchannels(const ChannelEnum ce) noexcept {
            switch (ce) {
                case CHANNELS_STEREO:
                case CHANNELS_JOINT_STEREO:
                case CHANNELS_DUAL_CHANNEL: return 2;

                case CHANNELS_SINGLE_CHANNEL: return 1;
                default: {
                    assert("Bad Channel Enumeration value" == nullptr);
                    return 0;
                }
            }
        }

        [[maybe_unused]] static inline const char* Channel_String(
            ChannelEnum ce) noexcept {
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
            EmphasisEnum ce) noexcept {
            assert(ce >= EMPHASIS_NONE && ce <= EMPHASIS_CCIT_J17);
            switch (ce) {
                case EMPHASIS_NONE: return "No Emphasis";
                case EMPHASIS_FIFTY_FIFTEEN_MS: return "15 Millisecond Emphasis";
                case EMPHASIS_RESERVED: return "Reserved (BAD) Emphasis";
                case EMPHASIS_CCIT_J17:
                    return "CCIT_J17 Emphasis";
                    // default: return "unknown emphasis";
            };
        }
        static constexpr int MIN_MPEG_PAYLOAD = 512;
    } // namespace detail

    enum class frame_mismatch {
        none = 0,
        samplerate = 1,
        bitrate = 2,
        channelmode = 3,
        emphasis = 4,
        layer = 5,
        version = 6
    };

    inline std::string frame_mismatch_string(const frame_mismatch& fm) {
        switch (fm) {
            case frame_mismatch::none: return "No mismatch; they are the same";
            case frame_mismatch::samplerate: return "Samplev rates differ";
            case frame_mismatch::bitrate: return "Bit rates differ";
            case frame_mismatch::channelmode: return "Channel modes differ";
            case frame_mismatch::emphasis: return "Emphasis differs";
            case frame_mismatch::layer: return "Layers differ";
            case frame_mismatch::version: return "Versions differ";
            default:

                assert("unhandled frame mismatch" == nullptr);
                return "unknown mismatch";
        }
    }

    struct error {
        enum class error_code : int {
            noerror = 0,
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
            tiny_file = 13,
            prev_frame_bad = 14,
            data_incomplete = 15

        };

        // static constexpr int buffer_too_small = -6;

        error_code value = error_code::noerror;
        // use to_int() here instead. otherwise operator bool() gets called ffs
        // explicit operator int() const { return CAST(int, value); }
        operator int() const = delete;
        operator bool() const noexcept {
            return value == error_code::noerror ? false : true;
        }
        int to_int() const noexcept { return CAST(int, value); }
        error() noexcept = default;
        error(const error& rhs) noexcept = default;

        error(const frame_mismatch& rhs) noexcept
            : value(static_cast<error::error_code>(rhs)) {
            m_bis_frame_mismatch = true;
        }
        error(const error_code& e) noexcept : value(e) {}
        error(error&& e) noexcept = default;
        error& operator=(const error& rhs) noexcept = default;

        bool operator==(const error& rhs) const noexcept {
            return value == rhs.value
                && (rhs.is_frame_mismatch() == this->is_frame_mismatch());
        }
        bool operator!=(const error& rhs) const noexcept { return !(operator==(rhs)); }

        bool is_errno() const noexcept { return to_int() < 0; }
        bool is_frame_mismatch() const noexcept { return m_bis_frame_mismatch; }

#ifdef _MSC_VER
#pragma warning(disable : 26446)
#endif
        const std::string to_string() {

            if (m_bis_frame_mismatch) {
                int i = to_int();
                auto fm = static_cast<frame_mismatch>(i);
                return frame_mismatch_string(fm);
            }
            static const char* const values[] = {"noerror", "unknown", "no more data",
                "buffer full", "no id3v2 tag", "bad mpeg version", "bad mpeg layer",
                "bad mpeg bitrate", "bad samplerate", "lost sync", "need_more_data",
                "bad mpeg channels", "bad mpeg emphasis",
                "file payload too small to contain any meaningful audio",
                "previous frame bad", "data incomplete"};

            if (is_errno()) {
                return strerror(-to_int());
            }
            // See:
            // https://stackoverflow.com/questions/9522760/find-the-number-of-strings-in-an-array-of-strings-in-c
            static constexpr int sz = sizeof(values) / sizeof(values[0]);

            const int val = to_int();
            if (val < 0 || val >= sz) {
                return std::string("Unknown error");
            }

#ifdef _MSC_VER
#pragma warning(disable : 26482)
#endif
            return values[val];
        }

        private:
        bool m_bis_frame_mismatch{false};
    };

    using seek_type = my::io::seek_type;

    template <typename READER_CALLBACK>
    class buffer : public my::io::buffer_guts_type<buffer<READER_CALLBACK>> {
        using base = typename my::io::buffer_guts_type<buffer<READER_CALLBACK>>;

        READER_CALLBACK&& m_cb = nullptr;

        std::string m_uri;

        template <typename T>
        buffer(T&& reader, int dummy, string_view svuri)
            : base(*this), m_cb(std::forward<READER_CALLBACK>(reader)), m_uri(svuri) {
            CAST(void, dummy);
        }

        public:
        buffer() = delete;
        buffer(const buffer& rhs) = delete;
        const std::string uri() const { return m_uri; }
        using base::begin;
        using base::clear;
        using base::data_begin;
        using base::data_end;
        using base::end;

        template <typename RCB>
        static buffer<RCB> make_buffer(string_view svuri, RCB reader) {
            return buffer<RCB>(reader, 0, svuri);
        }

#ifdef _MSC_VER
#pragma warning(disable : 26495)
#endif
        buffer(string_view uri, READER_CALLBACK&& reader) : buffer(reader, 0, uri) {}

#ifdef _MSC_VER
#pragma warning(default : 26495)
#endif

        using base::capacity;
        using base::size;

        // return 0 on success, <0 for an errno, >0 for a custom error.
        error get(int& how_much, const seek_type& sk, char* const buf_into,
            bool /*peek*/ = false) {

            const char* bi = buf_into;
            assert(bi);
            const int avail_space = how_much;
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
            const int rv = m_cb(ptr, how_much, sk);
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
        uint8_t version = {0};
        uint8_t layer = {0};
        uint16_t padda = {0};
        int samplerate = {0};
        int bitrate = {0};
        int padding = {0};
        bool crc = {false};
        bool copyright = {false};
        uint8_t emphasis = {0};
        uint8_t channelmode = {0};
        char cpad[4] = {0};
    };

    struct frame_base {

        my::io::sbo_buf<byte_type, MAX_STATIC_MPEG_PAYLOAD_SIZE> m_sbo;
        const unsigned char* header_bytes;
        int64_t file_position = {-1};
        // The amount of data after the header and before the end of the frame
        int payload_size = {0};
        mpeg_properties props = mpeg_properties{0};
        char fill[4];
        const unsigned char* const frame_header
            = reinterpret_cast<const unsigned char* const>(m_sbo.cbegin());
        mutable bool valid = {false};
        const mpeg_properties& props_const() const noexcept { return props; }

        // size_in_bytes() returns the TOTAL length of the frame, including the
        // header
        int size_in_bytes() const noexcept { return frame_get_length(); }

        int frame_dur_in_ms() const noexcept {
            if (!valid) {
                return 0;
            }

            const float frameTimeMs
                = (1000.0f / static_cast<float>(props_const().samplerate))
                * static_cast<float>(samples_per_frame());
            return static_cast<int>(frameTimeMs);
        }
        // This is the TOTAL length, in bytes, of the frame, INCLUDING the 4
        // header bytes.
        int length_in_bytes() const noexcept {
            if (this->m_frame_len != 0) {
                return m_frame_len;
            }
            const auto len = frame_get_length();
            if (len > 0) {
                valid = true;
                m_frame_len = len;
            } else {
                valid = false;
            }
            return len;
        }

        void clear() noexcept {
            valid = false;
            m_vbr = false;
            props = mpeg_properties{0};
            m_frame_len = 0;
        }
        bool vbr() const noexcept { return m_vbr; }
        void set_file_position(int64_t file_pos = -1) noexcept {
            valid = false;
            file_position = file_pos;
            header_bytes = reinterpret_cast<const unsigned char*>(m_sbo.cbegin());
        }

        protected:
        bool m_vbr = {false};
        char b[2];
        mutable int m_frame_len = 0;

        int frame_get_length() const noexcept {
            int sz = 0;
            if (!valid) {
                return 0;
            }
            if (props.layer == 1) {
                sz = ((12 * (props.bitrate) / props.samplerate + props.padding) * 4);
            } else {
                if (props.layer == 2) {
                    // they are all the same:
                    sz = (144 * props.bitrate / props.samplerate) + props.padding;
                    // dFrameLength = ();
                } else {
                    //'MPEG2 and 2.5 are different, but only in layer3:
                    if (props.version != 1) {
                        //'nobody told me this! - this is DEFINITELY right for
                        // Version2,
                        // layer 3 files. 'and most readers do get this wrong!
                        // (including v3Mpeg.dll!)
                        sz = (72 * (props.bitrate) / props.samplerate + props.padding);
                    } else {
                        sz = (144 * (props.bitrate) / props.samplerate + props.padding);
                    }
                }
            }
            assert(sz);
            return sz;
        }
        char pad[6];
        int samples_per_frame() const noexcept {
            /*/ SamplesPerFrame depends on both the MPEG version and Layer.
             * For MPEG-1.0 Layer-III and Layer-II the samples per frame
             * is 1152 bytes.
             * For MPEG-1.0 Layer-I it's 384 bytes.
             * For MPEG-2.0 and MPEG-2.5,
             * Layer-I is 384 bytes, Layer-II is 1152 bytes,
             * and Layer-III is 576 bytes.
             /*/
            if (!valid) {
                return 0;
            }
            const auto& p = props_const();
            if (p.version == 1) {
                if (p.layer == 1) {
                    return 384;
                }
                return 1152;
            }
            if (p.version >= 2 && p.version < 3) {
                if (p.layer == 1) {
                    return 384;
                }
                if (p.layer == 2) {
                    return 1152;
                }
                if (p.layer == 3) {
                    return 576;
                }
            }
            assert(0); // what have I missed?
            return 0;
        }
        char padd[4];

    }; // frame_base

    inline error::error_code error_from_frame_mismatch(const frame_mismatch& fm) {
        using namespace my::mpeg;
        switch (fm) {
            case frame_mismatch::none: return error::error_code::noerror;
            case frame_mismatch::samplerate:
                return error::error_code::bad_mpeg_samplerate;
            case frame_mismatch::bitrate: return error::error_code::bad_mpeg_bitrate;
            case frame_mismatch::emphasis: return error::error_code::bad_mpeg_emphasis;
            case frame_mismatch::channelmode: return error::error_code::bad_mpeg_channels;
            case frame_mismatch::layer: return error::error_code::bad_mpeg_layer;
            case frame_mismatch::version: return error::error_code::bad_mpeg_layer;
        }
    }

#ifdef _MSC_VER
#pragma warning(disable : 6326)
#endif

    struct frame : frame_base {
        char fillme[6];
        frame() = default;

        error parse_header(int64_t file_position) {
            assert(file_position >= 0);
            if (my::mpeg::detail::loglevel >= my::mpeg::detail::loglevel_t::all) {
                printf("Parsing MPEG header @ file position %lu ...\n",
                    static_cast<unsigned long>(file_position));
            }

            if (m_sbo.size() < MPEG_HEADER_SIZE) {
                return error::error_code::need_more_data;
            }
            set_file_position(file_position);
            error e = frame_version(*this);
            if (e) {
                return e;
            }
            e = frame_layer(*this);
            if (e) {
                return e;
            }
            e = frame_bitrate(*this);
            if (e) {
                return e;
            }
            e = frame_samplerate(*this);
            if (e) {
                return e;
            }
            e = frame_emph_copyright_etc(*this);
            if (e) {
                return e;
            }

            const auto lib = length_in_bytes();
            const auto bsz = m_sbo.size_i();
            if (bsz < lib) {
                e = mpeg::error::error_code::data_incomplete;
            }

            return e;
        }

        bool operator!() const noexcept { return !frame_base::valid; }
        operator bool() const noexcept { return frame_base::valid; }
        inline friend bool operator==(const frame& a, const frame& b) noexcept {
            return compare_frames(a, b) == frame_mismatch::none;
        }

        inline friend frame_mismatch compare_frames(
            const frame& x, const frame& y) noexcept {
            const auto& a = x.props_const();
            const auto& b = y.props_const();

            if (a.channelmode != b.channelmode) {
                return frame_mismatch::channelmode;
            }
            if (a.layer != b.layer) {
                return frame_mismatch::layer;
            }
            if (a.version != b.version) {
                return frame_mismatch::version;
            }
            if (a.emphasis != b.emphasis) {
                return frame_mismatch::emphasis;
            }
            if (a.samplerate != b.samplerate) {
                return frame_mismatch::samplerate;
            }
            if (a.bitrate != b.bitrate) {
                return frame_mismatch::bitrate;
            }

            return frame_mismatch::none;
        }

        void vbr_set(bool bvbr) noexcept { frame_base::m_vbr = bvbr; }

        private:
        inline error frame_version(frame& frame) {
            error e;

            const auto& data = frame.header_bytes;
            // frame.props.crc = static_cast<uint8_t>(
            //    static_cast<unsigned char>(data[1] & 0x01) == 0);

            frame.props.crc = {(data[1] & 0x01) == 0};

            static constexpr int MAX_MPEG_VERSIONS = 3;
            unsigned char version_index = (data[1] >> 3) & 0x03;
            static constexpr unsigned char MPEGVersions[MAX_MPEG_VERSIONS] = {3, 2, 1};

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
            const unsigned char layer_index = (frame.header_bytes[1] >> 1) & 0x03;
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
                = (static_cast<unsigned int>(frame.header_bytes[2]) >> 4) & 0x0F;

            static constexpr unsigned char BAD_BITRATE_INDEX = 0;
            if (bitrate_index
                    >= MAX_MPEG_BITRATES - 1 // -1 coz all values @ 16 -1 are OK for
                // us, 16 itself is -1, which is not.
                || bitrate_index == BAD_BITRATE_INDEX) {
                e = error::error_code::bad_mpeg_bitrate;
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
                    default: return error::error_code::bad_mpeg_bitrate;
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

            const unsigned int samplerate_index = (frame.header_bytes[2] >> 2) & 0x03;

            if (samplerate_index == 3) {
                e = error::error_code::bad_mpeg_samplerate;
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
            using namespace my::mpeg::detail;
            error e;
            static constexpr int MAX_MPEG_CHANNEL_MODE = 4;
            const unsigned int cmodeindex = (frame.header_bytes[3] >> 6) & 0x03;

            static constexpr int ChannelMode[MAX_MPEG_CHANNEL_MODE] = {CHANNELS_STEREO,
                CHANNELS_JOINT_STEREO, CHANNELS_DUAL_CHANNEL, CHANNELS_SINGLE_CHANNEL};

            assert(cmodeindex < MAX_MPEG_CHANNEL_MODE);

            if (cmodeindex >= MAX_MPEG_CHANNEL_MODE) {
                e = error::error_code::bad_mpeg_channels;
                return e;
            }
            frame.props.channelmode = CAST(uint8_t, ChannelMode[cmodeindex]);

            auto data = frame.header_bytes;
            // phead->origin = ((static_cast<unsigned char>(data[3]) & 0x04) !=
            // 0);
            frame.props.copyright = (data[3] & 0x08) != 0;
            frame.props.padding = ((data[2] & 0x02) != 0) ? 1 : 0;

            static constexpr int MAX_EMPH = 4;
            const unsigned int emphasis = ((data[3]) & 0x03);

            static constexpr EmphasisEnum emph_lookup[MAX_EMPH]
                = {EmphasisEnum::EMPHASIS_NONE, EmphasisEnum::EMPHASIS_FIFTY_FIFTEEN_MS,
                    EmphasisEnum::EMPHASIS_RESERVED, EmphasisEnum::EMPHASIS_CCIT_J17};
            frame.props.emphasis = emph_lookup[emphasis];
            frame.valid = true;
            return e;
        }
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
            uint32_t total_size() const noexcept { return tagsize_inc_header; }
        };
#pragma pack(pop)

        inline bool id3_is_valid(const id3v2Header& h) noexcept {
            return h.tagsize_inc_header > 10 && (h.tagsize_inc_header < ID3V2_MAX_SIZE);
        }

        uint32_t DecodeSyncSafe(const char* pb) noexcept {
            /// For i As Integer = 0 To UBound(arBytes)
            // usum = usum Or (arBytes(i) And 127) << ((3 - i) * 7)
            // Next
            uint32_t retval = 0;
            if (pb == nullptr) {
                return 0;
            }

            for (uint32_t i = 0; i < 4; i++) {
                retval = retval | CAST(uint32_t, (pb[i] & 127) << ((3 - i) * 7));
            }
            return retval;
        }

        static constexpr size_t NUM_MPEG_HEADERS = 4;
        using frames_t = std::array<frame, NUM_MPEG_HEADERS>;

        /*!
         * returns a pointer to a possible MPEG sync point,
         * or NULL if the buffer is exhausted.
         */
        /*/
        const byte* find_sync(
            const byte* buf, const int buf_position, const size_t bufsz)
        noexcept {

            const auto* const e = (buf + bufsz);
            const auto* p = buf;
            if (!p) return nullptr;

            const auto sz = e - p;
            assert(sz);
            p += buf_position;
            while (e - p >= 4) {
                if (*p == 255 && *(p + 1) >= 224) {
                    return p;
                }
                ++p;
            }
            return nullptr;
        }
        /*/
        using byte = my::io::byte_type;
        const byte* find_sync(const byte* buf, int /*offset*/, int bufsize) {

            auto my_buf = reinterpret_cast<const uint8_t*>(buf);
            auto p = my_buf;
            auto e = p + bufsize;

            while (p < e - 1) {
                auto pval = *p;
                auto ret = p;
                if (pval == 255) {
                    ++p;
                    auto next_p = *p;
                    if (next_p >= 224) {
                        return reinterpret_cast<const byte*>(ret);
                    }
                } else {
                    ++p;
                }
            }

            return nullptr;
        }
        template <typename IO>
        [[maybe_unused]] static error read_io(IO&& io, int& how_much,
            char* const your_buf, my::io::seek_type sk = my::io::seek_type(),
            bool peek = false) {

            assert(your_buf);

            auto& myio = std::forward<IO&>(io);
            error e = myio.get(
                how_much, std::forward<const my::io::seek_type&>(sk), your_buf, peek);
            if (e) {
                fprintf(stderr, "%s %s %s %s %i\n\n",
                    "Error reading io device:", e.to_string().c_str(),
                    "\n For:", io.uri().c_str(), e.to_int());
                if (e != error::error_code::no_more_data) {
                    assert("Error reading iodevice" == nullptr);
                }
            }
            if (your_buf == nullptr) {
                assert("always assumed you wanted to use your own buffer" == nullptr);
                myio.size_set(how_much);
            }

            return e;
        }

        template <typename CB> using _buffer_type = buffer_type<CB>;
        template <typename IO> inline error get_id3v2_tag(IO&& io, id3v2Header& id3) {

            const seek_type sk(0, io::seek_value_type::seek_from_begin);
            int how_much = detail::ID3V2_HEADER_SIZE;
            char* const p = reinterpret_cast<char* const>(&id3);
            id3.tagsize_inc_header = 0;

            error e = my::mpeg::detail::read_io(io, how_much, p, sk);
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

                return error::error_code::noerror;
            }
            return e;
        }

    } // namespace detail

    class myio {
        public:
        myio(std::string path, int& err, size_t read_chunk_size = 8192)
            : m_spath(std::move(path)), m_err(0) {
            m_file = ::fopen(m_spath.c_str(), "r+b");
            if (m_file == nullptr) {
                err = errno;
                m_serr = strerror(errno);
                return;
            }
            m_buffer.resize(read_chunk_size);
        }

        std::vector<char>& buffer() noexcept { return m_buffer; }

        myio(const myio&) = delete;
        myio& operator=(const myio& rhs) = delete;
        myio(myio&& rhs) noexcept : m_err(0) { do_move(std::forward<myio&&>(rhs)); }
        myio& operator=(myio&& rhs) noexcept {
            return do_move(std::forward<myio&&>(rhs));
        }
        ~myio() { close(); }

        char* begin() noexcept { return m_buffer.data(); }
        char* end() noexcept { return m_buffer.data() + m_buffer.size(); }
        void close() noexcept {
            if (m_file != nullptr) {
                const int i = fclose(m_file);
                if (i == 0) {
                    m_file = nullptr;
                    m_spath.clear();
                } else {
                    assert("Unexpected error closing file." == nullptr);
                }
            }
        }

        static bool exists(const std::string&& path) noexcept {
#ifdef _MSC_VER
#ifdef access
#undef access
#endif
#define access _access
#endif

            return ::access(path.c_str(), 0) != -1;
        }

        private:
        FILE* m_file = nullptr;
        std::string m_serr;
        std::string m_spath;
        int m_err;
        std::vector<char> m_buffer;

        myio& do_move(myio&& rhs) noexcept {
            using std::swap;
            swap(m_spath, rhs.m_spath);
            swap(m_file, rhs.m_file);
            swap(m_serr, rhs.m_serr);
            swap(m_err, rhs.m_err);
            swap(m_buffer, rhs.m_buffer);
            return *this;
        }

    }; // namespace mpeg

    /*/
    template <typename io> struct mpeg_iterator {

        struct end_of_data_exception {};
        struct end_of_data_iterator {};
        frame m_tmp_frame;

        mpeg_iterator& operator++() {
            if (io.empty()) {
                int read = 8192;
                int actual = io.read(m_ptr, read);
                if (actual == -1) {
                    throw end_of_data_exception();
                }
                auto e = 0; //
                // parse_single_frame(m_ptr, m_tmp_frame);
                if (e == error::error_code::noerror) {
                    m_ptr += m_tmp_frame.length();
                }
            }
        }

        char* m_ptr = nullptr;

        char* begin() { return m_ptr; }
        char* end() { return end_of_data_iterator{}; }
    };
    /*/
    using seek_t = my::io::seek_type;
    struct io_base : public my::io::buffer_guts_type<io_base> {};

    class parser {

        private:
        detail::frames_t m_frames{}; // frame[NUM_MPEG_HEADERS];

        void copy_frame_overflow(
            const frame* const pprev_frame, frame& cur_frame) noexcept {

            assert(pprev_frame
                && "cannot copy_frame_overflow if there isn't a previous frame!"
                    != nullptr);

            const auto prev_sz = pprev_frame->size_in_bytes();
            const auto copy_len = pprev_frame->m_sbo.size_i() - prev_sz;

            if (copy_len > 0) {
                auto copy_from = pprev_frame->m_sbo.cbegin() + prev_sz;
                auto copy_to = cur_frame.m_sbo.begin();
                cur_frame.m_sbo.resize(cur_frame.m_sbo.size() + CAST(size_t, copy_len));
                memcpy(copy_to, copy_from, CAST(size_t, copy_len));
            }
        }

        error single_frame(frame& cur_frame, frame* pprev_frame,
            my::io::seek_type sk = my::io::seek_type()) noexcept {

            error e;
            (void)sk;
            auto frame_pos = int64_t{0};

            if (pprev_frame != nullptr) {
                copy_frame_overflow(pprev_frame, cur_frame);
                frame_pos = pprev_frame->file_position + pprev_frame->size_in_bytes();
            } else {
                // finding first frame here
                frame_pos = cur_frame.file_position;
            }

            auto& sbo = cur_frame.m_sbo;
            if (sbo.size() == 0) {
                return error::error_code::need_more_data;
            }

            auto* sync = detail::find_sync(sbo.begin(), 0, sbo.size_i());
            if (sync == nullptr) {
                return error::error_code::lost_sync;
            }
            const auto skipped = sync - sbo.data();
            assert(!skipped);

            e = cur_frame.parse_header(frame_pos);
            if (e == error::error_code::data_incomplete
                || e == error::error_code::need_more_data) {
                return e;
            }

            return e;
        }

        template <typename IO> error get_id3v1(IO&& io, detail::ID3V1& v1tag) {

            error e;
            seek_t sk(128, seek_value_type::seek_from_end);
            v1tag = detail::ID3V1();
            int how_much = io::detail::BUFFER_CAPACITY;
            io.clear();

            e = detail::read_io(io, how_much, seek_t::seek_value_type::seek_from_end);
            if (e) return e;

            if (!e && how_much >= 128) {
                auto const v = memcmp(io.begin(), "TAG", 3);
            }
            return e;
        }
        template <typename IO>
        error get_id3(IO&& io, detail::id3v2Header& v2Header, detail::ID3V1& v1Tag) {

            error e = error::error_code::need_more_data;
            memset(&v1Tag, 0, sizeof(v1Tag));
            memset(&v2Header, 0, sizeof(v2Header));
            int how_much = 128;
            const seek_t sk(128, seek_value_type::seek_from_end);

            io.clear();
            e = detail::read_io(io, how_much, reinterpret_cast<char*>(&v1Tag), sk);
            if (e) {
                return e;
            }
            e = detail::get_id3v2_tag(io, v2Header);
            return e;
        }

        static bool id3v1_valid(const my::mpeg::detail::ID3V1& tag) noexcept {
            return memcmp(&tag, "TAG", 3) == 0;
        }

        template <typename IO>
        error find_next_frame(
            IO&& buf, int& buf_pos, frame& prev_frame, frame& next_frame) {

            (void)buf;
            (void)buf_pos;
            error e;
            e = single_frame(next_frame, &prev_frame);

            return e;
        }

        template <typename IO>
        error find_first_frame(IO&& io, int buf_pos, int64_t file_pos, frame& f) {
            const auto sk = my::io::seek_type(file_pos, seek_value_type::seek_from_begin);
            error e = single_frame(io, buf_pos, f, file_pos, sk);
            return e;
        }

        inline void init_frames() noexcept {
            // constexpr auto N = detail::NUM_MPEG_HEADERS;
            for (auto& m_frame : m_frames) {
                m_frame.clear();
            }
        }

        const frame& any_valid_frame() const noexcept {
            for (const auto& f : m_frames) {
                if (f.valid) {
                    return f;
                }
            }
            fprintf(
                stderr, "%s\n", "You asked for any valid frame, but there were none.\n");
            return m_frames[0];
        }

        template <typename IO>
        error frame_load_data(
            IO&& io, frame& f, int how_much, seek_type sk = seek_type{}) {

            int got = how_much;
            auto& sbo = f.m_sbo;
            sbo.resize(sbo.size() + CAST(size_t, how_much));
            auto e = detail::read_io(io, got, f.m_sbo.begin() + f.size_in_bytes(), sk);
            if (e) {
                assert(0);
                return e;
            }
            if (got > 0) {
                if (got < how_much) {
                    e = error::error_code::no_more_data;
                }
            } else {
                e = error::error_code::need_more_data;
            }

            return e;
        }

        // does all the things possible to find the next frame
        template <typename IO> error buf2frame(IO&& io, frame* pprev, frame& fcur) {
            if (pprev) {
                copy_frame_overflow(pprev, fcur);
            }

            auto e = error::error_code::noerror;
            while (e != error::error_code::need_more_data) {
                int how_much = fcur.m_sbo.capacity_i();
                e = frame_load_data(io, fcur, how_much);
            }
        }

        template <typename IO> error find_first_frames(IO&& io) {

            init_frames();
            error e;
            e = get_id3(io, m_id3v2Header, m_id3v1Tag);

            if (e == error::error_code::no_id3v2_tag) {

            } else {
                if (e) {
                    return e;
                }
            }

            e = error::error_code::noerror;
            auto headers_size = id3v1_valid(m_id3v1Tag) ? 128 : 0;
            headers_size += m_id3v2Header.tagsize_inc_header;
            this->m_payload_size = this->file_size - headers_size;
            if (m_payload_size <= mpeg::detail::MIN_MPEG_PAYLOAD) {
                e = error::error_code::tiny_file;
                return e;
            }

            io.clear();
            int64_t file_pos
                = m_id3v2Header.tagsize_inc_header ? m_id3v2Header.tagsize_inc_header : 0;

            size_t cur_frame_idx = 0;
            size_t next_frame_idx = 1;
            int buf_pos = 0;

            int n_confirmed_frames = 0;
            auto sk = my::io::seek_type(file_pos, seek_value_type::seek_from_begin);
            nframes = 0;
            using namespace std;
            int io_pos = 0;

            int prev_frame_index = -1;

            while (true) {

                auto& cur_frame = m_frames[0];
                assert(io_pos == 0);
                cur_frame.set_file_position(file_pos);
                e = frame_load_data(io, cur_frame, cur_frame.m_sbo.capacity_i(), sk);
                if (e) {
                    assert(0);
                    return e;
                }
                e = single_frame(cur_frame, nullptr, sk);

                if (e) {
                    if (e == error::error_code::no_more_data) {
                        return e;
                    }
                    assert(0);
                } else {

                    n_confirmed_frames++;
                    nframes++;

                    if (nframes == 1) {
                        const auto& fm = cur_frame;
                        const auto& props = fm.props_const();
                        cout << "reckon first frame is @ " << fm.file_position << endl
                             << props.bitrate << " kbps" << endl
                             << "version:    " << CAST(int, props.version) << endl
                             << "layer:      " << CAST(int, props.layer) << endl
                             << "samplerate: " << props.samplerate << endl
                             << "padding:    " << props.padding << endl
                             << "total size: " << fm.size_in_bytes() << endl
                             << "[next frame expected @ file position: "
                             << fm.file_position + fm.size_in_bytes() << "]" << endl;
                        cout << "file size is: " << this->file_size << endl;
                    }

                    buf_pos += cur_frame.length_in_bytes();
                    const auto capa = cur_frame.m_sbo.capacity_i();
                    const auto fsz = cur_frame.size_in_bytes();
                    const auto remain_in_buffer = capa - fsz;
                    if (remain_in_buffer < 0) {
                        assert(0);
                    }

                // ---------------------------------
                the_next_frame:
                    io.clear();
                    m_frames[next_frame_idx].clear();
                    frame* pcur_frame = &m_frames[cur_frame_idx];
                    e = find_next_frame(
                        io, buf_pos, *pcur_frame, m_frames[next_frame_idx]);

                    if (e == error::error_code::need_more_data
                        || e == error::error_code::data_incomplete) {
                        const auto stray_size
                            = m_frames[next_frame_idx].m_sbo.size_i() - buf_pos;
                        assert(stray_size > 0
                            && "stray size must be positive, or zero!" == nullptr);
                        const auto fp = m_frames[next_frame_idx].file_position;
                        assert(stray_size);
                        int my_io_pos = buf_pos; //
                        //
                        if (prev_frame_index >= 0) {
                            e = find_next_frame(io, my_io_pos, m_frames[prev_frame_index],
                                m_frames[next_frame_idx]);
                        } else {
                            e = single_frame(*pcur_frame,
                                &m_frames[CAST(size_t, prev_frame_index)], fp);
                        }

                        if (e != error::error_code::no_more_data) {
                            assert(e == error::error_code::data_incomplete);
                        }
                    }
                    if (e != error::error_code::no_more_data) {
                        assert(
                            e.to_int() == 0 || e == error::error_code::data_incomplete);
                    }
                    if (e == error::error_code::noerror) {

                        const auto baddun = compare_frames(
                            m_frames[cur_frame_idx], m_frames[next_frame_idx]);
                        if (baddun != frame_mismatch::none) {
                            if (baddun == frame_mismatch::bitrate) {
                                m_frames[cur_frame_idx].vbr_set(true);
                                m_frames[0].vbr_set(true);
                            }

                            fprintf(
                                stderr, "%s\n", frame_mismatch_string(baddun).c_str());
                            return error(baddun);
                        }

                        const auto prev_start_where = file_pos;
                        const auto& this_frame = m_frames[next_frame_idx];
                        file_pos = this_frame.file_position;
                        const auto fl = this_frame.length_in_bytes();
                        file_pos += fl;
                        if (this_frame.m_sbo.size_i() > fl) {
                            io_pos = fl;
                        } else {
                            io_pos = 0;
                        }

                        if (detail::loglevel >= detail::loglevel_t::all) {
                            printf("MPEG header %lu @ file position %lu has "
                                   "size of: %lu\n",
                                static_cast<unsigned long>(nframes),
                                static_cast<unsigned long>(this_frame.file_position),
                                static_cast<unsigned long>(this_frame.length_in_bytes()));
                        }
                        assert(file_pos > prev_start_where);

                        cur_frame_idx++;

                        if (cur_frame_idx >= detail::NUM_MPEG_HEADERS) {
                            cur_frame_idx = 0;
                        }
                        prev_frame_index = CAST(int, cur_frame_idx);

                        next_frame_idx = cur_frame_idx + 1;
                        if (next_frame_idx >= detail::NUM_MPEG_HEADERS) {
                            next_frame_idx = 0;
                        }
                        assert(next_frame_idx != cur_frame_idx);
                        n_confirmed_frames++;
                        nframes++;
                        buf_pos = this_frame.size_in_bytes();
                        goto the_next_frame;
                    } else {
                        if (e != error::error_code::no_more_data) {
                            if (e == error::error_code::data_incomplete) {
                                // might be a truncated frame, if there is no
                                // more data:
                                int x = 0;
                                (void)x;
                                std::cout << x;

                            } else {

                                assert("frame parser error that is NOT "
                                       "no_more_data!"
                                    == nullptr);
                            }
                        }
                        return e;
                    }
                }
            }

            return e;
        }
        // using buffer_t = buffer_type<IO>;
        uint32_t nframes{0};
        int64_t file_size{-1};
        std::string filepath;
        my::mpeg::error err;
        detail::id3v2Header m_id3v2Header;
        detail::ID3V1 m_id3v1Tag;
        int64_t m_payload_size = 0;

        // IO& m_buf;

        parser(int dum, string_view file_path, uintmax_t file_size)
            : file_size(CAST(int64_t, file_size)), filepath(file_path) {
            // puts("parser private construct");
            (void)dum;
        }

        public:
        parser(const parser&) = delete;
        parser& operator=(const parser&) = delete;
        using seek_value_type = my::io::seek_value_type;

        parser(string_view file_path, uintmax_t file_size)
            : parser(0, file_path, file_size) {}

        template <typename IO> mpeg::error parse(IO&& myio) {
            using namespace std;
            static auto ctr = 0;

            std::cout << endl;
            std::cout << "-----------------------------------------" << endl;

            std::cout << "Parsing: " << this->filepath << endl;
            cout << "Files parsed so far: " << ctr++ << endl;
            mpeg::error e;

            const auto myctr = ctr;
            (void)myctr;
            CAST(void, ctr);
            e = find_first_frames(myio);
            if (e) {
                if (e == error::error_code::no_more_data
                    || e == error::error_code::data_incomplete) {
                    e = error::error_code::noerror;

                } else {
                    assert("unexpected find_first_frames error" == nullptr);
                }
            }

            if (nframes) {
                const auto ll = detail::loglevel;
                if (ll >= detail::loglevel_t::all) {
                    puts(myio.uri().c_str());
                    printf("nFrames = %lu\n", CAST(unsigned long, nframes));
                    const auto& f = any_valid_frame();
                    printf("single frame dur in ms = %d\n", f.frame_dur_in_ms());
                    auto dur_ms = nframes * CAST(unsigned long, f.frame_dur_in_ms());
                    const int chans = detail::nchannels(
                        CAST(my::mpeg::detail::ChannelEnum, f.props_const().channelmode));
                    assert(
                        chans > 0 && chans <= 2 && "bad/weird num channels" != nullptr);
                    dur_ms *= CAST(unsigned int, chans);
                    printf("Dur: %f seconds.\n", CAST(double, dur_ms) / 1000.0);
                    assert(dur_ms > 10);
                }
            }
            assert(e == error::error_code::noerror);
            return e;
        }
    };

} // namespace mpeg
} // namespace my
