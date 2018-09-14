#ifndef _FPLAYER_H_
#define _FPLAYER_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
//#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

#include <unistd.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <functional>
#include <QDebug>
#include "karl_queue.h"

namespace karl{
using std::thread;
using std::atomic;
using std::mutex;
using std::unique_lock;
using std::cout;
using std::endl;
/*===================================================================================*/
typedef std::function<void(uint8_t*, int, int)> fplayerCallBack;
/*===================================================================================*/
class fplayer{
	mutex frame_lock;
	int dst_width;
	int dst_height;
	AVFormatContext *pFormatCtx;
	AVCodecContext *video_pcodec_ctx;
	AVCodecContext *audio_pcodec_ctx;
	fplayerCallBack videoCallBack;
	fplayerCallBack audioCallBack;
	int videoindex;
	int audioindex;
	thread th_packet;
	thread th_video;
	thread th_audio;
	atomic<bool> packet_keep_run;
	atomic<bool> video_keep_run;
	atomic<bool> audio_keep_run;
	karl_queue<AVPacket*> video_queue;
	karl_queue<AVPacket*> audio_queue;
	double timestamp;
public:
     	void cleanup(){
		packet_keep_run = false;
		video_keep_run = false;
		audio_keep_run = false;
		if(th_video.joinable())
			th_video.join();
		if(th_audio.joinable())
			th_audio.join();
		if(th_packet.joinable())
			th_packet.join();
        	if(video_pcodec_ctx != NULL){
                	avcodec_close(video_pcodec_ctx);
                        avcodec_free_context(&video_pcodec_ctx);
			video_pcodec_ctx = NULL;
                }
                if(audio_pcodec_ctx != NULL){
                        avcodec_close(audio_pcodec_ctx);
                        avcodec_free_context(&audio_pcodec_ctx);
			audio_pcodec_ctx = NULL;
                }
                if(pFormatCtx != nullptr){
                        avformat_close_input(&pFormatCtx);
			avformat_free_context(pFormatCtx);
			pFormatCtx = NULL;
                }
		videoindex = -1;
		audioindex = -1;
		videoCallBack = NULL;
		audioCallBack = NULL;
		packet_keep_run = true;
		video_keep_run = true;
		audio_keep_run = true;
	}
	fplayer(){
		dst_width = 640;
		dst_height = 480;
		videoCallBack = NULL;
		audioCallBack = NULL;
		videoindex = -1;
		audioindex = -1;
		pFormatCtx = NULL;
		video_pcodec_ctx = NULL;
		audio_pcodec_ctx = NULL;
		packet_keep_run = true;
		video_keep_run = true;
		audio_keep_run = true;
	}
	~fplayer(){
		cleanup();
	}
	int play(const char* filename, int out_width, int out_height, 
					fplayerCallBack p_videoCallBack,
					fplayerCallBack p_audioCallBack){
		int ret;
		//char errbuf[1024];
		if(pFormatCtx != NULL){
			qDebug() << "fplayer is busy.";
			return -1;
		}
		av_register_all();
		pFormatCtx = avformat_alloc_context();
		if(pFormatCtx == NULL){
			qDebug() << "avformat_alloc_context error.";
			return -1;
		}
		ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL);
		if(ret < 0){
			qDebug() << "avformat_open_input error.";
			cleanup();
			//av_strerror(ret, errbuf, sizeof(errbuf));
			return -1;
		}
		avformat_find_stream_info(pFormatCtx, NULL);
		for(int i=0; i<(int)pFormatCtx->nb_streams; i++) {
			if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
				videoindex = i;
			}
			if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
				audioindex = i;
			}
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
		av_dump_format(pFormatCtx, videoindex, filename, 0);
		av_dump_format(pFormatCtx, audioindex, filename, 0);

