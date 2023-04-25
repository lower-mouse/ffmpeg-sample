#include <memory>
#include <stdio.h>
#include "mp4_reader/Mp4ReaderInterface.h"
#include "decoder.h"
#include "encoder.h"
#include <string>
#include <thread>
#include <iostream>
#include <stdlib.h>
#include <list>
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
using namespace Ifai;
using namespace Ifmp4;

const int color_count = 9;
cv::Scalar color_array[color_count] = {CV_COLOR_RED, CV_COLOR_GREEN, CV_COLOR_BLUE, CV_COLOR_DARKGRAY, 
    CV_COLOR_CHOCOLATE, CV_COLOR_YELLOW, CV_COLOR_DARKCYAN, CV_COLOR_SKYBLUE,  CV_COLOR_PINK };

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
    std::list<target> target_list;
};

typedef std::shared_ptr<customInfo> PcustomInfo;

int video_frame_count = 0;
std::shared_ptr<Ifai::Ifmp4::Mp4ReaderInterface> reader_;
std::shared_ptr<Ifai::Decoder> decode_;
unsigned int time_base = 90000;
int once = 1;
PcustomInfo show_info;
std::string gps_info;
int show_frame_count = 0;
int show_frame_index = 0;
Ifai::Ifmp4::Mp4ReaderInterface::PMp4Info mp4_info;

std::vector<std::string> keep_type;
std::string convert_output_path;

bool need_display = false;
Encoder* encoder = NULL;

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

std::vector<std::string> g_target_type_map = {
        "rubbish",
        "pack_garbage",
        "rubbish_yes",
        "rubbish_serious",
        "rubbish_pack",
        "hand_trash_bag",
        "human_headshoulder_area",
        "hand_area",
        "trashbox",
        "junkman"};

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
            // t.confidence = node["confidence"].asString();
            t.track_id = node["track_id"].asInt();
            if(t.target_type.empty()){
                unsigned int type = (t.track_id >> 16)&0xffff;
                t.track_id = t.track_id & 0xffff;
                t.target_type = g_target_type_map[type];
            }

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

bool proc()
{
    Mp4ReaderInterface::Frame frame;
    auto ret = reader_->ReadFrame(frame);
    if ((ret != Mp4ReaderInterface::kOk && ret != Mp4ReaderInterface::KSampleReadFailed) || 
    (ret == Mp4ReaderInterface::KSampleReadFailed && frame.type == FrameType::video))
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
                        std::string text =  target.target_type + "  track_id:" + std::to_string(target.track_id);

                        int baseline=0;
                        cv::Size textSize = getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                1.5, 2, &baseline);

                        cv::Point text_point((target.x - (textSize.width - target.w)/2 ), target.y + 5);
                        if(text_point.x + textSize.width > mat.cols){
                            text_point.x = mat.cols - textSize.width;
                        }else if(text_point.x <= 0){
                            text_point.x = 5;
                        }

                        cv::putText(mat, text, text_point, cv::FONT_HERSHEY_SIMPLEX, 1.5, color, 2);
                        cv::rectangle(mat, cv::Point(target.x, target.y), cv::Point(target.x + target.w, target.y + target.h), color,3);
                    }
                    show_frame_count--;
                }

                if(!gps_info.empty()){
                    cv::putText(mat, gps_info, cv::Point(100, 100), cv::FONT_HERSHEY_SIMPLEX, 1.5, CV_COLOR_RED, 2);
                    gps_info.clear();
                }

                if(need_display){
                    cv::imshow("window", mat);
                    char KeyValue = 0;
                    KeyValue = cv::waitKey(frame.duration * 1000 / time_base);
                    handleKey(KeyValue);
                    if( getWindowProperty("window", cv::WND_PROP_AUTOSIZE) != 0){
                        printf("close playback\n");
                        return false;
                    }
                }

                if(encoder){
                    encoder->encode(mat);
                }

            }
        }
    }
    else if(frame.type == FrameType::eventInfo)
    {
        std::string str(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        show_info = parseJson(str);
        if(show_info){
            show_frame_count = 1;
        }

        if(show_info && !keep_type.empty()){
            for(auto target = show_info->target_list.begin(); target != show_info->target_list.end(); ){
                // printf("target video_frame_count:%d type:%s confidence:%s track_id:%d x y w h:%d %d %d %d\n", video_frame_count, target->target_type.c_str(), target->confidence.c_str(), target->track_id,target->x, target->y, target->w, target->h);
                bool need_keep = false;
                for(auto type : keep_type){
                    if(target->target_type == type){
                        need_keep = true;
                        break;
                    }
                }

                if(!need_keep){
                    target = show_info->target_list.erase(target);
                }else{
                    target++;
                }
            }            
        }
    }else if(frame.type == FrameType::gpsInfo){
        std::string tmp(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        gps_info = tmp;
        // printf("gps info: %s\n", gps_info.c_str());
    }

    return true;
}

void usage(int argc, char** argv){
    printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
    printf("-s=[seek time]     seek\n");
    printf("-k=[keep_type]     to hide all types except \"keep type\"\n");
    printf("-o=[file_name]     save to h264 file\n");
    printf("-p                 play video in sceen\n");
}

int main(int argc, char** argv)
{
    std::string path;
    int seek_time = 0;
    int optc; 
    while ((optc = getopt(argc, argv, ":s:k:o:ph")) != -1) {  
        // print(optc, argc, argv, optind);  
        switch (optc) {  
            case 's':
                seek_time = std::atoi(optarg);
                printf("s : optarg:%s seek_time:%d\n", optarg, seek_time);
                continue;
            case 'k':
                printf("k : optarg:%s \n", optarg);
                // keep_type = optarg;
                keep_type.push_back(optarg);
                continue;
            case 'o':
                printf("o : optarg:%s \n", optarg);
                convert_output_path = optarg;
                continue;
            case 'p':
                need_display = true;
                printf("p : need_display\n");
                continue;
            case 'h':
                usage(argc, argv);
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
        usage(argc, argv);
        exit(1);
    }

    if(need_display){
        cv::namedWindow("window", cv::WINDOW_NORMAL);
        cv::resizeWindow("window", 640, 480);
    }
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

    if(!convert_output_path.empty()){
        encoder = new Encoder();
        encoder->init(convert_output_path, mp4_info->width, mp4_info->height, mp4_info->frame_rate);
    }

    new std::thread([]() -> bool
                    {
    while(1){
      auto ret = proc();
      if(!ret){
        if(encoder){
            encoder->uninit();
        }
        exit(0);
      }
    } });

    while(1){
        sleep(1);
    }
}
