#include "media_player.h"
//#include "vdecoder.h"
#include <assert.h>
#include <time.h>
#include <sys/time.h>

inline double get_curr_time_s(){
    struct timeval t0;
    gettimeofday(&t0, NULL);
    return ((double)t0.tv_sec + (double)t0.tv_usec / 1.0e6);
}

struct {
    std::atomic<bool> stop;
    std::atomic<bool> pause;
    std::atomic<bool> resume;
}request;
struct {
    std::atomic<bool> started;
    std::atomic<bool> running;
}status;

mediaPlayer::mediaPlayer(QObject *parent):QThread(parent){
    request.stop = false;
    request.pause = false;
    request.resume = false;
    status.running = false;
    status.started = false;
    qDebug() << "media_player thread id " << QThread::currentThreadId() << endl;
}
QString mediaPlayer::rd_file_name() const{
    return file_name;
}
void mediaPlayer::wt_file_name(const QString a){
    file_name = a;
}
int mediaPlayer::rd_dst_width() const{
    return dst_width;
}
void mediaPlayer::wt_dst_width(const int a){
    dst_width = a;
}
int mediaPlayer::rd_dst_height() const{
    return dst_height;
}
void mediaPlayer::wt_dst_height(const int a){
    dst_height = a;
}
int mediaPlayer::open(){
    this->start();
    return 0;
}
int mediaPlayer::open(const QString a){
    file_name = a;
    this->start();
    return 0;
}
int mediaPlayer::command(CMD_KEY key){
    switch(key){
    case V_START:
        this->start();
        break;
    case V_PAUSE:
        request.pause = true;
        break;
    case V_RESUME:
        request.resume = true;
        break;
    case V_STOP:
        request.stop = true;
        break;
    }
    return 0;
}
void mediaPlayer::run(){
    status.started = true;
    int ret;
    char errbuf[1024];
    AVFormatContext *pFormatCtx = nullptr;
    int videoindex = -1;
    int audioindex = -1;
    AVCodecContext *video_pcodec_ctx = nullptr;
    AVCodecContext *audio_pcodec_ctx = nullptr;
    av_register_all();
    pFormatCtx = avformat_alloc_context();
    try{
        ret = avformat_open_input(&pFormatCtx, file_name.toStdString().c_str(), NULL, NULL);
        if(ret < 0){
            av_strerror(ret, errbuf, sizeof(errbuf));
            throw vError(ret, errbuf);
        }
        avformat_find_stream_info(pFormatCtx, NULL);
        for(int i=0; i<(int)pFormatCtx->nb_streams; i++) {
                if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
                        videoindex = i;
                }
                if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
                        audioindex = i;
                }
        }
        if(videoindex < 0)
            throw vError(-1, "cant fine video stream.");
        else {
            av_dump_format(pFormatCtx, videoindex, file_name.toStdString().c_str(), 0);
            AVStream *video_pstream = pFormatCtx->streams[videoindex];
            AVCodec *video_pcodec =  avcodec_find_decoder(video_pstream->codecpar->codec_id);
            if(video_pcodec == nullptr)
                throw vError(-1, "cant find the video decoder.");
            video_pcodec_ctx = avcodec_alloc_context3(video_pcodec);
            avcodec_parameters_to_context(video_pcodec_ctx, video_pstream->codecpar);
            av_codec_set_pkt_timebase(video_pcodec_ctx, video_pstream->time_base);
            ret = avcodec_open2(video_pcodec_ctx, video_pcodec, NULL);
            if(ret)
                throw vError(-1, "open video decoder error.");
            qDebug() << "open video decoder success." << endl;
        }

        if(audioindex < 0)
            qWarning() << "media player warning: no audio stream found.";
        else {
            av_dump_format(pFormatCtx, audioindex, file_name.toStdString().c_str(), 0);
            AVStream *audio_pstream = pFormatCtx->streams[audioindex];
            AVCodec *audio_pcodec =  avcodec_find_decoder(audio_pstream->codecpar->codec_id);
            if(audio_pcodec == nullptr)
                throw vError(-1, "cant find the audio decoder.");
            audio_pcodec_ctx = avcodec_alloc_context3(audio_pcodec);
            avcodec_parameters_to_context(audio_pcodec_ctx, audio_pstream->codecpar);
            av_codec_set_pkt_timebase(audio_pcodec_ctx, audio_pstream->time_base);
            ret = avcodec_open2(audio_pcodec_ctx, audio_pcodec, NULL);
            if(ret)
                throw vError(-1, "open audio decoder error.");
            qDebug() << "audio sample_rate " << audio_pcodec_ctx->sample_rate << endl;
            qDebug() << "audio channels " << audio_pcodec_ctx->channels << endl;
            qDebug() << "open audio decoder success." << endl;
        }
    }
    catch(vError err){
        qCritical("media player error: %s | code %d\n", err.what(), err.err());
        if(video_pcodec_ctx != nullptr){
            avcodec_close(video_pcodec_ctx);
            avcodec_free_context(&video_pcodec_ctx);
        }
        if(audio_pcodec_ctx != nullptr){
            avcodec_close(audio_pcodec_ctx);
            avcodec_free_context(&audio_pcodec_ctx);
        }
        if(pFormatCtx != nullptr){
            avformat_close_input(&pFormatCtx);
            avcodec_free_context(&video_pcodec_ctx);
        }
        status.started = false;
        return;
    }
    /*=======================================================================*/
    AVPacket *frame_pkt = av_packet_alloc();
    AVFrame *pframeYUV = av_frame_alloc();
    AVFrame *pframeRGB = av_frame_alloc();
    AVFrame *pframePCM = av_frame_alloc();
    SwsContext *pImgCvtCtx = sws_getContext(
                video_pcodec_ctx->width,
                video_pcodec_ctx->height,
                video_pcodec_ctx->pix_fmt,
                dst_width,
                dst_height,
                AV_PIX_FMT_RGB565LE,
                SWS_BICUBIC, NULL, NULL, NULL);
    //int frame_index = 0;
    status.running = true;
    time_start_s = get_curr_time_s();
    while(av_read_frame(pFormatCtx, frame_pkt) >= 0){
        if(frame_pkt->stream_index == videoindex){
            qDebug() << "get video frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
            // qDebug() << "video_pstream->time_base " << video_pstream->time_base
            avcodec_send_packet(video_pcodec_ctx, frame_pkt);
            while(avcodec_receive_frame(video_pcodec_ctx, pframeYUV) == 0){
                QImage img_rgb(dst_width, dst_height, QImage::Format_RGB16);
                av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, img_rgb.bits(),
                                AV_PIX_FMT_RGB565BE, dst_width, dst_height, 1);
                sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
                                pframeRGB->data, pframeRGB->linesize);
                double tp = frame_pkt->pts*av_q2d(pFormatCtx->streams[videoindex]->time_base);
                while(true){
                    double tc = get_curr_time_s();
                    if((tc-time_start_s) > tp)
                        break;
                    usleep(10e3);
                }
                emit refreshScrean(img_rgb);
                // usleep(60e3);
                av_frame_unref(pframeYUV);
            }
            av_packet_unref(frame_pkt);
        }else if(frame_pkt->stream_index == audioindex){
            qDebug() << "get audio frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
#if 0
            avcodec_send_packet(audio_pcodec_ctx, frame_pkt);
            while(avcodec_receive_frame(audio_pcodec_ctx, pframePCM) == 0){
                QImage img_rgb(dst_width, dst_height, QImage::Format_RGB16);
                av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, img_rgb.bits(),
                                AV_PIX_FMT_RGB565BE, dst_width, dst_height, 1);
                sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
                                pframeRGB->data, pframeRGB->linesize);
                double tp = frame_pkt->pts*av_q2d(pFormatCtx->streams[videoindex]->time_base);
                while(true){
                    double tc = get_curr_time_s();
                    if((tc-time_start_s) > tp)
                        break;
                    usleep(10e3);
                }
                emit refreshScrean(img_rgb);
                // usleep(60e3);
                av_frame_unref(pframePCM);
            }
