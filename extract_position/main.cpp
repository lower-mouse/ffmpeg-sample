#include <memory>
#include <stdio.h>
#include "mp4_reader/Mp4ReaderInterface.h"
#include <string>
#include <thread>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>


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



long long max_gap = 0;
long long min_gsp = 100000000;
int need_suspend = 0;
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
      
    }
    else if(frame.type == FrameType::eventInfo)
    {
    
    }else if(frame.type == FrameType::gpsInfo){
        std::string tmp(reinterpret_cast<char *>(frame.frame_data), frame.frame_data_len);
        gps_info = tmp;

        std::string str = gps_info.c_str();
        
        auto lenght = str.find(',');
        if(lenght != std::string::npos){
            std::string timestamp = str.substr(0, lenght);
            std::string dev_time = timestamp.substr(0, str.find('@'));
            std::string gps_time = timestamp.substr(str.find('@') + 1);

            printf("dev_time:%s gps_time:%s\n", dev_time.c_str(), gps_time.c_str());
            long long gap = std::atoll(dev_time.c_str()) - std::atoll(gps_time.c_str()) - 28800000;
            if(max_gap < gap){
                max_gap = gap;
            }

            if(min_gsp > gap){
                min_gsp = gap;
            }
            fprintf(stderr, "gps info: %s    %lld\n", str.c_str(), gap);
        }
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

    reader_ = Mp4ReaderInterface::CreateNew();
    Mp4ReaderInterface::ERRNO ret = reader_->Open(path);
    if (ret != Mp4ReaderInterface::kOk)
    {
        std::cout << "mp4 open failed, ret:" << ret << std::endl;
        return -1;
    }

    reader_->seekReadFile(seek_time);
    mp4_info = reader_->GetMp4Info();
    show_frame_index = seek_time * mp4_info->frame_rate;

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
        printf("max gap:%lld min gap:%lld\n", max_gap, min_gsp);
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
