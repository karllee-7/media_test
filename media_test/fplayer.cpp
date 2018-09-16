#include "fplayer.h"
#include <functional>
#include <chrono>

namespace karl{
using namespace std::placeholders;
using std::bind;
using std::chrono::milliseconds;
/*===================================================================================*/
fplayer::fplayer():video_queue(30), audio_queue(30){
}
//----------------------------------------------------------------------------
fplayer::~fplayer(){
    packet_keep_run = false;
    video_keep_run = false;
    audio_keep_run = false;
    if(th_core.joinable())
        th_core.join();
}
//----------------------------------------------------------------------------
void fplayer::take_packet(AVFormatContext *format_ctx, int videoindex, int audioindex){
    qDebug() << "thread take_packet start.";
    int ret;
    while(packet_keep_run){
        AVPacket *frame_pkt = av_packet_alloc();
        ret = av_read_frame(format_ctx, frame_pkt);
        if(ret < 0)
            break;
        if(frame_pkt->stream_index == videoindex){
            unique_lock<mutex> lock(video_queue._lock);
            while(video_queue.is_full() && packet_keep_run)
                video_queue._cv.wait_for(lock, milliseconds(500));
            if(packet_keep_run){
                video_queue._queue.push(frame_pkt);
                video_queue._cv.notify_all();
            }
        }else if(frame_pkt->stream_index == audioindex){
            unique_lock<mutex> lock(audio_queue._lock);
            while(audio_queue.is_full() && packet_keep_run)
                audio_queue._cv.wait_for(lock, milliseconds(500));
            if(packet_keep_run){
                audio_queue._queue.push(frame_pkt);
                audio_queue._cv.notify_all();
            }
        }
    }
    qDebug() << "thread take_packet exit";
}
//----------------------------------------------------------------------------
void fplayer::decode_video(AVFormatContext *format_ctx, 
                        AVCodecContext *video_codec_ctx,
                        int videoindex, 
                        int width, int height,
                        fplayerVideoCallBack videoCallBack){
    qDebug() << "thread decode_video start.";
    //int ret;
    double timeBase = av_q2d(format_ctx->streams[videoindex]->time_base);
    AVPacket *frame_pkt = NULL;
    AVFrame *pframeYUV = av_frame_alloc();
    AVFrame *pframeRGB = av_frame_alloc();
    SwsContext *pImgCvtCtx = sws_getContext(
                video_codec_ctx->width,
                video_codec_ctx->height,
                video_codec_ctx->pix_fmt,
                width, height,
                AV_PIX_FMT_RGB565LE,
                SWS_BICUBIC, NULL, NULL, NULL);
    while(video_keep_run){
        {
            unique_lock<mutex> lock(video_queue._lock);
            while(video_queue.is_empty() && video_keep_run)
                video_queue._cv.wait_for(lock, milliseconds(500));
            if(video_keep_run){
                frame_pkt = video_queue._queue.front();
                video_queue._queue.pop();
                video_queue._cv.notify_all();
            }else{
                frame_pkt = NULL;
            }
        }
        if(frame_pkt == NULL)
            break;
        avcodec_send_packet(video_codec_ctx, frame_pkt);
        while(avcodec_receive_frame(video_codec_ctx, pframeYUV) == 0){
            auto img_rgb = (uint8_t *)malloc(sizeof(uint16_t)*width*height);
            av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, (const uint8_t *)img_rgb,
                        AV_PIX_FMT_RGB565BE, width, height, 1);
            sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
                        pframeRGB->data, pframeRGB->linesize);
            {
                unique_lock<mutex> lock(timestamp._lock);
                if(frame_pkt->pts * timeBase > timestamp._data)
                    timestamp._cv.wait(lock, [&]{return frame_pkt->pts * timeBase <= timestamp._data;});
                qDebug() << "vdecode " << frame_pkt->pts * timeBase << ", " << timestamp._data;
            }
            if(videoCallBack)
                videoCallBack(img_rgb, width, height);
            else
                free((void *)img_rgb);
            av_frame_unref(pframeYUV);
        }
        av_packet_free(&frame_pkt);
    }
    av_frame_free(&pframeYUV);
    av_frame_free(&pframeRGB);
    sws_freeContext(pImgCvtCtx);
    qDebug() << "thread decode_video exit";
}
//----------------------------------------------------------------------------
void fplayer::decode_audio(AVFormatContext *format_ctx, 
                        AVCodecContext *audio_codec_ctx,
                        int audioindex,
                        fplayerAudioCallBack audioCallBack){
    qDebug() << "thread decode_audio start.";
    int ret;
    bool mute_flg;
    snd_pcm_t *handle = NULL;
    if(audioCallBack == NULL){
        handle = pcm_open(audio_codec_ctx);
        if(handle == NULL){
            qDebug() << "open pcm error";
            return;
        }
    }
    double timeBase = av_q2d(format_ctx->streams[audioindex]->time_base);
    int frame_size = av_get_bytes_per_sample(audio_codec_ctx->sample_fmt);
    int channels = audio_codec_ctx->channels;
    AVPacket *frame_pkt;
    AVFrame *framePCM = av_frame_alloc();
    while(audio_keep_run){
        {
            unique_lock<mutex> lock(audio_queue._lock);
            while((audio_queue.is_empty() || in_pause) && audio_keep_run)
                audio_queue._cv.wait_for(lock, milliseconds(500));
            if(audio_keep_run){
                mute_flg = in_mute;
                frame_pkt = audio_queue._queue.front();
                audio_queue._queue.pop();
                audio_queue._cv.notify_all();
            }else{
                frame_pkt = NULL;
            }
        }
        if(frame_pkt == NULL)
            break;
        avcodec_send_packet(audio_codec_ctx, frame_pkt);
        while(avcodec_receive_frame(audio_codec_ctx, framePCM) == 0){
            int frame_num = framePCM->nb_samples;
            auto pcm_data = (uint8_t*)malloc(frame_size*frame_num*channels);
            if(mute_flg){
                memset(pcm_data, 0, frame_size*channels*frame_num);
            }else if(av_sample_fmt_is_planar(audio_codec_ctx->sample_fmt)){
                for(int i=0; i<frame_num; i++)
                    for(int k=0; k<channels; k++)
                        memcpy(pcm_data+frame_size*(channels*i+k), framePCM->data[k]+frame_size*i, frame_size);
            }else{
                memcpy(pcm_data, framePCM->data[0], frame_size*channels*frame_num);
            }
            {
                unique_lock<mutex> lock(timestamp._lock);
                timestamp._data = frame_pkt->pts * timeBase;
                qDebug() << "audio decode timestamp " << timestamp._data;
                timestamp._cv.notify_all();
            }
            if(audioCallBack)
                audioCallBack(pcm_data, framePCM->nb_samples, audio_codec_ctx->channels);
            else{
                ret = snd_pcm_writei(handle, pcm_data, frame_num);//写入声卡  （放音）
                if (ret == -EPIPE){
                    qDebug() << "underrun occurred";
                    snd_pcm_prepare(handle);
                }else if (ret < 0) {
                    qDebug() << "error from writei. " << snd_strerror(ret);
                }else if (ret != frame_num) {
                    qDebug() << "short write, write " << ret << " frames";
                }
                free((void *)pcm_data);
            }
            av_frame_unref(framePCM);
        }
        av_packet_free(&frame_pkt);
    }
    av_frame_free(&framePCM);
    if(audioCallBack == NULL){
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
    }
    qDebug() << "thread decode_audio exit.";
};
//----------------------------------------------------------------------------
int fplayer::core(string filename,
                  int width, int height,
                  fplayerVideoCallBack videoCallBack,
                  fplayerAudioCallBack audioCallBack,
                  fplayerExitCallBack exitCallBack){
    qDebug() << "thread fplayer core start.";
    int ret;
    AVFormatContext *format_ctx      = NULL;
    AVCodecContext  *video_codec_ctx = NULL;
    AVCodecContext  *audio_codec_ctx = NULL;
    int videoindex = -1;
    int audioindex = -1;
    auto cleanUp = [&](){
        {
            unique_lock<mutex> lock(video_queue._lock);
            while(!video_queue.is_empty()){
                AVPacket *frame_pkt = video_queue._queue.front();
                av_packet_free(&frame_pkt);
                video_queue._queue.pop();
            }
        }
        qDebug() << "clean up video_queue";
        {
            unique_lock<mutex> lock(audio_queue._lock);
            while(!audio_queue.is_empty()){
                AVPacket *frame_pkt = audio_queue._queue.front();
                av_packet_free(&frame_pkt);
                audio_queue._queue.pop();
            }
        }
        qDebug() << "clean up audio_queue";
        if(video_codec_ctx != NULL){
            avcodec_close(video_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            qDebug() << "clean up video_codec_ctx";
        }
        if(audio_codec_ctx != NULL){
            avcodec_close(audio_codec_ctx);
            avcodec_free_context(&audio_codec_ctx);
            qDebug() << "clean up audio_codec_ctx";
        }
        if(format_ctx != NULL){
            avformat_close_input(&format_ctx);
            avformat_free_context(format_ctx);
            qDebug() << "clean up format_ctx";
        }
    };
    av_register_all();
    format_ctx = avformat_alloc_context();
    if(format_ctx == NULL){
        qDebug() << "avformat_alloc_context error.";
        return -1;
    }
    ret = avformat_open_input(&format_ctx, filename.c_str(), NULL, NULL);
    if(ret < 0){
        char errbuf[1024];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString s(errbuf);
        qDebug("avformat_open_input error.\n%s\n", errbuf);
        cleanUp();
        return -1;
    }
    avformat_find_stream_info(format_ctx, NULL);
    for(int i=0; i<(int)format_ctx->nb_streams; i++) {
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            audioindex = i;
    }
    if(videoindex < 0){
        qDebug() << "can not find video stream.";
        cleanUp();
        return -1;
    }
    if(audioindex < 0){
        qDebug() << "can not find audio stream.";
        cleanUp();
        return -1;
    }
    av_dump_format(format_ctx, videoindex, filename.c_str(), 0);
    av_dump_format(format_ctx, audioindex, filename.c_str(), 0);

    AVStream *video_pstream = format_ctx->streams[videoindex];
    AVCodec *video_pcodec =  avcodec_find_decoder(video_pstream->codecpar->codec_id);
    if(video_pcodec == NULL){
        qDebug() << "cant find the video decoder.";
        cleanUp();
        return -1;
    }
    video_codec_ctx = avcodec_alloc_context3(video_pcodec);
    avcodec_parameters_to_context(video_codec_ctx, video_pstream->codecpar);
    av_codec_set_pkt_timebase(video_codec_ctx, video_pstream->time_base);
    ret = avcodec_open2(video_codec_ctx, video_pcodec, NULL);
    if(ret){
        qDebug() << "open video decoder error.";
        cleanUp();
        return -1;
    }
    AVStream *audio_pstream = format_ctx->streams[audioindex];
    AVCodec *audio_pcodec =  avcodec_find_decoder(audio_pstream->codecpar->codec_id);
    if(audio_pcodec == nullptr){
        qDebug() << "cant find the audio decoder.";
        cleanUp();
        return -1;
    }
    audio_codec_ctx = avcodec_alloc_context3(audio_pcodec);
    avcodec_parameters_to_context(audio_codec_ctx, audio_pstream->codecpar);
    av_codec_set_pkt_timebase(audio_codec_ctx, audio_pstream->time_base);
    ret = avcodec_open2(audio_codec_ctx, audio_pcodec, NULL);
    if(ret){
        qDebug() << "open audio decoder error.";
        cleanUp();
        return -1;
    }
    packet_keep_run = true;
    video_keep_run = true;
    audio_keep_run = true;
    timestamp._data = 0;
    //----------------------------------------------------------------------------
    thread th_packet= thread(bind(&fplayer::take_packet, this, format_ctx, videoindex, audioindex));
    thread th_video = thread(bind(&fplayer::decode_video, this, format_ctx, 
                            video_codec_ctx, videoindex, width, height,
                            videoCallBack));
    thread th_audio = thread(bind(&fplayer::decode_audio, this, format_ctx, 
                            audio_codec_ctx, audioindex,
                            audioCallBack));
    //----------------------------------------------------------------------------
    if(th_video.joinable())
        th_video.join();
    if(th_audio.joinable())
        th_audio.join();
    if(th_packet.joinable())
        th_packet.join();
    cleanUp();
    if(exitCallBack)
        exitCallBack();
    qDebug() << "thread fplayer core exit.";
    return 0;
}
int fplayer::play(string filename,
                int width, int height, 
                fplayerVideoCallBack videoCallBack,
                fplayerAudioCallBack audioCallBack,
                fplayerExitCallBack exitCallBack){
    if(th_core.joinable()){
        qDebug() << "warning! fplayer core is busy.";
        return -1;
    }
    auto fnc_core = bind(&fplayer::core, this, _1, _2, _3, _4, _5, _6);
    th_core = thread(fnc_core, filename,
                            width, height,
                            videoCallBack,
                            audioCallBack,
                            exitCallBack);
    return 0;
}
//----------------------------------------------------------------------------
snd_pcm_t *fplayer::pcm_open(AVCodecContext *audio_codec_ctx){
    int ret;
    snd_pcm_t *handle;
    ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if(ret < 0){
        qDebug() << "unable to open pcm device " << snd_strerror(ret);
        return NULL;
    }
#ifdef __arm__
    int rate = audio_codec_ctx->sample_rate;
#endif
#ifdef __amd64__
    int rate = 48000;
#endif
    ret = snd_pcm_set_params(handle,
                             SND_PCM_FORMAT_S16_LE,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             audio_codec_ctx->channels,
                             rate,
                             1,
                             500000);
    if(ret < 0){
        qDebug() << "unable to set hw parameters "  << snd_strerror(ret);
        snd_pcm_close(handle);
        return NULL;
    }
    return handle;
}
void fplayer::set_pause(){
    unique_lock<mutex> lock(audio_queue._lock);
    in_pause = true;
    audio_queue._cv.notify_all();
}
void fplayer::set_resume(){
    unique_lock<mutex> lock(audio_queue._lock);
    in_pause = false;
    audio_queue._cv.notify_all();
}
void fplayer::set_mute(){
    unique_lock<mutex> lock(audio_queue._lock);
    in_mute = true;
    audio_queue._cv.notify_all();
}
void fplayer::set_unmute(){
    unique_lock<mutex> lock(audio_queue._lock);
    in_mute = false;
    audio_queue._cv.notify_all();
}
/*========================================================================================*/
}

