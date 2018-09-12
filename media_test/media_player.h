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
    fplayer *player;
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
    void run() override;
private:
    QString file_name;
    struct {
        std::atomic<bool> stop;
        std::atomic<bool> pause;
        std::atomic<bool> resume;
    }request;
    struct {
        std::atomic<bool> started;
        std::atomic<bool> running;
    }status;
    double time_start_s;
    int dst_width;
    int dst_height;
};
/*===================================================================================*/

#endif // MEDIA_PLAYER_H
