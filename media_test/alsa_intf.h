#ifndef ALSA_INTF_H
#define ALSA_INTF_H

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

class alsa_intf{

public:
    snd_pcm_t *handle;
    alsa_intf(const char* pcm_name="default",
              snd_pcm_format_t format=SND_PCM_FORMAT_S16_LE,
              uint32_t channel=2,
              uint32_t rate=44100,
              snd_pcm_uframes_t period_size=32){
        int re;
        int dir;
        handle = NULL;
        re = snd_pcm_open(&handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0);
        if (re < 0){
            printf("unable to open pcm device: %s\n",snd_strerror(re));
            handle = NULL;
            return ;
        }
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(handle, params);
        snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(handle, params, format);
        snd_pcm_hw_params_set_channels(handle, params, channel);
        snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);
        snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, &dir);
        re = snd_pcm_hw_params(handle, params);
        if (re < 0) {
            printf("unable to set hw parameters: %s\n", snd_strerror(re));
            snd_pcm_close(handle);
            return;
        }
    }
    ~alsa_intf(){
        if(handle != NULL)
            snd_pcm_close(handle);
    }
    int write(const void * buffer, snd_pcm_uframes_t frames){
        return snd_pcm_writei(handle, buffer, frames);
    }
};

#endif // ALSA_INTF_H
