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
}
void mediaPlayer::exit_callback(int flag){
    emit videoExit(flag);
    qDebug() << "fplayer exit_callback.";
}
int mediaPlayer::open(const QString a, int width, int height){
    int re;
    karl::fplayerVideoCallBack f0 = std::bind(&mediaPlayer::video_callback, this, _1, _2, _3);
    karl::fplayerExitCallBack f2 = std::bind(&mediaPlayer::exit_callback, this, _1);
    re = player->play(a.toStdString(), width, height, f0, NULL, f2);
    if(re){
        qDebug() << "player open error";
        return -1;
    }
    return 0;
}
int mediaPlayer::command(CMD_KEY key){
    switch(key){
    case V_PAUSE:
        player->set_pause();
        break;
    case V_RESUME:
        player->set_resume();
        break;
    case V_MUTE:
        player->set_mute();
        break;
    case V_UNMUTE:
        player->set_unmute();
        break;
    case V_STOP:
        player->set_stop();
        break;
    }
    return 0;
}

