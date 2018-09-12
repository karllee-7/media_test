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
#ifdef __cplusplus
};
#endif

#include <thread>

using std::thread;

class fplayer{
	int dst_width;
	int dst_height;
	AVFormatContext *pFormatCtx;
	void (*videoCallBack)(const uint8_t *ptr);
	void (*audioCallBack)(const uint8_t *ptr);
	int videoindex;
	int audioindex;
	AVCodecContext *video_pcodec_ctx;
	AVCodecContext *audio_pcodec_ctx;
	thread th_video;
	thread th_audio;
public:
	fplayer(){
		pFormatCtx = NULL;
		videoCallBack = NULL;
		audioCallBack = NULL;
		videoindex = 0;
		audioindex = 0;
		video_pcodec_ctx = NULL;
		audio_pcodec_ctx = NULL;
		th_video = NULL;
		th_audio = NULL;
	}
	~fplayer(){
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
		// status.started = false;
	}
	int open(const char* name, int out_width, int out_height, void (*videoCallBack)(void *ptr), 
			void (*audioCallBack)(void* ptr)){
		int ret;
		av_register_all();
		try{
			pFormatCtx = avformat_alloc_context();
			ret = avformat_open_input(&pFormatCtx, name, NULL, NULL);
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
				av_dump_format(pFormatCtx, videoindex, name, 0);
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
				av_dump_format(pFormatCtx, audioindex, name, 0);
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
			dst_width = out_width;
			dst_height = out_height;
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
			//status.started = false;
			return;
		}
	}
	int run(){
		th_video = thread([&video_pcodec_ctx, dst_width, dst_height](){
			AVPacket *frame_pkt = av_packet_alloc();
			AVFrame *pframeYUV = av_frame_alloc();
			AVFrame *pframeRGB = av_frame_alloc();
			SwsContext *pImgCvtCtx = sws_getContext(
				video_pcodec_ctx->width,
				video_pcodec_ctx->height,
				video_pcodec_ctx->pix_fmt,
				dst_width,
				dst_height,
				AV_PIX_FMT_RGB565LE,
				SWS_BICUBIC, NULL, NULL, NULL);
			while(av_read_frame(pFormatCtx, frame_pkt) >= 0){
				if(frame_pkt->stream_index == videoindex){
					cout << "get video frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
					// qDebug() << "video_pstream->time_base " << video_pstream->time_base
					avcodec_send_packet(video_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(video_pcodec_ctx, pframeYUV) == 0){
						const uint8_t *img_rgb = malloc(sizeof(uint16_t)*dst_width*dst_height);
						av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, img_rgb,
							AV_PIX_FMT_RGB565BE, dst_width, dst_height, 1);
						sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
						pframeRGB->data, pframeRGB->linesize);
						//double tp = frame_pkt->pts*av_q2d(pFormatCtx->streams[videoindex]->time_base);
						//usleep(60e3);
						if(videoCallBack)
							videoCallBack(img_rgb);
						else
							free(img_rgb);
						av_frame_unref(pframeYUV);
					}
					av_packet_unref(frame_pkt);
				}else if(frame_pkt->stream_index == audioindex){
					qDebug() << "get audio frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
					avcodec_send_packet(audio_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(audio_pcodec_ctx, pframePCM) == 0){
						if(audioCallBack)
							;//audioCallBack(NULL);
						else
							;
						av_frame_unref(pframePCM);
					}
				}else{
					//qDebug("drop channel %d frame.", frame_pkt->stream_index);
				}
			}
			sws_freeContext(pImgCvtCtx);
			av_frame_free(&pframePCM);
			av_frame_free(&pframeYUV);
			av_frame_free(&pframeRGB);
			av_packet_free(&frame_pkt);
		});
		th_audio = thread([](){
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
			//status.running = true;
			time_start_s = get_curr_time_s();
			while(av_read_frame(pFormatCtx, frame_pkt) >= 0){
				if(frame_pkt->stream_index == videoindex){
					qDebug() << "get video frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
					// qDebug() << "video_pstream->time_base " << video_pstream->time_base
					avcodec_send_packet(video_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(video_pcodec_ctx, pframeYUV) == 0){
						const uint8_t *img_rgb = malloc(sizeof(uint16_t)*dst_width*dst_height);
						av_image_fill_arrays(pframeRGB->data, pframeRGB->linesize, img_rgb,
							AV_PIX_FMT_RGB565BE, dst_width, dst_height, 1);
						sws_scale(pImgCvtCtx, pframeYUV->data, pframeYUV->linesize, 0, pframeYUV->height,
						pframeRGB->data, pframeRGB->linesize);
						//double tp = frame_pkt->pts*av_q2d(pFormatCtx->streams[videoindex]->time_base);
						//usleep(60e3);
						if(videoCallBack)
							videoCallBack(img_rgb);
						else
							free(img_rgb);
						av_frame_unref(pframeYUV);
					}
					av_packet_unref(frame_pkt);
				}else if(frame_pkt->stream_index == audioindex){
					qDebug() << "get audio frame pts " << frame_pkt->pts*av_q2d(pFormatCtx->streams[frame_pkt->stream_index]->time_base) << endl;
					avcodec_send_packet(audio_pcodec_ctx, frame_pkt);
					while(avcodec_receive_frame(audio_pcodec_ctx, pframePCM) == 0){
						if(audioCallBack)
							;//audioCallBack(NULL);
						else
							;
						av_frame_unref(pframePCM);
					}
				}else{
					//qDebug("drop channel %d frame.", frame_pkt->stream_index);
				}
			}
			sws_freeContext(pImgCvtCtx);
			av_frame_free(&pframePCM);
			av_frame_free(&pframeYUV);
			av_frame_free(&pframeRGB);
			av_packet_free(&frame_pkt);
		});
	}
};


#endif
