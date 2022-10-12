#include <memory>
#include <stdio.h>
#include "mp4_reader/Mp4ReaderInterface.h"
#include "decoder.h"
#include <string>
#include <thread>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <opencv4/opencv2/core/core.hpp>
#include <opencv4/opencv2/highgui.hpp>
#include <opencv4/opencv2/opencv.hpp>
std::shared_ptr<Ifai::Ifmp4::Mp4ReaderInterface> reader_;
std::shared_ptr<Ifai::Decoder> decode_;
FILE* fp_ = NULL;

int once = 1;

using namespace Ifai;
using namespace Ifmp4;

bool writeFile(void* data, unsigned lenght, std::string file_name){
    FILE* fp = fopen(file_name.c_str(), "w+");
    if (fp && data && lenght)
    {

        auto write_len = fwrite(data, 1, lenght, fp);
        if (write_len != lenght)
        {
            printf("fwrite failed, lenght:%u ret:%d", lenght, write_len);
        }

        fclose(fp);
        fp = NULL;
    }

    return true;
}

bool proc()
{
    static int mp4v2_frame = 0;
    Mp4ReaderInterface::Frame frame;
    auto ret = reader_->ReadFrame(frame);
    if (ret != Mp4ReaderInterface::kOk)
    {
        printf("mp4 read frame failed, ret:%d\n", ret); 
        return false;
    }

    if(!mp4v2_frame){
        writeFile(frame.frame_data, frame.frame_data_len, "mp4v2_frame");
        mp4v2_frame = 1;
    }

    if (frame.type == FrameType::video)
    {
        if(frame.frame_data_len == 6){
            return true;
        }
        if(decode_){
            cv::Mat mat;
            if(decode_->push(frame, mat) != 0){
                printf("decode failed\n");
                return false;
            }

            if(mat.data)
            {
                cv::rectangle(mat, cv::Point(100, 150), cv::Point(250, 300), cv::Scalar(0, 0, 255),4);
                cv::imshow("window", mat);
                cv::waitKey(30);
            }
        }
    }
    else
    {
        std::string str(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        printf("%s\n", str.c_str());
    }

    return true;
}

int main(int argc, char** argv)
{
    if(argc != 2){
        printf("usage: input mp4 file\n");
        exit(1);
    }

    cv::namedWindow("window", 1);
    cv::imshow("window", 0);
    reader_ = Mp4ReaderInterface::CreateNew();
    decode_ = std::make_shared<Decoder>();
    std::string path = argv[1];
    Mp4ReaderInterface::ERRNO ret = reader_->Open(path);
    if (ret != Mp4ReaderInterface::kOk)
    {
        std::cout << "mp4 open failed, ret:" << ret << std::endl;
        return -1;
    }

    decode_->init(reader_->GetMp4Info());

    std::string outfile = "out.raw";
    fp_ = fopen(outfile.c_str(), "w+");
    if (fp_ == NULL)
    {
        printf("open output file:%s failed", outfile.c_str());
    }

    new std::thread([]() -> bool
                    {
    while(1){
      auto ret = proc();
      if(!ret){
        if(fp_){
          fclose(fp_);
          fp_ = NULL;
        }

        exit(0);
      }
    } });

    while(1){
        sleep(1);
    }
}

// extern "C"
// {
// #include "FFmpegVideoDecoder.h"
// }
// int buffer_size;
// unsigned char* buffer_linear;
// bool proc1()
// {
//     static int mp4v2_frame = 0;
//     unsigned char* data = NULL;
//     unsigned int data_size = 0;
//     Mp4ReaderInterface::Frame frame;
//     auto ret = reader_->ReadFrame(frame);
//     if (ret != Mp4ReaderInterface::kOk)
//     {
//         printf("mp4 read frame failed, ret:%d\n", ret); 
//         return false;
//     }

//     if (frame.type == FrameType::video)
//     {
//         int framePara[6];
//         cv::Mat mat;
//         mat.create(cv::Size(846, 486), CV_8UC3);
//         FFmpeg_H264Decode((unsigned char*)frame.frame_data, frame.frame_data_len, framePara, mat.data, NULL);
//         if(framePara[0]){
//             printf("show \n");
//             cv::imshow("window", mat);
//             cv::waitKey(30);
//         }
//     }
//     else
//     {
//         std::string str(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
//         printf("%s\n", str.c_str());
//     }

//     return true;
// }

// int main1(int argc, char** argv)
// {
//     if(argc != 2){
//         printf("usage: input mp4 file\n");
//         exit(1);
//     }
//     av_register_all();
//     cv::namedWindow("window", 1);
//     cv::imshow("window", 0);
//     reader_ = Mp4ReaderInterface::CreateNew();
//     std::string path = argv[1];
//     Mp4ReaderInterface::ERRNO ret = reader_->Open(path);
//     if (ret != Mp4ReaderInterface::kOk)
//     {
//         std::cout << "mp4 open failed, ret:" << ret << std::endl;
//         return -1;
//     }

//     if(0){
//         AVFormatContext* fmt_ctx;
//         if (avformat_open_input(&fmt_ctx, path.c_str(), NULL, NULL) < 0) {
//             fprintf(stderr, "Could not open source file %s\n", path.c_str());
//             return -1;
//         }

//         /* retrieve stream information */
//         if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
//             fprintf(stderr, "Could not find stream information\n");
//             return -1;
//         }  

//         int index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

//         printf("codecParameters codec_type:%d\n", fmt_ctx->streams[index]->codecpar->codec_type);
//         printf("codecParameters codec_id:%d\n", fmt_ctx->streams[index]->codecpar->codec_id);
//         printf("codecParameters bit_rate:%lu\n", fmt_ctx->streams[index]->codecpar->bit_rate);
//         // printf("codecParameters \n", codecParameters->time_base);
//         printf("codecParameters width:%d height:%d\n", fmt_ctx->streams[index]->codecpar->width, fmt_ctx->streams[index]->codecpar->height);
//         // printf("codecParameters \n", codecParameters->refs);
//         printf("codecParameters profile:%d\n", fmt_ctx->streams[index]->codecpar->profile);
//         printf("codecParameters level:%d\n", fmt_ctx->streams[index]->codecpar->level);
//         printf("codecParameters extradata_size:%d\n", fmt_ctx->streams[index]->codecpar->extradata_size);
//         printf("extradata: ");
//         for(int i = 0; i < fmt_ctx->streams[index]->codecpar->extradata_size; i++){
//             printf("0x%02x,", fmt_ctx->streams[index]->codecpar->extradata[i]);
//         }
//         printf("\n");
//         FFmpeg_VideoDecoderInit(fmt_ctx->streams[index]->codecpar);


//     }else{
//         FFmpeg_H264DecoderInit();
//         exit(1);
//     }
//     new std::thread([]() -> bool
//                     {
//     while(1){
//       auto ret = proc1();
//       if(!ret){
//         if(fp_){
//           fclose(fp_);
//           fp_ = NULL;
//         }

//         exit(0);
//       }
//     } });

//     while(1){
//         sleep(1);
//     }
// }