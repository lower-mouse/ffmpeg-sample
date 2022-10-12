/*
 * Copyright (c) 2012 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * Demuxing and decoding example.
 *
 * Show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data.
 * @example demuxing_decoding.c
 */
#define __STDC_CONSTANT_MACROS 
extern "C"{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

}

#include "decoder.h"

using namespace Ifai;
bool writeFile(void* data, unsigned lenght, std::string file_name);

cv::Mat AVFrameToCVMat(AVFrame *yuv420Frame)
{
	//得到AVFrame信息
    int srcW = yuv420Frame->width;
    int srcH = yuv420Frame->height;
    SwsContext *swsCtx = sws_getContext(srcW, srcH, AV_PIX_FMT_YUV420P, srcW, srcH, (AVPixelFormat)AV_PIX_FMT_BGR24, 0, NULL, NULL, NULL);//SWS_BICUBIC

	//生成Mat对象
    cv::Mat mat;
    mat.create(cv::Size(srcW, srcH), CV_8UC3);

	//格式转换，直接填充Mat的数据data
    AVFrame *bgr24Frame = av_frame_alloc();
    av_image_fill_arrays(bgr24Frame->data, bgr24Frame->linesize, (uint8_t *)mat.data, (AVPixelFormat)AV_PIX_FMT_BGR24, srcW, srcH, 1);
    sws_scale(swsCtx,(const uint8_t* const*)yuv420Frame->data, yuv420Frame->linesize, 0, srcH, bgr24Frame->data, bgr24Frame->linesize);

	//释放
    av_free(bgr24Frame);
    sws_freeContext(swsCtx);

    return mat;
}

int Decoder::push(Ifai::Ifmp4::Mp4ReaderInterface::Frame& src_packet, cv::Mat& mat){
    pkt->data = (uint8_t*)src_packet.frame_data;
    pkt->size = src_packet.frame_data_len;
    pkt->flags = src_packet.sync_frame;
    pkt->dts = src_packet.dts;
    pkt->duration = src_packet.duration;
    pkt->pts = src_packet.pts;
    pkt->stream_index = video_stream_idx;

    printf("packet stream_index:%d\n", pkt->stream_index);
    printf("packet size:%d\n", pkt->size);
    printf("packet flags:%d\n", pkt->flags);
    printf("packet duration:%lu\n", pkt->duration);
    printf("packet pos:%lu\n", pkt->pos);
    printf("packet pts:%lu\n", pkt->pts);
    printf("packet dts:%lu\n", pkt->dts);       
    int ret = decode_packet(video_dec_ctx, pkt, frame);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)){
        return 0;
    }
    
    if (ret < 0){
        printf("decode packed faied\n");
        return -1;
    }    

    if(frame->linesize[0]){
        mat = AVFrameToCVMat(frame);
        av_frame_unref(frame);
    }
    return 0;
}

int Decoder::push(cv::Mat& mat){
    static int ffmpeg_frame = 0;
    int ret = 0;
    if (av_read_frame(fmt_ctx, pkt) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise
        // skip it
        if (pkt->stream_index == video_stream_idx){
            printf("packet stream_index:%d\n", pkt->stream_index);
            printf("packet size:%d\n", pkt->size);
            printf("packet flags:%d\n", pkt->flags);
            printf("packet duration:%lu\n", pkt->duration);
            printf("packet pos:%lu\n", pkt->pos);
            printf("packet pts:%lu\n", pkt->pts);
            printf("packet dts:%lu\n", pkt->dts);    
            if(!ffmpeg_frame){
                writeFile(pkt->data, pkt->size, "ffmpeg_frame");
                ffmpeg_frame = 1;
            }                    
            ret = decode_packet(video_dec_ctx, pkt, frame);
        }

        av_packet_unref(pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)){
            return 0;
        }

        if (ret < 0){
            printf("decode packed faied\n");
            return -1;
        }

        if(frame->linesize[0]){
            mat = AVFrameToCVMat(frame);
            av_frame_unref(frame);
        }
    }else{
        printf("read frame failed\n");
        return -1;
    }

    return 0;
}

int Decoder::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame* frame)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", err_buf);
        return ret;
    }

    // get all the available frames from the decoder
    if (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)){
                return ret;
            }

            av_strerror(ret, err_buf, sizeof(err_buf));
            fprintf(stderr, "Error during decoding (%s)\n", err_buf);
            return ret;
        }

        if (ret < 0)
            return ret;
    }

    return 0;
}

int Decoder::open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream \n",
                av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

