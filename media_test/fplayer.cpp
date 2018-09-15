#include "fplayer.h"

namespace karl{
/*===================================================================================*/
void fplayer::cleanup(){
    if(th_video.joinable())
        th_video.join();
    if(th_audio.joinable())
        th_audio.join();
    if(th_packet.joinable())
        th_packet.join();
    if(video_codec_ctx != NULL){
        avcodec_close(video_codec_ctx);
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = NULL;
    }
    if(audio_codec_ctx != NULL){
        avcodec_close(audio_codec_ctx);
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = NULL;
    }
    if(format_ctx != nullptr){
        avformat_close_input(&format_ctx);
        avformat_free_context(format_ctx);
        format_ctx = NULL;
    }
    videoCallBack = NULL;
    audioCallBack = NULL;
}
//----------------------------------------------------------------------------
fplayer::fplayer():video_queue(30), audio_queue(30){
    videoCallBack = NULL;
    audioCallBack = NULL;
    format_ctx = NULL;
    video_codec_ctx = NULL;
    audio_codec_ctx = NULL;
}
//----------------------------------------------------------------------------
fplayer::~fplayer(){
    packet_keep_run = false;
    video_keep_run = false;
    audio_keep_run = false;
    cleanup();
}
//----------------------------------------------------------------------------
void fplayer::take_packet(AVFormatContext *format_ctx, int videoindex, int audioindex){
    int ret;
    while(packet_keep_run){
        AVPacket *frame_pkt = av_packet_alloc();
        ret = av_read_frame(format_ctx, frame_pkt);
        if(ret < 0)
            break;
        if(frame_pkt->stream_index == videoindex){
            unique_lock<mutex> lock(video_queue._lock);
            if(video_queue.is_full())
                video_queue._cv.wait(lock, [this]{return !video_queue.is_full();});
            video_queue._queue.push(frame_pkt);
            video_queue._cv.notify_all();
        }else if(frame_pkt->stream_index == audioindex){
            unique_lock<mutex> lock(audio_queue._lock);
            if(audio_queue.is_full())
                audio_queue._cv.wait(lock, [this]{return !audio_queue.is_full();});
            audio_queue._queue.push(frame_pkt);
            audio_queue._cv.notify_all();
        }
    }
}
//----------------------------------------------------------------------------
void fplayer::decode_video(AVFormatContext *format_ctx, int videoindex, int width, int height){
    //int ret;
    double timeBase = av_q2d(format_ctx->streams[videoindex]->time_base);
    AVPacket *frame_pkt;
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
        //qDebug() << "decode wait for new video frame";
        {
            unique_lock<mutex> lock(video_queue._lock);
            if(video_queue.is_empty())
                video_queue._cv.wait(lock, [this]{return !video_queue.is_empty();});
            frame_pkt = video_queue._queue.front();
            video_queue._queue.pop();
            video_queue._cv.notify_all();
        }
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
            // usleep(60e3);
        }
        av_packet_free(&frame_pkt);
    }
    sws_freeContext(pImgCvtCtx);
    av_frame_free(&pframeYUV);
    av_frame_free(&pframeRGB);
};
//----------------------------------------------------------------------------
void fplayer::decode_audio(AVFormatContext *format_ctx, int audioindex){
    //int ret;
    double timeBase = av_q2d(format_ctx->streams[audioindex]->time_base);
    int frame_size = av_get_bytes_per_sample(audio_codec_ctx->sample_fmt);
    AVPacket *frame_pkt;
    AVFrame *framePCM = av_frame_alloc();
    while(audio_keep_run){
        {
            unique_lock<mutex> lock(audio_queue._lock);
            if(audio_queue.is_empty())
                audio_queue._cv.wait(lock, [this]{return !audio_queue.is_empty();});
            frame_pkt = audio_queue._queue.front();
            audio_queue._queue.pop();
            audio_queue._cv.notify_all();
        }
        avcodec_send_packet(audio_codec_ctx, frame_pkt);
        while(avcodec_receive_frame(audio_codec_ctx, framePCM) == 0){
            auto pcm_data = (uint8_t*)malloc(frame_size*framePCM->nb_samples*audio_codec_ctx->channels);
            if(av_sample_fmt_is_planar(audio_codec_ctx->sample_fmt)){
                for(int i=0; i<framePCM->nb_samples; i++)
                    for(int k=0; k<audio_codec_ctx->channels; k++)
                        memcpy(pcm_data, framePCM->data[k]+frame_size*i, frame_size);
            }else{
                for(int i=0; i<framePCM->nb_samples; i++)
                    for(int k=0; k<audio_codec_ctx->channels; k++)
                        memcpy(pcm_data, framePCM->data[0]+frame_size*(audio_codec_ctx->channels*i+k), frame_size);
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
                free((void *)pcm_data);
            }
            usleep(30e3);
        }
        av_packet_free(&frame_pkt);
        av_frame_unref(framePCM);
    }
    av_frame_free(&framePCM);
};
//----------------------------------------------------------------------------
int fplayer::play(const char* filename, int width, int height, 
                fplayerVideoCallBack p_videoCallBack,
                fplayerAudioCallBack p_audioCallBack){
    int ret;
    int videoindex = -1;
    int audioindex = -1;
    auto 
    av_register_all();
    AVFormatContext *format_ctx = avformat_alloc_context();
    assert(format_ctx != NULL);
    ret = avformat_open_input(&format_ctx, filename, NULL, NULL);
    if(ret < 0){
        char errbuf[1024];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString s(errbuf);
        qDebug("avformat_open_input error.\n%s\n", errbuf);
    }
    assert(ret >= 0);
    avformat_find_stream_info(format_ctx, NULL);
    for(int i=0; i<(int)format_ctx->nb_streams; i++) {
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            audioindex = i;
    }
    if(videoindex < 0){
        qDebug() << "can not find video stream.";
        cleanup();
        return -1;
    }
    if(audioindex < 0){
        qDebug() << "can not find audio stream.";
        cleanup();
        return -1;
    }
    av_dump_format(format_ctx, videoindex, filename, 0);
    av_dump_format(format_ctx, audioindex, filename, 0);

    AVStream *video_pstream = format_ctx->streams[videoindex];
    AVCodec *video_pcodec =  avcodec_find_decoder(video_pstream->codecpar->codec_id);
    if(video_pcodec == NULL){
        qDebug() << "cant find the video decoder.";
        cleanup();
        return -1;
    }
    video_codec_ctx = avcodec_alloc_context3(video_pcodec);
    avcodec_parameters_to_context(video_codec_ctx, video_pstream->codecpar);
    av_codec_set_pkt_timebase(video_codec_ctx, video_pstream->time_base);
    ret = avcodec_open2(video_codec_ctx, video_pcodec, NULL);
    if(ret){
        qDebug() << "open video decoder error.";
        cleanup();
        return -1;
    }
    AVStream *audio_pstream = format_ctx->streams[audioindex];
    AVCodec *audio_pcodec =  avcodec_find_decoder(audio_pstream->codecpar->codec_id);
    if(audio_pcodec == nullptr){
        qDebug() << "cant find the audio decoder.";
        cleanup();
        return -1;
    }
    audio_codec_ctx = avcodec_alloc_context3(audio_pcodec);
    avcodec_parameters_to_context(audio_codec_ctx, audio_pstream->codecpar);
    av_codec_set_pkt_timebase(audio_codec_ctx, audio_pstream->time_base);
    ret = avcodec_open2(audio_codec_ctx, audio_pcodec, NULL);
    if(ret){
        qDebug() << "open audio decoder error.";
        cleanup();
        return -1;
    }
    videoCallBack = p_videoCallBack;
    audioCallBack = p_audioCallBack;
    packet_keep_run = true;
    video_keep_run = true;
    audio_keep_run = true;
    timestamp._data = 0;
    //----------------------------------------------------------------------------
    th_packet= thread(bind(&fplayer::take_packet, this, format_ctx, videoindex, audioindex));
    th_video = thread(bind(&fplayer::decode_video, this, format_ctx, videoindex, width, height));
    th_audio = thread(bind(&fplayer::decode_audio, this, format_ctx, audioindex));
    return 0;



    avformat_close_input(&format_ctx);
    avformat_free_context(format_ctx);
}
/*========================================================================================*/
}

