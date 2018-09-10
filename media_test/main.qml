import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import karl.media 1.0

Window {
    visible: true
    width: 800
    height: 480
    title: qsTr("Hello World")

    MediaPlayer {
        id: mply
        dst_width: 640
        dst_height: 480
        // file_name: "H249_21.mp4"
    }

    Button {
        id: ctrl_paly
        text: "\u25AE"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        height: parent.height * 0.1
        width: height
    }
    Slider {
        id: ctrl_process
        anchors.left: ctrl_paly.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.1
    }
    Image {
        id: video_show
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: ctrl_process.top
    }
    Connections {
        target: ctrl_paly
        onClicked:{
            if(ctrl_paly.text == "\u25AE"){
                mply.command(MediaPlayer.V_PAUSE)
                ctrl_paly.text = "\u25B6"
            }else{
                mply.command(MediaPlayer.V_RESUME)
                ctrl_paly.text = "\u25AE"
            }
        }
    }
    Component.onCompleted: {
        mply.open("/home/karllee/temp/video_test.mp4")
        //video_show.source = 'http://img.pconline.com.cn/images/bbs4/200910/29/1256757186135.jpg'
    }
    Connections {
        target: mply
        onRefreshScrean:{
            //console.debug("qml get one")
            xImageProvider_0.setImage(img)
        }
    }
    Connections{
        target: xImageProvider_0
        onImageChanged:{
            video_show.source = "image://xProvider_0/"+Math.random()
        }
    }
}
