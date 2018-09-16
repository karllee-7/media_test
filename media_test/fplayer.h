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
#include <string>
#include <QDebug>
#include "karl_thread.h"
//#include "alsa_ctrl.h"
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

namespace karl{
using std::thread;
using std::atomic;
using std::mutex;
using std::unique_lock;
using std::string;
using std::cout;
using std::endl;
/*===================================================================================*/
typedef std::function<void(uint8_t*, int, int)> fplayerVideoCallBack;
typedef std::function<void(uint8_t*, int, int)> fplayerAudioCallBack;
typedef std::function<void()> fplayerExitCallBack;
/*===================================================================================*/
class fplayer{
    //AVFormatContext *format_ctx;
    //AVCodecContext *video_codec_ctx;
    //AVCodecContext *audio_codec_ctx;
    //fplayerVideoCallBack videoCallBack;
    //fplayerAudioCallBack audioCallBack;
    //thread th_packet;
    //thread th_video;
    //thread th_audio;
    thread th_core;
    atomic<bool> packet_keep_run;
    atomic<bool> video_keep_run;
    atomic<bool> audio_keep_run;
    karl_queue<AVPacket*> video_queue;
    karl_queue<AVPacket*> audio_queue;
    karl_protected<double> timestamp;
public:
    fplayer();
    ~fplayer();
    void take_packet(AVFormatContext *format_ctx,
		    int videoindex, int audioindex);
    void decode_video(AVFormatContext *format_ctx,
                        AVCodecContext *video_codec_ctx,
                        int videoindex,
                        int width, int height,
                        fplayerVideoCallBack videoCallBack);
    void decode_audio(AVFormatContext *format_ctx,
                        AVCodecContext *audio_codec_ctx,
                        int audioindex,
                        fplayerAudioCallBack audioCallBack);
    int core(string filename,
		    int width, int height, 
                    fplayerVideoCallBack p_videoCallBack,
                    fplayerAudioCallBack p_audioCallBack,
		    fplayerExitCallBack exitCallBack);
    int play(string filename,
		    int width, int height, 
                    fplayerVideoCallBack p_videoCallBack,
                    fplayerAudioCallBack p_audioCallBack,
		    fplayerExitCallBack exitCallBack);
    snd_pcm_t *pcm_open(unsigned int channels, unsigned int rate, unsigned int period_size);
};

}

#endif
