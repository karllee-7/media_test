#include "media_player.h"
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <functional>
#include <iostream>
#include <QDebug>

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::cout;
using std::endl;

void myImageCleanupHandler(void *info){
    free(info);
}
mediaPlayer::mediaPlayer(QObject *parent):QObject(parent){
    player = new karl::fplayer();
}
mediaPlayer::~mediaPlayer(){
    delete player;
}
void mediaPlayer::video_callback(uint8_t *data, int width, int height){
	if(data){
        QImage img(data, width, height, QImage::Format_RGB16, myImageCleanupHandler, (void*)data);
        emit refreshScrean(img);
	}
    //qDebug() << "video_callback width " << width << " height " << height << endl;
}
void mediaPlayer::audio_callback(uint8_t *data, int length, int channel){
	if(data)
        free(data);
    //qDebug() << "audio_callback length " << length << " channel " << channel;
}
void mediaPlayer::exit_callback(){
    qDebug() << "fplayer exit.";
}
int mediaPlayer::open(const QString a, int width, int height){
    int re;
    karl::fplayerVideoCallBack f0 = std::bind(&mediaPlayer::video_callback, this, _1, _2, _3);
    karl::fplayerAudioCallBack f1 = std::bind(&mediaPlayer::audio_callback, this, _1, _2, _3);
    karl::fplayerExitCallBack f2 = std::bind(&mediaPlayer::exit_callback, this);
    re = player->play(a.toStdString(), width, height, f0, NULL, f2);
    if(re){
        qDebug() << "player open error";
        return -1;
    }
    return 0;
}
int mediaPlayer::command(CMD_KEY key){
    switch(key){
    case V_START:
        ;
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

