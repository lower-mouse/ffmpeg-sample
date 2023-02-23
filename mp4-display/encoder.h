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
#include <stdio.h>
namespace Ifai
{
    class Encoder
    {
    public:
        Encoder();
        Encoder(const Encoder&) = delete;
        Encoder& operator=(const Encoder&)= delete;
        ~Encoder();

        int init(const std::string& file_name, int width, int height, int frame_rate);
        void uninit();
        void encode(cv::Mat& mat);
    private:
        void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile);
        const AVCodec *codec = NULL;
        AVCodecContext *c = NULL;
        AVFrame *frame = NULL;
        AVPacket *pkt = NULL;
        FILE* f = NULL;
        int pts = 0;
    };
}