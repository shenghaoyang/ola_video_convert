#ifndef MEDIA_HPP_INCLUDED
#define MEDIA_HPP_INCLUDED

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <io.hpp>

#include <cstdint>
#include <type_traits>
#include <memory>

namespace olavc {
namespace DMXVideoEncoder {

template<typename T, std::add_pointer_t<void(T*)> Fn>
struct CFnDeleterP {
    void operator()(T *p) const noexcept {
        Fn(p);
    }
};

template<typename T, std::add_pointer_t<void(T**)> Fn>
struct CFnDeleterPP {
    void operator()(T *p) const noexcept {
        Fn(&p);
    }
};

template<typename T, std::add_pointer_t<void(T*)> Fn>
using UniqueCDeleterPtr = std::unique_ptr<T, CFnDeleterP<T, Fn>>;


template<typename T, std::add_pointer_t<void(T**)> Fn>
using UniqueCDeleterPPtr = std::unique_ptr<T, CFnDeleterPP<T, Fn>>;


using UniqueAVCodecContext = 
    UniqueCDeleterPPtr<AVCodecContext, avcodec_free_context>;

using UniqueAVIOContext =
    UniqueCDeleterPPtr<AVIOContext, avio_context_free>;

using UniqueAVFormatContext = 
    UniqueCDeleterPtr<AVFormatContext, avformat_free_context>;

using UniqueAVFrame = 
    UniqueCDeleterPPtr<AVFrame, av_frame_free>;

struct stream_frame {
    AVStream *s;
    UniqueAVFrame f;
};

class DMXVideoEncoder {
private:
    UniqueAVCodecContext enc_ctx;
    UniqueAVFormatContext fmt_ctx;
    UniqueAVIOContext io_ctx;
    UniqueAVFrame fbuf;
    AVStream *s;
    bool closed{false};
    std::uint64_t next_pts{0};

    void ensure_not_closed();
    void write_frame(bool flush=false);
public:
    DMXVideoEncoder(int universes, const std::string& path);
    DMXVideoEncoder(DMXVideoEncoder& enc) = delete;
    DMXVideoEncoder(DMXVideoEncoder&& enc) = delete;
    DMXVideoEncoder& operator=(DMXVideoEncoder& enc) = delete;
    DMXVideoEncoder& operator=(DMXVideoEncoder&& enc) = delete;
    ~DMXVideoEncoder();

    void write_universe(const io::UniverseStates& sts, uint64_t duration);
    void close();
};
}
}

#endif
