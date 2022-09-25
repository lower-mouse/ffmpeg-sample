// extern "C"{
#define __STDC_CONSTANT_MACROS 
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <GL/glut.h>
#include <libswscale/swscale.h>

// }

static int global_bgr_buffer_size;
static void* p_global_bgr_buffer;
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int video_frame_count = 0;
static int audio_frame_count = 0;

static int output_video_frame(AVFrame *frame)
{
    if (frame->width != width || frame->height != height ||
        frame->format != pix_fmt) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr, "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                width, height, av_get_pix_fmt_name(pix_fmt),
                frame->width, frame->height,
                av_get_pix_fmt_name(frame->format));
        return -1;
    }

    printf("video_frame n:%d coded_n:%d\n",
           video_frame_count++, frame->coded_picture_number);

    /* copy decoded frame to destination buffer:
     * this is required since rawvideo expects non aligned data */
    av_image_copy(video_dst_data, video_dst_linesize,
                  (const uint8_t **)(frame->data), frame->linesize,
                  pix_fmt, width, height);

    /* write to rawvideo file */
    fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
    return 0;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO){
            printf("frame info width:%d height:%d format:%d linesize:%d %d %d %d\n", frame->width, frame->height, frame->format, frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3]);
            return 0;
        }

        if (ret < 0)
            return ret;
    }

    return 0;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
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

static int av_get_frame(){
    int ret;
    /* read frames from the file */
    if (av_read_frame(fmt_ctx, pkt) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise
        // skip it
        if (pkt->stream_index == video_stream_idx)
            ret = decode_packet(video_dec_ctx, pkt);

        av_packet_unref(pkt);
        if (ret < 0)
            return -1;
    }else{
        return -1;
    }

    if (frame->width != width || frame->height != height ||
        frame->format != pix_fmt) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr, "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                width, height, av_get_pix_fmt_name(pix_fmt),
                frame->width, frame->height,
                av_get_pix_fmt_name(frame->format));
        return -1;
    }
    return 0;
}

void display(){
    // glClear(GL_COLOR_BUFFER_BIT);
    if(av_get_frame() != 0){
        printf("get frame failed\n");
        av_frame_unref(frame);
        return ;
    }

    static int once = 0;


    struct SwsContext* swsContext = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P,frame->width, frame->height, AV_PIX_FMT_RGB24,
                          0, NULL, NULL, NULL);

    int linesize[8] = {frame->linesize[0] * 3};
    uint8_t* bgr_buffer[8] = {p_global_bgr_buffer};

    printf("frame width:%d height:%d pix_fmt:%d\n", frame->width, frame->height, frame->pict_type);
    sws_scale(swsContext, frame->data, frame->linesize, 0, height, bgr_buffer, linesize);
    
    av_frame_unref(frame);

    if(once == 0){
        fwrite(p_global_bgr_buffer, 1, global_bgr_buffer_size, video_dst_file);
        once = 1;
        fclose(video_dst_file);
        video_dst_file = NULL;
    }

    glRasterPos3f(-1.0f,1.0f,0);
	//Zoom, Flip
	glPixelZoom(1.0f, -1.0f);

    glDrawPixels(896, height, GL_RGB, GL_UNSIGNED_BYTE, bgr_buffer[0]);
    glFlush();
}

void timeDisplayFunc(int value){
    display();
    // Present frame every 40 ms
    glutTimerFunc(40, timeDisplayFunc, 0);
}


int main(int argc, char**argv){
    int ret;
    src_filename = argv[1];
    video_dst_filename = argv[2];
    audio_dst_filename = argv[3];
    av_register_all();
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        video_dst_file = fopen(video_dst_filename, "wb");
        if (!video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
            ret = 1;
            goto end;
        }

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        global_bgr_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 32);
        p_global_bgr_buffer = malloc(global_bgr_buffer_size);
        printf("global_bgr_buffer_size:%d\n", global_bgr_buffer_size);
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             width, height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto end;
        }
        video_dst_bufsize = ret;
    }

    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dst_file = fopen(audio_dst_filename, "wb");
        if (!audio_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
            ret = 1;
            goto end;
        }
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!audio_stream && !video_stream) {
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

    if (video_stream)
        printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
    if (audio_stream)
        printf("Demuxing audio from file '%s' into '%s'\n", src_filename, audio_dst_filename);


    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowPosition(0, 0); // 用于设置窗口的位置。可以指定x，y坐标
    glutInitWindowSize(896, 480); //用于设置窗口的大小。可以设置窗口的宽，高。
    
    glutCreateWindow("OpenGL 3D View"); //创建窗口
    
    glutDisplayFunc(display); //设置绘制函数

    glutTimerFunc(40, timeDisplayFunc, 0);
    
    glutMainLoop(); //glutMainLoop()将会进入GLUT事件处理循环
end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (video_dst_file)
        fclose(video_dst_file);
    if (audio_dst_file)
        fclose(audio_dst_file);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return ret < 0;
}