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
    property alias autoReconnect: qmlAvPlayer.autoReconnect
    readonly property alias reconnecting: qmlAvPlayer.reconnecting
    readonly property alias reconnectAttempt: qmlAvPlayer.reconnectAttempt
    readonly property alias errorString: qmlAvPlayer.errorString
    readonly property alias reconnectDelayMs: qmlAvPlayer.reconnectDelayMs

    property int reconnectCountdown: 0
    property bool manualRetryPending: false

    Connections {
        target: qmlAvPlayer
        onReconnectDelayMsChanged: {
            if (qmlAvPlayer.reconnectDelayMs > 0) {
                root.reconnectCountdown = Math.ceil(qmlAvPlayer.reconnectDelayMs / 1000);
                countdownTimer.restart();
            } else {
                root.reconnectCountdown = 0;
                countdownTimer.stop();
            }
        }
        onStatusChanged: {
            if (qmlAvPlayer.status !== MediaPlayer.Loading && !resetManualRetryTimer.running) {
                root.manualRetryPending = false;
            }
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        running: false
        onTriggered: {
            if (root.reconnectCountdown > 1) {
                root.reconnectCountdown -= 1;
            } else {
                root.reconnectCountdown = 0;
                stop();
            }
        }
    }

    Timer {
        id: resetManualRetryTimer
        interval: 800
        onTriggered: {
            if (qmlAvPlayer.status !== MediaPlayer.Loading) {
                root.manualRetryPending = false;
            }
        }
    }

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
            visible: qmlAvPlayer.status !== MediaPlayer.Buffered && !qmlAvPlayer.reconnecting && qmlAvPlayer.status !== MediaPlayer.InvalidMedia
            anchors.centerIn: parent
        }

        // Reconnecting & Stream Error Overlay
        Rectangle {
            id: reconnectOverlay
            visible: (qmlAvPlayer.reconnecting || qmlAvPlayer.status === MediaPlayer.InvalidMedia) && !qmlAvPlayer.hasVideo
            anchors.centerIn: parent
            width: Math.min(parent.width - 16, Math.max(210, reconnectLayout.implicitWidth + 32))
            height: reconnectLayout.implicitHeight + 20
            radius: 8
            color: "#E61A1D24"
            border.color: isConnecting ? "#388BFD" : (qmlAvPlayer.errorString.length > 0 ? "#D94848" : "#E3B341")
            border.width: 1

            readonly property bool isConnecting: qmlAvPlayer.status === MediaPlayer.Loading || root.manualRetryPending

            Column {
                id: reconnectLayout
                anchors.centerIn: parent
                spacing: 8
                width: parent.width - 24

                // Header with Status Icon & Main Title
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    Text {
                        text: reconnectOverlay.isConnecting ? "🔄" : (qmlAvPlayer.errorString.length > 0 ? "⚠️" : "⏳")
                        font.pointSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                            running: reconnectOverlay.isConnecting
                        }
                    }

                    Text {
                        text: {
                            if (reconnectOverlay.isConnecting) {
                                return qsTr("Connecting (attempt %1)...").arg(Math.max(1, qmlAvPlayer.reconnectAttempt));
                            } else if (root.reconnectCountdown > 0) {
                                return qsTr("Retrying in %1s (attempt %2)...").arg(root.reconnectCountdown).arg(Math.max(1, qmlAvPlayer.reconnectAttempt));
                            } else {
                                return qsTr("Stream Offline (attempt %1)").arg(Math.max(1, qmlAvPlayer.reconnectAttempt));
                            }
                        }
                        color: reconnectOverlay.isConnecting ? "#58A6FF" : "#F0883E"
                        font.bold: true
                        font.pointSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Subtitle / Error Detail
                Text {
                    id: statusDetailText
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    wrapMode: Text.WrapAnywhere
                    elide: Text.ElideRight
                    font.pointSize: 8
                    text: {
                        if (reconnectOverlay.isConnecting) {
                            return qsTr("Attempting connection to stream...");
                        } else if (qmlAvPlayer.errorString.length > 0) {
                            return qsTr("Error: %1").arg(qmlAvPlayer.errorString);
                        } else {
                            return qsTr("Connection lost. Waiting to retry...");
                        }
                    }
                    color: reconnectOverlay.isConnecting ? "#8B949E" : (qmlAvPlayer.errorString.length > 0 ? "#FF7B72" : "#8B949E")
                }

                // Interactive "Retry Now" / "Retrying..." Button
                Rectangle {
                    id: retryButton
                    width: Math.max(90, retryBtnContent.implicitWidth + 24)
                    height: 26
                    radius: 4
                    anchors.horizontalCenter: parent.horizontalCenter

                    readonly property bool isBusy: reconnectOverlay.isConnecting

                    color: {
                        if (isBusy) return "#1F3A5C";
                        if (retryMouseArea.pressed) return "#1F56A3";
                        if (retryMouseArea.containsMouse) return "#3A4553";
                        return "#2C313A";
                    }

                    border.color: {
                        if (isBusy) return "#388BFD";
                        if (retryMouseArea.pressed || retryMouseArea.containsMouse) return "#58A6FF";
                        return "#484F58";
                    }
                    border.width: 1

                    scale: retryMouseArea.pressed && !isBusy ? 0.95 : 1.0
                    Behavior on scale { NumberAnimation { duration: 80 } }
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    Row {
                        id: retryBtnContent
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "🔄"
                            font.pointSize: 8
                            visible: retryButton.isBusy
                            anchors.verticalCenter: parent.verticalCenter
                            RotationAnimator on rotation {
                                from: 0
                                to: 360
                                duration: 800
                                loops: Animation.Infinite
                                running: retryButton.isBusy
                            }
                        }

                        Text {
                            id: retryBtnText
                            text: retryButton.isBusy ? qsTr("Retrying...") : qsTr("Retry Now")
                            color: retryButton.isBusy ? "#79C0FF" : (retryMouseArea.containsMouse ? "#FFFFFF" : "#C9D1D9")
                            font.pointSize: 8
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: retryMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: retryButton.isBusy ? Qt.ArrowCursor : Qt.PointingHandCursor
                        enabled: !retryButton.isBusy

                        onClicked: {
                            root.manualRetryPending = true;
                            resetManualRetryTimer.restart();
                            qmlAvPlayer.retry();
                        }
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
            autoReconnect: typeof viewportSettings !== "undefined" ? viewportSettings.autoReconnect : true

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
