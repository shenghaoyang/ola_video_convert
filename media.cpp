extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <io.hpp>
#include <media.hpp>
#include <stdexcept>

namespace olavc {
namespace DMXVideoEncoder {
static constexpr const auto image_format{AV_PIX_FMT_GRAY8};
static constexpr const AVRational millisecond{1, 1000};

static bool operator!=(const AVRational &r1, const AVRational &r2) {
  return (r1.num != r2.num) || (r1.den != r2.den);
}

static UniqueAVCodecContext init_ffv1_context(int universes) {
  auto *ffv1 = avcodec_find_encoder_by_name("ffv1");
  if (!ffv1) throw std::runtime_error{"finding FFV1 encoder"};

  auto *ffv1_ctx = avcodec_alloc_context3(nullptr);
  if (!ffv1_ctx) throw std::runtime_error{"allocating encoder context"};

  AVDictionary *options{nullptr};
  try {
    if (av_dict_set(&options, "g", "1", 0) < 0)
      throw std::runtime_error{"setting encoder options"};

    if (av_dict_set(&options, "slicecrc", "0", 0) < 0)
      throw std::runtime_error{"setting encoder options"};

    ffv1_ctx->framerate = AVRational{0, 1};
    ffv1_ctx->pix_fmt = image_format;
    ffv1_ctx->time_base = millisecond;
    ffv1_ctx->width = io::frame_width;
    ffv1_ctx->height = universes;
    ffv1_ctx->sample_aspect_ratio = AVRational{1, 1};

    if (avcodec_open2(ffv1_ctx, ffv1, &options) < 0)
      throw std::runtime_error{"could not open encoder"};
  } catch (const std::exception &e) {
    av_dict_free(&options);
    throw e;
  }
  av_dict_free(&options);

  return UniqueAVCodecContext{ffv1_ctx};
}

static UniqueAVFormatContext init_mkv_context() {
  auto fmt = av_guess_format("matroska", nullptr, nullptr);
  if (!fmt) throw std::runtime_error{"finding MKV muxer"};

  AVFormatContext *ctx;
  if (avformat_alloc_output_context2(&ctx, fmt, nullptr, nullptr) < 0)
    throw std::runtime_error{"allocating MKV context"};

  return UniqueAVFormatContext{ctx};
}

static UniqueAVIOContext init_output_context(const std::string &path) {
  AVIOContext *ctx;
  if (avio_open2(&ctx, path.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
    throw std::runtime_error{"allocating output context"};

  return UniqueAVIOContext{ctx};
}

static UniqueAVFrame init_frame(AVCodecContext *cctx) {
  auto v_frame{av_frame_alloc()};
  if (!v_frame) throw std::runtime_error{"allocating frame"};

  v_frame->format = cctx->pix_fmt;
  v_frame->width = cctx->width;
  v_frame->height = cctx->height;
  v_frame->sample_aspect_ratio = cctx->sample_aspect_ratio;

  if (av_frame_get_buffer(v_frame, 0) < 0)
    throw std::runtime_error{"allocating frame buffer"};

  return UniqueAVFrame{v_frame};
}

DMXVideoEncoder::DMXVideoEncoder(int universes, const std::string &path)
    : enc_ctx{init_ffv1_context(universes)},
      fmt_ctx{init_mkv_context()},
      io_ctx{init_output_context(path)},
      fbuf{init_frame(enc_ctx.get())} {
  fmt_ctx->pb = io_ctx.get();

  s = avformat_new_stream(fmt_ctx.get(), enc_ctx->codec);
  if (!s) throw std::runtime_error{"allocating stream for muxer"};
  if (avcodec_parameters_from_context(s->codecpar, enc_ctx.get()) < 0)
    throw std::runtime_error{"setting stream codec parameters"};
  s->time_base = enc_ctx->time_base;

  if (avformat_write_header(fmt_ctx.get(), nullptr) < 0)
    throw std::runtime_error{"writing MKV header"};
  if (s->time_base != enc_ctx->time_base)
    throw std::runtime_error{"using millisecond time base for stream"};
}

DMXVideoEncoder::~DMXVideoEncoder() { close(); }

void DMXVideoEncoder::ensure_not_closed() {
  if (closed) throw std::logic_error{"closed"};
}

void DMXVideoEncoder::write_frame(bool flush) {
  if (avcodec_send_frame(enc_ctx.get(), flush ? nullptr : fbuf.get()) < 0)
    throw std::runtime_error{"sending to encoder"};

  AVPacket pkt{};
  int ret;
  while (!(ret = avcodec_receive_packet(enc_ctx.get(), &pkt))) {
    pkt.stream_index = s->index;
    if (flush) pkt.dts = pkt.pts = next_pts;

    if (av_interleaved_write_frame(fmt_ctx.get(), &pkt) < 0)
      throw std::runtime_error{"write packet to muxer"};
    av_packet_unref(&pkt);
  }

  if (ret != (flush ? AVERROR_EOF : AVERROR(EAGAIN)))
    throw std::runtime_error{"receive packet from encoder"};
}

void DMXVideoEncoder::write_universe(const io::UniverseStates &sts,
                                     std::uint64_t duration) {
  ensure_not_closed();

  // Copy frame data if encoder is still referencing it.
  if (av_frame_make_writable(fbuf.get()) < 0)
    throw std::runtime_error{"write to allocated frame"};

  io::write_lines(fbuf->data[0], fbuf->linesize[0], sts);
  fbuf->pts = next_pts;

  write_frame();

  next_pts += duration;
}

void DMXVideoEncoder::close() {
  if (closed) return;

  closed = true;

  write_frame(true);

  if (av_write_trailer(fmt_ctx.get()))
    throw std::runtime_error{"writing trailer"};

  if (avio_close(io_ctx.release())) {
    throw std::runtime_error{"closing output"};
  }
}
}  // namespace DMXVideoEncoder
}  // namespace olavc
