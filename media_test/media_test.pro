QT += quick
CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
    media_player.cpp

RESOURCES += qml.qrc


INCLUDEPATH += /opt/ffmpeg-3.4.2/include
INCLUDEPATH += /opt/alsa-lib-1.1.6/include
#INCLUDEPATH += /opt/ffmpeg-3.4.2-armhf-4.9.4/include
#INCLUDEPATH += /opt/cedarx-12.06.2015-armhf-4.9.4/include/

LIBS += -L/opt/ffmpeg-3.4.2/lib
LIBS += -L/opt/alsa-lib-1.1.6/lib
#LIBS += -L/opt/ffmpeg-3.4.2-armhf-4.9.4/lib
LIBS += -lavformat -lavcodec -lavutil -lswscale -lswresample -lasound -lpthread
#LIBS += -L/opt/cedarx-12.06.2015-armhf-4.9.4/lib
#LIBS += -lcedar_vdecoder -lcedar_common -lcedar_base

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    media_player.h \
    ximageprovider.h \
    alsa_intf.h \
    fplayer.h
