#include "media_player.h"
//#include "vdecoder.h"
#include <assert.h>
#include <time.h>
#include <sys/time.h>

mediaPlayer::mediaPlayer(QObject *parent):QThread(parent){
    player = new fplayer();
}
mediaPlayer::~mediaPlayer(){
    delete player;
}
int mediaPlayer::open(const QString a, int width, int height){
    return player->open(a.toStdString().c_str(), width, height, NULL, NULL);
}
int mediaPlayer::command(CMD_KEY key){
    switch(key){
    case V_START:
        player->run();
        break;
    case V_PAUSE:
        ;
        break;
    case V_RESUME:
        ;
        break;
    case V_STOP:
        ;
        break;
    }
    return 0;
}
void mediaPlayer::run(){

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