		AVStream *video_pstream = pFormatCtx->streams[videoindex];
		AVCodec *video_pcodec =  avcodec_find_decoder(video_pstream->codecpar->codec_id);
		if(video_pcodec == NULL){
			qDebug() << "cant find the video decoder.";
			cleanup();
			return -1;
		}
		video_pcodec_ctx = avcodec_alloc_context3(video_pcodec);
		avcodec_parameters_to_context(video_pcodec_ctx, video_pstream->codecpar);
		av_codec_set_pkt_timebase(video_pcodec_ctx, video_pstream->time_base);
		ret = avcodec_open2(video_pcodec_ctx, video_pcodec, NULL);
		if(ret){
			qDebug() << "open video decoder error.";
			cleanup();
			return -1;
		}
		AVStream *audio_pstream = pFormatCtx->streams[audioindex];
		AVCodec *audio_pcodec =  avcodec_find_decoder(audio_pstream->codecpar->codec_id);
		if(audio_pcodec == nullptr){
			qDebug() << "cant find the audio decoder.";
			cleanup();
			return -1;
		}
		audio_pcodec_ctx = avcodec_alloc_context3(audio_pcodec);
		avcodec_parameters_to_context(audio_pcodec_ctx, audio_pstream->codecpar);
		av_codec_set_pkt_timebase(audio_pcodec_ctx, audio_pstream->time_base);
		ret = avcodec_open2(audio_pcodec_ctx, audio_pcodec, NULL);
		if(ret){
			qDebug() << "open audio decoder error.";
			cleanup();
			return -1;
		}
		dst_width = out_width;
		dst_height = out_height;
		videoCallBack = p_videoCallBack;
		audioCallBack = p_audioCallBack;
		/*===========================================================================*/
		th_packet= thread([this](){
			int ret;
			double timeBase = av_q2d(pFormatCtx->streams[videoindex]->time_base);
			while(true){
				AVPacket *frame_pkt = av_packet_alloc();
				ret = av_read_frame(pFormatCtx, frame_pkt);
				if(ret < 0 || (!packet_keep_run))
					break;
				if(frame_pkt->stream_index == videoindex){
					unique_lock<mutex> lock(video_queue._lock);
					if(video_queue._queue.is_full())
						video_queue._cv.wait(lock, [this]{return !video_queue._queue.is_full();});
					video_queue._queue.push(frame_pkt);
					video_queue._cv.notify_all();
				}else if(frame_pkt->stream_index == audioindex){
					unique_lock<mutex> lock(audio_queue._lock);
					if(audio_queue._queue.is_full())
						audio_queue._cv.wait(lock, [this]{return !audio_queue._queue.is_full();});
					audio_queue._queue.push(frame_pkt);
					audio_queue._cv.notify_all();
				}
			}
		});
		th_video = thread([this](){
			int ret;
			double timeBase = av_q2d(pFormatCtx->streams[videoindex]->time_base);
			AVPacket *frame_pkt;
			AVFrame *pframeYUV = av_frame_alloc();
			AVFrame *pframeRGB = av_frame_alloc();
			SwsContext *pImgCvtCtx = sws_getContext(
					this->video_pcodec_ctx->width,
					this->video_pcodec_ctx->height,
					this->video_pcodec_ctx->pix_fmt,
					this->dst_width,
					this->dst_height,
					AV_PIX_FMT_RGB565LE,
					SWS_BICUBIC, NULL, NULL, NULL);
			while(video_keep_run){
				{
					unique_lock<mutex> lock(video_queue._lock);
					if(video_queue.is_empty())
						video_queue._cv.wait(lock, [this]{return !video_queue._queue.is_empty();});
					frame_pkt = video_queue._queue.front();
				}
				avcodec_send_packet(video_pcodec_ctx, frame_pkt);
				while(avcodec_receive_frame(this->video_pcodec_ctx, pframeYUV) == 0){
					auto img_rgb = (uint8_t *)malloc(sizeof(uint16_t)*this->dst_width*this->dst_height);
					av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, (const uint8_t *)img_rgb,
							
						
		});
	}
	int run(){
		video_keep_run = true;
		audio_keep_run = true;
		timestamp = 0;
#if 1
		th_video = thread([this](){
		int ret;
		double timeBase = av_q2d(this->pFormatCtx->streams[videoindex]->time_base);
			AVPacket *frame_pkt = av_packet_alloc();
			AVFrame *pframeYUV = av_frame_alloc();
			AVFrame *pframeRGB = av_frame_alloc();
			SwsContext *pImgCvtCtx = sws_getContext(
					this->video_pcodec_ctx->width,
					this->video_pcodec_ctx->height,
					this->video_pcodec_ctx->pix_fmt,
					this->dst_width,
					this->dst_height,
					AV_PIX_FMT_RGB565LE,
					SWS_BICUBIC, NULL, NULL, NULL);
            while(true){
                {
                    unique_lock<mutex> lock(frame_lock);
                    ret = av_read_frame(this->pFormatCtx, frame_pkt);
                }
                if(ret < 0 || (!this->video_keep_run))
                    break;
				if(frame_pkt->stream_index == videoindex){
                    double display_time = frame_pkt->pts * timeBase;
                    qDebug() << "get video frame pts " << frame_pkt->pts << " time " << display_time;
                    while(display_time > this->timestamp){ usleep(10e3); }
                    // qDebug() << "video_pstream->time_base " << video_pstream->time_base
					avcodec_send_packet(this->video_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(this->video_pcodec_ctx, pframeYUV) == 0){
						auto img_rgb = (uint8_t *)malloc(sizeof(uint16_t)*this->dst_width*this->dst_height);
						av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, (const uint8_t *)img_rgb,
								AV_PIX_FMT_RGB565BE, this->dst_width, this->dst_height, 1);
						sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
								pframeRGB->data, pframeRGB->linesize);
						//double tp = frame_pkt->pts*av_q2d(this->pFormatCtx->streams[videoindex]->time_base);
						if(videoCallBack)
                            videoCallBack(img_rgb, this->dst_width, this->dst_height);
						else
							free((void *)img_rgb);
						av_frame_unref(pframeYUV);
					}
					av_packet_unref(frame_pkt);
				}
			}
			sws_freeContext(pImgCvtCtx);
			av_frame_free(&pframeYUV);
			av_frame_free(&pframeRGB);
			av_packet_free(&frame_pkt);
		});
        qDebug() << "thread video decoder success.";
#endif
#if 1
        th_audio = thread([this](){
            int ret;
            double timeBase = av_q2d(this->pFormatCtx->streams[audioindex]->time_base);
			AVPacket *frame_pkt = av_packet_alloc();
			AVFrame *pframePCM = av_frame_alloc();
			int frame_size = av_get_bytes_per_sample(this->audio_pcodec_ctx->sample_fmt);
            while(true){
                {
                    unique_lock<mutex> lock(frame_lock);
                    ret = av_read_frame(this->pFormatCtx, frame_pkt);
                }
                if(ret < 0 || (!this->audio_keep_run))
                    break;
				if(frame_pkt->stream_index == audioindex){
                    double display_time = frame_pkt->pts * timeBase;
                    qDebug() << "get audio frame pts " << frame_pkt->pts << " time " << display_time;
                    this->timestamp = display_time;
					avcodec_send_packet(this->audio_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(this->audio_pcodec_ctx, pframePCM) == 0){
                        auto audio_pcm = (uint8_t*)malloc(frame_size*pframePCM->nb_samples*this->audio_pcodec_ctx->channels);
                        if(av_sample_fmt_is_planar(this->audio_pcodec_ctx->sample_fmt)){
                            for(int i=0; i<pframePCM->nb_samples; i++)
                                for(int k=0; k<this->audio_pcodec_ctx->channels; k++)
                                    memcpy(audio_pcm, pframePCM->data[k]+frame_size*i, frame_size);
                        }else{
                            for(int i=0; i<pframePCM->nb_samples; i++)
                                for(int k=0; k<this->audio_pcodec_ctx->channels; k++)
                                    memcpy(audio_pcm, pframePCM->data[0]+frame_size*(this->audio_pcodec_ctx->channels*i+k), frame_size);
                        }
                        usleep(60e3);
						if(audioCallBack)
                            audioCallBack(audio_pcm, pframePCM->nb_samples, this->audio_pcodec_ctx->channels);
						else
							free((void *)audio_pcm);
						av_frame_unref(pframePCM);
					}
				}
			}
			av_frame_free(&pframePCM);
			av_packet_free(&frame_pkt);
		});
        qDebug() << "thread audio decoder success.";
#endif
		return 0;
	}
};

}

#endif
