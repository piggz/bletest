import QtQuick 2.9
import QtQuick.Controls 2.2
import QtBluetooth 5.2

Page {
    width: 600
    height: 400

    header: Label {
        text: qsTr("Page 1")
        font.pixelSize: Qt.application.font.pixelSize * 2
        padding: 10
    }

    Column {
    Button {
        text: "Scan/Pair"
        onClicked: {
            LEInterface.startScan();
        }
    }

    Button {
        text: "Read Info"
        onClicked: {
            LEInterface.requestDeviceInfo();
        }
    }
    }

}
