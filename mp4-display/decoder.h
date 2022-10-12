#ifndef _STREAM_DECODE_H_
#define _STREAM_DECODE_H_

#include <string>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}
#include <opencv4/opencv2/core/core.hpp>
#include <opencv4/opencv2/opencv.hpp>
#include "mp4_reader/Mp4ReaderInterface.h"

namespace Ifai
{
    class Decoder
    {
    public:
        Decoder()
        {
            fmt_ctx = NULL;
            video_dec_ctx = NULL;
            video_stream = NULL;
            frame = NULL;
            pkt = NULL;
            video_stream_idx = -1;
        }
        ~Decoder() = default;

        int init(std::string file);
        int init(Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info mp4_info);
        int uninit();

        int push(Ifai::Ifmp4::Mp4ReaderInterface::Frame& src_packet, cv::Mat& mat);
        int push(cv::Mat& mat);

    private:
        int decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame* frame);

        int open_codec_context(int *stream_idx,
                                        AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);
        AVFormatContext *fmt_ctx;
        AVCodecContext *video_dec_ctx;
        AVStream *video_stream;
        AVFrame *frame;
        AVPacket *pkt;
        int video_stream_idx;
        int width, height;
        enum AVPixelFormat pix_fmt;        
        int bgr_buffer_size;
        unsigned char* bgr_buffer_linear;
        char err_buf[256];
    };
}

#endif