import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control
import Linphone
import UtilsCpp
import SettingsCpp
import "qrc:/qt/qml/Linphone/view/Style/buttonStyle.js" as ButtonStyle
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

// Dedicated "dial a number" page: a permanently visible keypad, reachable
// directly from the left tab bar (not hidden behind a tiny icon inside the
// call history screen). Lets an operator type or paste a number, call it,
// and save it to contacts right away — without first having to make a call.
// It also carries a grid of office-phone-style speed-dial keys: click a
// programmed key to call it instantly, click an empty one to program it.
FocusScope {
    id: mainItem
    objectName: "dialerPage"

    signal createContactRequested(string name, string address)

    readonly property int speedDialCount: 21

    property CallProxy callsModel: CallProxy {
        sourceModel: AppCpp.calls
    }

    property var speedDials: []

    function loadSpeedDials() {
        var arr = [];
        try {
            var parsed = JSON.parse(SettingsCpp.speedDialsJson || "[]");
            if (Array.isArray(parsed)) arr = parsed;
        } catch (e) {
            arr = [];
        }
        while (arr.length < mainItem.speedDialCount) arr.push({
            "name": "",
            "number": ""
        });
        mainItem.speedDials = arr;
    }

    function saveSpeedDial(index, name, number) {
        var arr = mainItem.speedDials.slice();
        arr[index] = {
            "name": name,
            "number": number
        };
        mainItem.speedDials = arr;
        SettingsCpp.lSetSpeedDialsJson(JSON.stringify(arr));
    }

    Component.onCompleted: {
        loadSpeedDials();
        dialerInput.forceActiveFocus();
    }

    Connections {
        target: SettingsCpp
        function onSpeedDialsJsonChanged() {
            mainItem.loadSpeedDials();
        }
    }

    function forceActiveFocus(reason = undefined) {
        dialerInput.forceActiveFocus(reason)
    }

    function clear() {
        dialerInput.clearText()
    }

    Dialog {
        id: speedDialEditor
        property int slotIndex: -1
        padding: Utils.getSizeWithScreenRatio(30)
        width: Utils.getSizeWithScreenRatio(500)
        anchors.centerIn: parent
        closePolicy: Control.Popup.CloseOnEscape
        modal: true
        //: "Программируемая кнопка"
        title: qsTr("speed_dial_editor_title")
        onOpened: speedDialNameField.forceActiveFocus()
        content: ColumnLayout {
            spacing: Utils.getSizeWithScreenRatio(14)
            TextField {
                id: speedDialNameField
                Layout.fillWidth: true
                height: Utils.getSizeWithScreenRatio(49)
                backgroundColor: DefaultStyle.grey_0
                backgroundBorderColor: DefaultStyle.grey_200
                //: "Имя"
                placeholderText: qsTr("speed_dial_editor_name_placeholder")
            }
            TextField {
                id: speedDialNumberField
                Layout.fillWidth: true
                height: Utils.getSizeWithScreenRatio(49)
                backgroundColor: DefaultStyle.grey_0
                backgroundBorderColor: DefaultStyle.grey_200
                //: "Номер"
                placeholderText: qsTr("speed_dial_editor_number_placeholder")
                onAccepted: speedDialSaveButton.clicked()
            }
        }
        buttons: RowLayout {
            Button {
                visible: speedDialEditor.slotIndex >= 0 && (mainItem.speedDials[speedDialEditor.slotIndex]?.number || "").length > 0
                style: ButtonStyle.noBackgroundRed
                //: "Clear"
                text: qsTr("speed_dial_editor_clear")
                onClicked: {
                    mainItem.saveSpeedDial(speedDialEditor.slotIndex, "", "");
                    speedDialEditor.close();
                }
            }
            Item { Layout.fillWidth: true }
            BigButton {
                style: ButtonStyle.secondary
                text: qsTr("cancel")
                onClicked: speedDialEditor.close()
            }
            BigButton {
                id: speedDialSaveButton
                style: ButtonStyle.main
                text: qsTr("save")
                enabled: speedDialNumberField.text.trim().length > 0
                onClicked: {
                    if (!enabled) return;
                    mainItem.saveSpeedDial(speedDialEditor.slotIndex, speedDialNameField.text.trim(), speedDialNumberField.text.trim());
                    speedDialEditor.close();
                }
            }
        }
        function openFor(index) {
            slotIndex = index;
            var entry = mainItem.speedDials[index] || {
                "name": "",
                "number": ""
            };
            speedDialNameField.text = entry.name || "";
            speedDialNumberField.text = entry.number || "";
            open();
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Utils.getSizeWithScreenRatio(45)
        anchors.rightMargin: Utils.getSizeWithScreenRatio(45)
        spacing: Utils.getSizeWithScreenRatio(24)

        // Kept narrow and left-aligned on purpose: the speed-dial grid next
        // to it needs the width far more than the keypad does. Wrapped in a
        // plain Item because a nested ColumnLayout ignores its own
        // Layout.preferredWidth and sizes to its children's implicit width
        // instead — the Item's width is what the outer RowLayout actually
        // honors.
        Item {
            Layout.preferredWidth: Utils.getSizeWithScreenRatio(280)
            Layout.fillHeight: true

        ColumnLayout {
            anchors.fill: parent
            spacing: Utils.getSizeWithScreenRatio(20)

            Text {
                Layout.fillWidth: true
                //: "Набор номера"
                text: qsTr("dialer_page_title")
                maximumLineCount: 1
                color: DefaultStyle.main2_700
                font.pixelSize: Typography.h2.pixelSize
                font.weight: Typography.h2.weight
            }

            Item { Layout.preferredHeight: Utils.getSizeWithScreenRatio(8) }

            SearchBar {
                id: dialerInput
                Layout.preferredWidth: Utils.getSizeWithScreenRatio(260)
                height: Utils.getSizeWithScreenRatio(46)
                magnifierVisible: false
                color: DefaultStyle.grey_100
                //: "Numéro à composer"
                placeholderText: qsTr("dialer_page_placeholder")
                numericPadPopup: numPad
                numericPadButton.visible: false
                Keys.onReturnPressed: UtilsCpp.createCall(dialerInput.text)
                Keys.onEnterPressed: UtilsCpp.createCall(dialerInput.text)
            }

            // NumericPad has no size property of its own, so it's rendered at
            // 80% and wrapped in a same-ratio Item to keep the layout's
            // reserved space in sync with what's actually drawn.
            Item {
                Layout.preferredWidth: numPad.width * 0.8
                Layout.preferredHeight: numPad.height * 0.8
                NumericPad {
                    id: numPad
                    scale: 0.8
                    transformOrigin: Item.TopLeft
                    currentCall: mainItem.callsModel.currentCall
                    onLaunchCall: {
                        if (dialerInput.text.length > 0) UtilsCpp.createCall(dialerInput.text)
                    }
                }
            }

            Button {
                id: addToContactsButton
                Layout.topMargin: Utils.getSizeWithScreenRatio(4)
                enabled: dialerInput.text.length > 0
                opacity: enabled ? 1 : 0.5
                style: ButtonStyle.secondary
                icon.source: AppIcons.plusCircle
                icon.width: Utils.getSizeWithScreenRatio(24)
                icon.height: Utils.getSizeWithScreenRatio(24)
                //: "Add to contacts"
                text: qsTr("menu_add_address_to_contacts")
                onClicked: {
                    var number = dialerInput.text
                    mainItem.createContactRequested(number, number)
                    mainItem.clear()
                }
            }

            Item { Layout.fillHeight: true }
        }
        }

        Rectangle {
            Layout.preferredWidth: Utils.getSizeWithScreenRatio(1)
            Layout.fillHeight: true
            Layout.topMargin: Utils.getSizeWithScreenRatio(60)
            Layout.bottomMargin: Utils.getSizeWithScreenRatio(40)
            color: DefaultStyle.grey_200
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Utils.getSizeWithScreenRatio(16)

            Text {
                Layout.fillWidth: true
                Layout.topMargin: Utils.getSizeWithScreenRatio(4)
                //: "Быстрый набор"
                text: qsTr("speed_dial_section_title")
                maximumLineCount: 1
                color: DefaultStyle.main2_700
                font.pixelSize: Typography.h4.pixelSize
                font.weight: Typography.h4.weight
            }

            Text {
                Layout.fillWidth: true
                //: "Cliquez pour appeler, sur le crayon pour modifier"
                text: qsTr("speed_dial_section_hint")
                wrapMode: Text.WordWrap
                color: DefaultStyle.main2_500_main
                font.pixelSize: Typography.p3.pixelSize
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                columns: 3
                rowSpacing: Utils.getSizeWithScreenRatio(10)
                columnSpacing: Utils.getSizeWithScreenRatio(16)

                Repeater {
                    model: mainItem.speedDials
                    delegate: Rectangle {
                        id: slotDelegate
                        required property var modelData
                        required property int index
                        readonly property bool filled: (modelData.number || "").length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: Utils.getSizeWithScreenRatio(50)
                        radius: Utils.getSizeWithScreenRatio(10)
                        color: filled ? DefaultStyle.grey_100 : "transparent"
                        border.width: Utils.getSizeWithScreenRatio(1)
                        border.color: filled ? DefaultStyle.grey_200 : DefaultStyle.grey_300

                        // Name only — the number doesn't fit next to it once
                        // the window is un-maximized, and the name alone is
                        // what an operator actually needs to recognize a key.
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: Utils.getSizeWithScreenRatio(14)
                            anchors.rightMargin: Utils.getSizeWithScreenRatio(14)
                            verticalAlignment: Text.AlignVCenter
                            visible: slotDelegate.filled
                            text: (slotDelegate.modelData.name || "").length > 0 ? slotDelegate.modelData.name : slotDelegate.modelData.number
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            color: DefaultStyle.main2_700
                            font.pixelSize: Typography.p1.pixelSize
                            font.weight: Typography.p1b.weight
                        }

                        EffectImage {
                            visible: !slotDelegate.filled
                            anchors.centerIn: parent
                            imageSource: AppIcons.plusCircle
                            colorizationColor: DefaultStyle.grey_400
                            width: Utils.getSizeWithScreenRatio(20)
                            height: Utils.getSizeWithScreenRatio(20)
                        }

                        // Left click dials (or programs an empty key); right
                        // click always opens the editor — no per-key pencil
                        // icon cluttering the grid.
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) speedDialEditor.openFor(slotDelegate.index);
                                else if (slotDelegate.filled) UtilsCpp.createCall(slotDelegate.modelData.number);
                                else speedDialEditor.openFor(slotDelegate.index);
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
