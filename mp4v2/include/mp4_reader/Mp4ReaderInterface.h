#ifndef _MP4_READER_INTERFACE_H_
#define _MP4_READER_INTERFACE_H_

#include <memory>
#include <string>
#include <map>

namespace Ifai {
namespace Ifmp4 {


enum FrameType{
    video = 0,
    audio,
    eventInfo,
    gpsInfo,
};

const std::map<FrameType, std::string> g_custom_frame_map = {{eventInfo, "MINE"}, {gpsInfo, "MGPS"}};
class Mp4ReaderInterface {
public:
    Mp4ReaderInterface();
public:
    struct Frame{
        FrameType type;
        void* frame_data;
        unsigned int frame_data_len; // 视频帧数据
        unsigned int pts;
        unsigned int dts;
        unsigned int duration;
        bool sync_frame;
    };

    struct Mp4Info{
        int video_type; // 264, 265
        int width;
        int height;
        int frame_rate;
        unsigned int bit_rate;
        unsigned int time_base;
        unsigned int video_duration;
        u_int8_t profile;
        u_int8_t level;
        u_int8_t compatibility;
        std::string sps;
        std::string pps;
        std::string vps;
    };

    typedef std::shared_ptr<Mp4Info> PMp4Info;

    enum ERRNO{
        kOk,
        KFailed,
        KFileNonexist,
        KFileNotOpen,
        KSampleReadFailed,
        KSeekOverFile,
        KSampleLack,
        KEof,
    };

    static std::shared_ptr<Mp4ReaderInterface> CreateNew();
    virtual ~Mp4ReaderInterface() = default;

    virtual ERRNO Open(const std::string& path) = 0;
    virtual PMp4Info GetMp4Info() = 0;
    virtual ERRNO ReadVideoFrame(Frame& frame) = 0;
    virtual ERRNO ReadFrame(Frame& frame) = 0;
    virtual ERRNO seekReadFile(unsigned int timeStamp) = 0;
};

} // namespace Ifmp4
} // namespace Ifai

#endif