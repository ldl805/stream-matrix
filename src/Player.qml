import QtQml 2.12
import QtQuick 2.12
import QtMultimedia 5.12
import StreamMatrix.Multimedia 1.0
import StreamMatrix.Themes 1.0

FocusScope {
    id: root

    property string color: "black"
    property string cameraTitle: ""
    property bool showDiagnostics: false

    property var avOptions: ({})

    property alias loops: qmlAvPlayer.loops
    property alias source: qmlAvPlayer.source
    property alias muted: qmlAvPlayer.muted
    property alias volume: qmlAvPlayer.volume
    readonly property alias hasAudio: qmlAvPlayer.hasAudio
    readonly property alias hasVideo: qmlAvPlayer.hasVideo
    readonly property alias fps: qmlAvPlayer.fps
    readonly property alias bitrate: qmlAvPlayer.bitrate
    readonly property alias videoCodec: qmlAvPlayer.videoCodec
    readonly property alias videoResolution: qmlAvPlayer.videoResolution
    readonly property alias isHWAccelerated: qmlAvPlayer.isHWAccelerated
    readonly property alias reconnecting: qmlAvPlayer.reconnecting
    readonly property alias reconnectAttempt: qmlAvPlayer.reconnectAttempt

    onVisibleChanged: {
        if (visible) {
            if (!timer.running) {
                timer.start();
            }
        } else {
            timer.stop();
            qmlAvPlayer.autoPlay = false;
            qmlAvPlayer.stop();
        }
    }
    Component.onCompleted: {
        if (visible) {
            timer.start();
        }
    }

    Timer {
        id: timer

        interval: 50

        onTriggered: {
            if (root.visible) {
                qmlAvPlayer.autoPlay = true;
            }
        }
    }

    Rectangle {
        color: root.color
        border.color: "#101010"
        anchors.fill: parent

        VideoOutput {
            id: videoOutput

            source: qmlAvPlayer
            anchors.fill: parent
        }

        // Center Message (Loading, End of media, etc.)
        Text {
            id: message

            color: "white"
            font.pointSize: 11
            visible: qmlAvPlayer.status !== MediaPlayer.Buffered && !qmlAvPlayer.reconnecting
            anchors.centerIn: parent
        }

        // Reconnecting Badge Overlay
        Rectangle {
            id: reconnectBadge
            visible: qmlAvPlayer.reconnecting
            anchors.centerIn: parent
            width: reconnectLayout.implicitWidth + 24
            height: reconnectLayout.implicitHeight + 16
            radius: 6
            color: "#CC181818"
            border.color: "#FF9900"
            border.width: 1

            Column {
                id: reconnectLayout
                anchors.centerIn: parent
                spacing: 6

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    Text {
                        text: "🔄"
                        font.pointSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 1200
                            loops: Animation.Infinite
                            running: qmlAvPlayer.reconnecting
                        }
                    }

                    Text {
                        text: qsTr("Reconnecting (attempt %1)...").arg(qmlAvPlayer.reconnectAttempt)
                        color: "#FFCC00"
                        font.bold: true
                        font.pointSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    width: 70
                    height: 22
                    radius: 3
                    color: "#333333"
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: qsTr("Retry Now")
                        color: "white"
                        font.pointSize: 8
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: qmlAvPlayer.retry()
                    }
                }
            }
        }

        // Camera Title Badge (Top-Left)
        Rectangle {
            id: titleBadge
            visible: root.cameraTitle.length > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 6
            height: titleText.implicitHeight + 6
            width: titleText.implicitWidth + 12
            radius: 3
            color: "#80000000"

            Text {
                id: titleText
                anchors.centerIn: parent
                text: root.cameraTitle
                color: "#EEEEEE"
                font.pointSize: 9
                font.bold: true
            }
        }

        // Stream Diagnostics HUD (Top-Right / Bottom-Left)
        Rectangle {
            id: diagnosticsHud
            visible: root.showDiagnostics && qmlAvPlayer.hasVideo
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 6
            height: diagText.implicitHeight + 6
            width: diagText.implicitWidth + 12
            radius: 3
            color: "#99000000"
            border.color: "#444444"
            border.width: 1

            Text {
                id: diagText
                anchors.centerIn: parent
                color: "#00FF66"
                font.pointSize: 8
                font.family: "Monospace"
                text: {
                    var parts = [];
                    if (qmlAvPlayer.fps > 0) parts.push(qmlAvPlayer.fps.toFixed(1) + " FPS");
                    if (qmlAvPlayer.videoResolution) parts.push(qmlAvPlayer.videoResolution);
                    if (qmlAvPlayer.videoCodec) parts.push(qmlAvPlayer.videoCodec.toUpperCase());
                    if (qmlAvPlayer.isHWAccelerated) parts.push("[HW]");
                    if (qmlAvPlayer.bitrate > 0) parts.push(qmlAvPlayer.bitrate + " kbps");
                    return parts.join(" | ");
                }
            }
        }

        QmlAVPlayer {
            id: qmlAvPlayer

            autoLoad: false
            autoReconnect: true

            avOptions: {
                var avOptions = root.avOptions;
                Object.assignDefault(avOptions, layoutsCollectionSettings.toJSValue("defaultAVFormatOptions"));
                return avOptions;
            }

            onStatusChanged: {
                switch (status) {
                case MediaPlayer.NoMedia:
                    message.text = qsTr("No media");
                    break;
                case MediaPlayer.Loading:
                    message.text = qsTr("Connecting...");
                    break;
                case MediaPlayer.Loaded:
                    message.text = qsTr("Loaded");
                    break;
                case MediaPlayer.Buffering:
                    break;
                case MediaPlayer.Stalled:
                    message.text = qsTr("Stalled");
                    break;
                case MediaPlayer.Buffered:
                    break;
                case MediaPlayer.EndOfMedia:
                    message.text = qsTr("End of media");
                    break;
                case MediaPlayer.InvalidMedia:
                    message.text = qsTr("Connection error");
                    break;
                case MediaPlayer.UnknownStatus:
                    break;
                }
            }

            onBufferProgressChanged: {
                message.text = qsTr("Buffering %1%").arg(Math.round(bufferProgress * 100));
            }
        }
    }

    function play() { qmlAvPlayer.play(); }
    function retry() { qmlAvPlayer.retry(); }
    function stop() { qmlAvPlayer.stop(); }
}
