#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <QThread>
#include <QImage>
#include <QDebug>
#include <QString>
#include <iostream>
#include <string>
#include <QQuickImageProvider>

#include "fplayer.h"

using std::endl;
using std::string;
/*===================================================================================*/
class mediaPlayer : public QObject{
    Q_OBJECT
    Q_ENUMS(CMD_KEY)
public:
    enum CMD_KEY{
        V_START,
        V_PAUSE,
        V_RESUME,
        V_STOP
    };
    mediaPlayer(QObject *parent = nullptr);
    ~mediaPlayer();
    Q_INVOKABLE int open(const QString a, int width, int height);
    Q_INVOKABLE int command(CMD_KEY key);
signals:
    void refreshScrean(QImage img);
signals:
    void videoExit(int flag); // flag not use, default 0
protected:

private:
    karl::fplayer *player;
    void video_callback(uint8_t *data, int width, int height);
    void audio_callback(uint8_t *data, int length, int channel);
    void exit_callback();
};
/*===================================================================================*/

#endif // MEDIA_PLAYER_H
