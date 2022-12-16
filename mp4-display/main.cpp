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
#include <jsoncpp/json/json.h>

#define CV_COLOR_RED cv::Scalar(0,0,255)       //纯红
#define CV_COLOR_GREEN cv::Scalar(0,255,0)        //纯绿
#define CV_COLOR_BLUE cv::Scalar(255,0,0)       //纯蓝
#define CV_COLOR_DARKGRAY cv::Scalar(169,169,169) //深灰色
#define CV_COLOR_DARKRED cv::Scalar(0,0,139) //深红色
#define CV_COLOR_ORANGERED cv::Scalar(0,69,255)     //橙红色
#define CV_COLOR_CHOCOLATE cv::Scalar(30,105,210) //巧克力
#define CV_COLOR_GOLD cv::Scalar(10,215,255) //金色
#define CV_COLOR_YELLOW cv::Scalar(0,255,255)     //纯黄色
#define CV_COLOR_OLIVE cv::Scalar(0,128,128) //橄榄色
#define CV_COLOR_LIGHTGREEN cv::Scalar(144,238,144) //浅绿色
#define CV_COLOR_DARKCYAN cv::Scalar(139,139,0)     //深青色
#define CV_COLOR_SKYBLUE cv::Scalar(230,216,173) //天蓝色
#define CV_COLOR_INDIGO cv::Scalar(130,0,75) //藏青色
#define CV_COLOR_PURPLE cv::Scalar(128,0,128)     //紫色
#define CV_COLOR_PINK cv::Scalar(203,192,255) //粉色
#define CV_COLOR_DEEPPINK cv::Scalar(147,20,255) //深粉色
#define CV_COLOR_VIOLET cv::Scalar(238,130,238)     //紫罗兰

const int color_count = 18;
cv::Scalar color_array[color_count] = {CV_COLOR_RED, CV_COLOR_GREEN, CV_COLOR_BLUE, CV_COLOR_DARKGRAY, CV_COLOR_DARKRED, CV_COLOR_ORANGERED, CV_COLOR_CHOCOLATE, CV_COLOR_GOLD,
                              CV_COLOR_YELLOW, CV_COLOR_OLIVE, CV_COLOR_LIGHTGREEN, CV_COLOR_DARKCYAN, CV_COLOR_SKYBLUE, CV_COLOR_INDIGO, CV_COLOR_PURPLE, CV_COLOR_PINK, 
                              CV_COLOR_DEEPPINK, CV_COLOR_VIOLET};

struct target
{
    int x;
    int y;
    int w;
    int h;
    std::string confidence;
    std::string target_type;
    int track_id;
};

struct customInfo{
    int channel_id;
    std::vector<target> target_list;
};

typedef std::shared_ptr<customInfo> PcustomInfo;

int video_frame_count = 0;
std::shared_ptr<Ifai::Ifmp4::Mp4ReaderInterface> reader_;
std::shared_ptr<Ifai::Decoder> decode_;
unsigned int time_base = 90000;
FILE* fp_ = NULL;
int once = 1;
PcustomInfo show_info;
std::string gps_info;
int show_frame_count = 0;
int show_frame_index = 0;
Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info mp4_info;

using namespace Ifai;
using namespace Ifmp4;

bool writeFile(void* data, unsigned lenght, std::string file_name){
    FILE* fp = fopen(file_name.c_str(), "w+");
    if (fp && data && lenght)
    {

        auto write_len = fwrite(data, 1, lenght, fp);
        if (write_len != lenght)
        {
            printf("fwrite failed, lenght:%u ret:%lu", lenght, write_len);
        }

        fclose(fp);
        fp = NULL;
    }

    return true;
}

PcustomInfo parseJson(std::string& str){
    Json::Reader reader;
    Json::Value root;
    if (reader.parse(str, root) == false){
        printf("json parse failed\n");
        return nullptr;
    }

    PcustomInfo info = std::make_shared<customInfo>();
    try{
        info->channel_id = root["channel_id"].asInt();
        for(auto node : root["target"]){
            target t;
            t.target_type = node["target_type"].asString();
            t.confidence = node["confidence"].asString();
            t.track_id = node["track_id"].asInt();
            t.x = node["x"].asInt();
            t.y = node["y"].asInt();
            t.w = node["w"].asInt();
            t.h = node["h"].asInt();   
            info->target_list.push_back(t);
        }
    }catch(std::exception &e){
        fprintf(stderr, "%s", e.what());
        return nullptr;
    }

    return info;
}

