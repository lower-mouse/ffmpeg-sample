
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __STDC_CONSTANT_MACROS 
extern "C"{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "encoder.h"

using namespace Ifai;
AVFrame* cvmatToAvframe(cv::Mat* image, AVFrame * frame){  
    int width = image->cols;  
    int height = image->rows;  
    int cvLinesizes[1];  
    cvLinesizes[0] = image->step1();  
    if (frame == NULL){  
        frame = av_frame_alloc();  
        av_image_alloc(frame->data, frame->linesize, width, height, AVPixelFormat::AV_PIX_FMT_YUV420P, 1);  
    }  
    SwsContext* conversion = sws_getContext(width, height, AVPixelFormat::AV_PIX_FMT_BGR24, width, height, (AVPixelFormat) frame->format, SWS_FAST_BILINEAR, NULL, NULL, NULL);  
    sws_scale(conversion, &image->data, cvLinesizes , 0, height, frame->data, frame->linesize);  
    sws_freeContext(conversion);  
    return  frame;  
}

void Encoder::encode(cv::Mat& mat){
    frame = cvmatToAvframe(&mat, frame);
    frame->pts = pts++;
    encode(c, frame, pkt, f);
}

void Encoder::encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *outfile)
{
    int ret;
    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3"PRId64"\n", frame->pts);
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, " Error sending a frame for encoding\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, " Error during encoding\n");
            exit(1);
        }
        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

Encoder::Encoder(){

}

Encoder::~Encoder(){

}

int Encoder::init(const std::string& file_name, int width, int height, int frame_rate){
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec h.264 not found\n");
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width  = width;
    c->height = height;
    /* frames per second */
    c->time_base = (AVRational){1, frame_rate};
    c->framerate = (AVRational){frame_rate, 1};
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
* then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);
    /* open it */
    int ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    f = fopen(file_name.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", file_name.c_str());
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame-> format  = c->pix_fmt;
    frame-> width   = c-> width ;
    frame->height = c->height;
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
}

void Encoder::uninit(){
    /* flush the encoder */

    if(f){
        encode(c, NULL, pkt, f);
        fclose(f);
        f = NULL;
    }

    if(c)
        avcodec_free_context(&c);
    if(frame)
        av_frame_free(&frame);
    if(pkt)
        av_packet_free(&pkt);    
}
