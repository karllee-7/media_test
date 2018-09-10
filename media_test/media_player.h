#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <QThread>
#include <QImage>
#include <QDebug>
#include <QString>
#include <iostream>
#include <string>
#include <QQuickImageProvider>

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

using std::endl;
using std::string;
/*===================================================================================*/
class mediaPlayer : public QThread{
    Q_OBJECT
    Q_PROPERTY(QString file_name READ rd_file_name WRITE wt_file_name)
    Q_PROPERTY(int dst_width READ rd_dst_width WRITE wt_dst_width)
    Q_PROPERTY(int dst_height READ rd_dst_height WRITE wt_dst_height)
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
    QString rd_file_name() const;
    void wt_file_name(const QString a);
    int rd_dst_width() const;
    void wt_dst_width(const int a);
    int rd_dst_height() const;
    void wt_dst_height(const int a);
    Q_INVOKABLE int open();
    Q_INVOKABLE int open(const QString a);
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
class vError : public std::exception
{
private:
        int err_;
        const char * errStr_;
public:
        vError(int err, const char * errStr) : err_(err), errStr_(errStr){}

        ~vError() throw() {}

        const char * what() const throw (){
                return errStr_;
        }

        int err(void) const { return err_; }
};
/*===================================================================================*/

#endif // MEDIA_PLAYER_H