void handleKey(char KeyValue){
    int cur_time;
    int seek_time;
    switch (KeyValue)
    {
    case ' ':
        printf("suspend playback, press any key to continue playback\n");
        cv::waitKey(0);
        break;
    case 0x51: //left
            cur_time = show_frame_index / mp4_info->frame_rate;
            if(cur_time > 5){
                seek_time = cur_time - 5;
            }else{
                seek_time = 0;
            }

            show_frame_index = seek_time * mp4_info->frame_rate;
            printf("seek cur_time:%d seek time:%d\n", cur_time, seek_time);
            reader_->seekReadFile(seek_time);
        break;
    case 0x53: //right
            cur_time = show_frame_index / mp4_info->frame_rate;
            seek_time = cur_time + 5;
            show_frame_index = seek_time * mp4_info->frame_rate;
            printf("seek cur_time:%d seek time:%d\n", cur_time, seek_time);
            reader_->seekReadFile(seek_time);
        break;
    default:
        break;
    }

}

int need_suspend = 0;
bool proc()
{
    Mp4ReaderInterface::Frame frame;
    auto ret = reader_->ReadFrame(frame);
    if (ret != Mp4ReaderInterface::kOk)
    {
        printf("mp4 read frame failed, ret:%d\n", ret); 
        return false;
    }

    if (frame.type == FrameType::video)
    {
        if(frame.frame_data_len == 6){
            return true;
        }

        if(decode_){
            cv::Mat mat;
            video_frame_count++;
            if(decode_->push(frame, mat) != 0){
                printf("decode failed\n");
                return true;
                // return false;
            }

            if(mat.data)
            {
                show_frame_index ++;
                if(show_frame_count && show_info){
                    for(auto target : show_info->target_list){
                        
                        cv::Scalar color = color_array[target.track_id % color_count];
                        std::string text =  target.target_type + "  confidence:" + target.confidence + "  track_id:" + std::to_string(target.track_id);
                        cv::putText(mat, text, cv::Point(target.x, target.y), cv::FONT_HERSHEY_SIMPLEX, 1.5, color, 2);
                        cv::rectangle(mat, cv::Point(target.x, target.y), cv::Point(target.x + target.w, target.y + target.h), color,3);
                    }
                    show_frame_count--;
                }

                if(!gps_info.empty()){
                    cv::putText(mat, gps_info, cv::Point(100, 100), cv::FONT_HERSHEY_SIMPLEX, 1.5, CV_COLOR_RED, 2);
                    gps_info.clear();
                }

                cv::imshow("window", mat);

                char KeyValue = 0;
                if(need_suspend){
                    KeyValue = cv::waitKey(0);
                    need_suspend = 0;
                }else{
                    KeyValue = cv::waitKey(frame.duration * 1000 / time_base);
                }
                handleKey(KeyValue);
                if( getWindowProperty("window", cv::WND_PROP_AUTOSIZE) != 0){
                    printf("close playback\n");
                    exit(0);
                }
            }
        }
    }
    else if(frame.type == FrameType::eventInfo)
    {
        std::string str(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        show_info = parseJson(str);
        if(show_info){
            show_frame_count = 2;
            for(auto target : show_info->target_list){
                if(target.target_type == "kengcao"){
//                    need_suspend = 1;
                }
                // printf("target video_frame_count:%d type:%s confidence:%s track_id:%d x y w h:%d %d %d %d\n", video_frame_count, target.target_type.c_str(), target.confidence.c_str(), target.track_id,target.x, target.y, target.w, target.h);
            }            
        }
    }else if(frame.type == FrameType::gpsInfo){
        std::string tmp(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        gps_info = tmp;
        // printf("gps info: %s\n", gps_info.c_str());
    }

    return true;
}

int main(int argc, char** argv)
{
    std::string path;
    int seek_time = 0;
    int optc; 
    while ((optc = getopt(argc, argv, ":s:f:")) != -1) {  
        // print(optc, argc, argv, optind);  
        switch (optc) {  
            case 's':
                seek_time = std::atoi(optarg);
                printf("s : optarg:%s seek_time:%d\n", optarg, seek_time);
                continue;
            default:  
                // printf("optarg:%s\n", optarg);
                continue;  
        }  
    }  

    for(int  i = optind; i < argc; i++){
        printf("remain arg:%s\n", argv[i]);
        path = argv[i];
    }

    if(path.empty()){
        printf("usage: input mp4 file\n");
        exit(1);
    }

    cv::namedWindow("window", cv::WINDOW_NORMAL);
    cv::resizeWindow("window", 640, 480);
    // cv::imshow("window", 0);
    reader_ = Mp4ReaderInterface::CreateNew();
    decode_ = std::make_shared<Decoder>();
    Mp4ReaderInterface::ERRNO ret = reader_->Open(path);
    if (ret != Mp4ReaderInterface::kOk)
    {
        std::cout << "mp4 open failed, ret:" << ret << std::endl;
        return -1;
    }

    reader_->seekReadFile(seek_time);
    mp4_info = reader_->GetMp4Info();
    show_frame_index = seek_time * mp4_info->frame_rate;
    decode_->init(mp4_info);

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
