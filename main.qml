import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtMultimedia 5.12
import org.tal.buzz 1.0

ApplicationWindow {
    visible: true
    width: 800
    height: 640
    title: qsTr("Buzz!")

    property int tsec: 10

    property int playerTurn: 0

    property variant playerStats;

    Component.onCompleted: {
        gb.start("/dev/input/event18");
        //gb.start("");

        clearScores();
    }

    SoundEffect {
        id: audioBuzz
        source: "sounds/button.wav"
    }

    SoundEffect {
        id: audioCorrect
        source: "sounds/bubble.wav"
    }

    SoundEffect {
        id: audioWrong
        source: "sounds/hithard.wav"
    }

    SoundEffect {
        id: audioBad
        source: "sounds/hit2.wav"
    }

    GameBuzz {
        id: gb

        onButton: {
            console.debug("Key("+ player + "): "+color)
            if (player==playerTurn) {
                var a=m.get(color)
                console.debug(a)

                if (a.correct) {
                    console.debug("Correct!")
                    playerStats[player]++;
                    rightAnswer.visible=true;
                    playerTurn=0;
                    audioCorrect.play();
                } else {
                    console.debug("Wrong!")
                    wrongAnswer.visible=true;
                    playerTurn=0;
                    audioWrong.play()
                }

            } else {
                console.debug("Not your turn player "+player)
                audioBad.play()
            }
        }

        onBuzzer: {
            console.debug("BUZZ: "+ player)
            if (t.running) {
                t.stop()
                playerBuzzed.text="Player: "+player
                playerTurn=player
                gb.led(player, true);
                audioBuzz.play();
            } else if (playerTurn==0) {
                startRound();
            } else {
                console.debug("No time to buzz now")
            }
        }

        onHasDeviceChanged: {
            if (!hasDevice) {
                console.debug("No buzzer found")
            }
        }

        function setAllLeds(blink) {
            gb.led(1, blink)
            gb.led(2, blink)
            gb.led(3, blink)
            gb.led(4, blink)
        }

        Component.onCompleted: {
            setAllLeds(false);
        }

        Component.onDestruction: {
            setAllLeds(false);
        }

    }

    function clearScores() {
        playerStats=Array();
        playerStats[1]=0;
        playerStats[2]=0;
        playerStats[3]=0;
        playerStats[4]=0;
    }

    function startRound() {
        tsec=10;
        playerTurn=0;
        rightAnswer.visible=false;
        wrongAnswer.visible=false;
        t.start();
    }

    ColumnLayout {
        id: cl
        anchors.fill: parent
        spacing: 8
        Rectangle {
            id: countDown
            Layout.fillWidth: true
            height: 150
            color: t.running ? "green" : "cyan"

            Text {
                visible: t.running
                anchors.centerIn: parent
                text: tsec
                font.pixelSize: 60
            }

            Text {
                visible: !t.running  && playerTurn==0
                anchors.centerIn: parent
                text: "Buzz to start!"
                font.pixelSize: 60
            }

            Text {
                id: playerBuzzed
                anchors.centerIn: parent
                visible: !t.running && playerTurn>0
                font.pixelSize: 60
                text: ""
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    startRound();
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 100
            color: "green"
            visible: (playerTurn==0 && tsec>0 && t.running) || playerTurn>0
            Text {
                id: questions
                anchors.centerIn: parent
                font.pixelSize: 42
                text: "This is the question!"
            }
        }
        Rectangle {
            visible: tsec==0 && playerTurn==0
            Layout.fillWidth: true
            height: 120
            color: "red"
            Text {
                id: toslow
                anchors.centerIn: parent
                font.pixelSize: 48
                text: "Too slow!"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !t.running && tsec>0 && playerTurn>0
            Column {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                Repeater {
                    anchors.fill: parent
                    model: m
                    delegate: Row {
                        spacing: 12
                        Rectangle {  width: 50; height: 50; color: getButtonColor(index) }
                        Text { text: answer; height: 50; font.pixelSize: 42 }
                    }
                }
            }
        }
    }

    Rectangle {
        id: rightAnswer
        anchors.centerIn: parent
        color: "green"
        width: parent.width/2
        height: parent.height/2
        visible: false
        Text {
            anchors.centerIn: parent
            font.pixelSize: 48
            text: "That is correct!"
        }
    }

    Rectangle {
        id: wrongAnswer
        anchors.centerIn: parent
        color: "red"
        width: parent.width/2
        height: parent.height/2
        visible: false
        Text {
            anchors.centerIn: parent
            font.pixelSize: 48
            text: "That is wrong!"
        }
    }

    function getButtonColor(c) {
        switch (c) {
        case 0:
            return "blue"
        case 1:
            return "orange"
        case 2:
            return "green"
        case 3:
            return "yellow"
        }
        return ""
    }

    ListModel {
        id: m

        ListElement {
            correct: false;
            answer: "This answer for blue"
        }
        ListElement {
            correct: false;
            answer: "This answer for orange"
        }
        ListElement {
            correct: true;
            answer: "This answer for green"
        }
        ListElement {
            correct: false;
            answer: "This answer for yellow"
        }
    }

    Timer {
        id: t
        interval: 1000
        repeat: true
        onTriggered: {
            tsec--;
            if (tsec==0) {
                playerTurn=0;
                t.stop()
            }
        }
    }

    Timer {
        id: blinker
        interval: 200
        repeat: true
        running: t.running
        triggeredOnStart: true

        property bool blink: true;

        onTriggered: {
            blink=!blink;
            gb.setAllLeds(blink)
        }

        onRunningChanged: {
            gb.setAllLeds(running);
        }
    }

}
