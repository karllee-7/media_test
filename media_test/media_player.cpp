#include "media_player.h"
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <functional>
#include <iostream>
#include <QDebug>

using std::bind;
using std::placeholders::_1;
using std::cout;
using std::endl;

mediaPlayer::mediaPlayer(QObject *parent):QObject(parent){
    player = new karl::fplayer();
}
mediaPlayer::~mediaPlayer(){
    delete player;
}
void mediaPlayer::video_callback(uint8_t *data){
	if(data){
		// emit refreshScrean();
        free(data);
	}
	qDebug() << "video_callback" << endl;
}
void mediaPlayer::audio_callback(uint8_t *data){
	if(data)
        free(data);
	qDebug() << "audio_callback" << endl;
}
    
int mediaPlayer::open(const QString a, int width, int height){
    int re;
    karl::fplayerCallBack f0 = std::bind(&mediaPlayer::video_callback, this, _1);
    re = player->open(a.toStdString().c_str(), width, height, f0, NULL);
    if(re){
        qDebug() << "player open error";
        return -1;
    }
    re = player->run();
    if(re){
        qDebug() << "player run error";
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