#endif
        }else{
            //qDebug("drop channel %d frame.", frame_pkt->stream_index);
        }
        /*
        if(frame_index > 20000)
                break;
        frame_index++;
        */
        if(request.stop){
            request.stop = false;
            status.running = false;
            break;
        }
        if(request.pause){
            request.pause = false;
            status.running = false;
            double t0 = get_curr_time_s();
            while(!request.resume && !request.stop){
                usleep(50000);
            }
            double t1 = get_curr_time_s();
            time_start_s += (t1-t0);
            request.resume = false;
            status.running = true;
            continue;
        }
        if(request.resume){
            request.resume = false;
            continue;
        }
    }
    status.running = false;
    sws_freeContext(pImgCvtCtx);
    av_frame_free(&pframePCM);
    av_frame_free(&pframeYUV);
    av_frame_free(&pframeRGB);
    av_packet_free(&frame_pkt);
    /*=======================================================================*/
    status.started = false;
    if(audio_pcodec_ctx != nullptr){
        avcodec_close(audio_pcodec_ctx);
        avcodec_free_context(&audio_pcodec_ctx);
    }
    if(video_pcodec_ctx != nullptr){
        avcodec_close(video_pcodec_ctx);
        avcodec_free_context(&video_pcodec_ctx);
    }
    if(pFormatCtx != nullptr){
        avformat_close_input(&pFormatCtx);
        //avcodec_free_context(&video_pcodec_ctx);
    }
    emit videoExit(0);
}
mediaPlayer::~mediaPlayer(){
    request.stop = true;
    while(status.started){
        usleep(50000);
    }
}
