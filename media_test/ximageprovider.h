#ifndef XIMAGEPROVIDER_H
#define XIMAGEPROVIDER_H

#include <QImage>
#include <QDebug>
#include <QQuickImageProvider>

class XProvider : public QQuickImageProvider{
public:
    XProvider()
        : QQuickImageProvider(QQuickImageProvider::Image){

    }
    QImage requestImage(const QString &, QSize *, const QSize &){
        return this->img;
    }
    QImage img;
};

class XImageProvider : public QObject{
    Q_OBJECT
public:
    XProvider * provider;
    XImageProvider(QObject* parent = nullptr):QObject(parent){
        provider = new XProvider();
    }
    ~XImageProvider(){
        delete provider;
    }
    Q_INVOKABLE void setImage(QImage img){
        provider->img = img;
        //qDebug() << "XImageProvider set image." << endl;
        emit imageChanged();
    }
signals:
    void imageChanged();
};

#endif // XIMAGEPROVIDER_H