int mp4info_to_avcode_parameter(Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info info, AVCodecParameters *codecParameters){
    unsigned char spspps[] = {0x01,0x64,0x00,0x1e,0xff,0xe1,0x00,0x19,0x67,0x64,0x00,0x1e,0xac,0xc8,0x50,0x35,0x0f,0xfa,0x9a,0x68,0x80,0x00,0x00,0x03,0x00,0x80,0x00,0x00,0x12,0x07,0x8b,0x16,0xcb,0x01,0x00,0x05,0x68,0xeb,0xec,0xf2,0x3c,0xfd,0xf8,0xf8,0x00};
    codecParameters->width = info->width;
    codecParameters->height = info->height;
    codecParameters->profile = info->profile;
    codecParameters->level = info->level;

    int extradata_len = 8 + info->sps.size() + 1 + 2 + info->sps.size();
    codecParameters->extradata = (uint8_t*)av_mallocz(extradata_len);
    codecParameters->extradata_size = extradata_len;
    codecParameters->extradata[0] = 0x01;
    codecParameters->extradata[1] = info->sps[1];
    codecParameters->extradata[2] = info->sps[2];
    codecParameters->extradata[3] = info->sps[3];
    codecParameters->extradata[4] = 0xFC | 3;
    codecParameters->extradata[5] = 0xE0 | 1;
    int tmp = info->sps.size();
    codecParameters->extradata[6] = (tmp >> 8) & 0x00ff;
    codecParameters->extradata[7] = tmp & 0x00ff;
    int i = 0;
    for (i=0;i<tmp;i++)
        codecParameters->extradata[8+i] = info->sps[i];
    codecParameters->extradata[8+tmp] = 0x01;
    int tmp2 = info->pps.size();   
    codecParameters->extradata[8+tmp+1] = (tmp2 >> 8) & 0x00ff;
    codecParameters->extradata[8+tmp+2] = tmp2 & 0x00ff;
    for (i=0;i<tmp2;i++)
        codecParameters->extradata[8+tmp+3+i] = info->pps[4+i];    
}

int Decoder::init(Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info mp4_info){
    av_register_all();
    if(!mp4_info){
        printf("mp4 info is null\n");
        return -1;
    }

    AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!decoder) {
        printf("Can not find codec:%d\n", AV_CODEC_ID_H264);
        return -1;
    }
    
    AVCodecParameters *codec_parameters = avcodec_parameters_alloc();
    video_dec_ctx = avcodec_alloc_context3(decoder);
    if (!video_dec_ctx) {
        printf("Failed to alloc codec context.");
        goto err_;
    }
    
    if(avcodec_parameters_from_context(codec_parameters, video_dec_ctx) < 0){
        printf("Failed to copy codec context  to avcodec parameters.");
        goto err_;
    }

    mp4info_to_avcode_parameter(mp4_info, codec_parameters);
    if (avcodec_parameters_to_context(video_dec_ctx, codec_parameters) < 0) {
        printf("Failed to copy avcodec parameters to codec context.");
        goto err_;
    }
    
    if (avcodec_open2(video_dec_ctx, decoder, NULL) < 0){
        printf("Failed to open h264 decoder");
        goto err_;
    }

    width = video_dec_ctx->width;
    height = video_dec_ctx->height;    
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        goto err_;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        goto err_;
    }    
    avcodec_parameters_free(&codec_parameters);
    return 0;

err_:
    avcodec_parameters_free(&codec_parameters);
    uninit();
    return -1;
}

int Decoder::init(std::string file){
    int ret = 0;
    av_register_all();
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, file.c_str(), NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", file.c_str());
        return -1;
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }  

    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        bgr_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
        bgr_buffer_linear = (unsigned char*)malloc(bgr_buffer_size);

        // ret = av_image_alloc(video_dst_data, video_dst_linesize,
        //                      width, height, pix_fmt, 1);
        // if (ret < 0) {
        //     fprintf(stderr, "Could not allocate raw video buffer\n");
        //     goto end;
        // }
        // video_dst_bufsize = ret;
    }     

    if (!video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    return ret;
end:
    uninit();
    return ret;
}

int Decoder::uninit(){
    if(video_dec_ctx){
        avcodec_free_context(&video_dec_ctx);
        video_dec_ctx = NULL;
    }

    if(fmt_ctx){
        avformat_close_input(&fmt_ctx);
        fmt_ctx = NULL;
    }

    if(pkt){
        av_packet_free(&pkt);
        pkt = NULL;
    }

    if(frame){
        av_frame_free(&frame);
        frame = NULL;
    }
    // av_free(video_dst_data[0]);    
}
