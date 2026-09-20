import QtQuick
import QtQuick.Layouts
import Linphone
import UtilsCpp
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

// =============================================================================

Notification {
	id: mainItem
    radius: Utils.getSizeWithScreenRatio(10)
	backgroundColor: DefaultStyle.grey_600
	backgroundOpacity: 0.8
    overriddenWidth: Utils.getSizeWithScreenRatio(400)
	overriddenHeight: content.height
	
	readonly property var call: notificationData && notificationData.call
	readonly property var displayName: notificationData && notificationData.displayName
	property var state: call.core.state
	property var status: call.core.status
	property var conference: call.core.conference

	onStateChanged:{
		if (state != LinphoneEnums.CallState.IncomingReceived){
			close()
		}
	}
	onStatusChanged:{
		console.log("status", status)
	}
	
	Popup {
		id: content
		visible: mainItem.visible
        leftPadding: Utils.getSizeWithScreenRatio(32)
        rightPadding: Utils.getSizeWithScreenRatio(32)
        topPadding: Utils.getSizeWithScreenRatio(9)
        bottomPadding: Utils.getSizeWithScreenRatio(18)
		anchors.centerIn: parent
		background: Item{}
		contentItem: ColumnLayout {
			anchors.verticalCenter: parent.verticalCenter
            spacing: Utils.getSizeWithScreenRatio(9)
			RowLayout {
                spacing: Utils.getSizeWithScreenRatio(4)
				Layout.alignment: Qt.AlignHCenter
				Image {
                    Layout.preferredWidth: Utils.getSizeWithScreenRatio(12)
                    Layout.preferredHeight: Utils.getSizeWithScreenRatio(12)
					source: AppIcons.logo
				}
				Text {
					text: "Linphone"
					color: DefaultStyle.grey_0
					font {
                        pixelSize: Utils.getSizeWithScreenRatio(12)
                        weight: Typography.b3.weight
						capitalization: Font.Capitalize
					}
				}
			}
			ColumnLayout {
                spacing: Utils.getSizeWithScreenRatio(17)
				ColumnLayout {
                    spacing: Utils.getSizeWithScreenRatio(14)
					Layout.alignment: Qt.AlignHCenter
					Avatar {
                        Layout.preferredWidth: Utils.getSizeWithScreenRatio(60)
                        Layout.preferredHeight: Utils.getSizeWithScreenRatio(60)
						Layout.alignment: Qt.AlignHCenter
						call: mainItem.call
						displayNameVal: mainItem.displayName
						secured: securityLevel === LinphoneEnums.SecurityLevel.EndToEndEncryptedAndVerified
						isConference: mainItem.call && mainItem.call.core.isConference
					}
					ColumnLayout {
						spacing: 0
						Text {
							text: displayName
							Layout.fillWidth: true
                            Layout.maximumWidth: mainItem.width - content.leftPadding - content.rightPadding
							Layout.alignment: Qt.AlignHCenter
							horizontalAlignment: Text.AlignHCenter
							maximumLineCount: 1
							color: DefaultStyle.grey_0
							font {
                                pixelSize: Utils.getSizeWithScreenRatio(20)
                                weight: Typography.b3.weight
								capitalization: Font.Capitalize
							}
						}
						Text {
                            //: "Appel entrant"
                            text: qsTr("call_audio_incoming")
							Layout.alignment: Qt.AlignHCenter
							color: DefaultStyle.grey_0
							font {
                                pixelSize: Utils.getSizeWithScreenRatio(14)
                                weight: Utils.getSizeWithScreenRatio(500)
							}
						}
					}
				}
				// "Шторка звонка": CRM card for the caller, looked up by
				// CrmCardCore (see CallCore::findCrmCard). Kept intentionally
				// minimal here (name + visit count) - the in-call window shows
				// the fuller card (recent calls, appointments).
				ColumnLayout {
					id: crmCardSection
					readonly property var crmCard: mainItem.call && mainItem.call.core.crmCard
					Layout.alignment: Qt.AlignHCenter
					Layout.fillWidth: true
					spacing: Utils.getSizeWithScreenRatio(2)
					visible: crmCard && (crmCard.loading || crmCard.found)
					Text {
						visible: crmCardSection.crmCard && crmCardSection.crmCard.loading
						//: "Ищем карточку клиента..."
						text: qsTr("crm_card_loading")
						Layout.alignment: Qt.AlignHCenter
						color: DefaultStyle.grey_200
						font.pixelSize: Utils.getSizeWithScreenRatio(12)
					}
					Text {
						visible: crmCardSection.crmCard && !crmCardSection.crmCard.loading && crmCardSection.crmCard.found
						text: crmCardSection.crmCard ? crmCardSection.crmCard.clientName : ""
						Layout.alignment: Qt.AlignHCenter
						color: DefaultStyle.grey_0
						font {
							pixelSize: Utils.getSizeWithScreenRatio(13)
							weight: Typography.b3.weight
						}
					}
					Text {
						visible: crmCardSection.crmCard && !crmCardSection.crmCard.loading && crmCardSection.crmCard.found
						//: "%1 визитов"
						text: qsTr("crm_card_visits_count").arg(crmCardSection.crmCard ? crmCardSection.crmCard.visitsCount : 0)
						Layout.alignment: Qt.AlignHCenter
						color: DefaultStyle.grey_200
						font.pixelSize: Utils.getSizeWithScreenRatio(12)
					}
				}
				RowLayout {
					Layout.alignment: Qt.AlignHCenter
					Layout.fillWidth: true
                    spacing: Utils.getSizeWithScreenRatio(26)
					Button {
                        spacing: Utils.getSizeWithScreenRatio(6)
						style: ButtonStyle.phoneGreen
                        Layout.preferredWidth: Utils.getSizeWithScreenRatio(118)
                        Layout.preferredHeight: Utils.getSizeWithScreenRatio(32)
						asynchronous: false
                        icon.width: Utils.getSizeWithScreenRatio(19)
                        icon.height: Utils.getSizeWithScreenRatio(19)
                        //: "Accepter"
                        text: qsTr("dialog_accept")
                        textSize: Utils.getSizeWithScreenRatio(14)
                        textWeight: Utils.getSizeWithScreenRatio(500)
						onClicked: {
							console.debug("[NotificationReceivedCall] Accept click")
							UtilsCpp.openCallsWindow(mainItem.call)
							mainItem.call.core.lAccept(false)
						}
					}
					Button {
                        spacing: Utils.getSizeWithScreenRatio(6)
						style: ButtonStyle.phoneRed
                        Layout.preferredWidth: Utils.getSizeWithScreenRatio(118)
                        Layout.preferredHeight: Utils.getSizeWithScreenRatio(32)
						asynchronous: false
                        icon.width: Utils.getSizeWithScreenRatio(19)
                        icon.height: Utils.getSizeWithScreenRatio(19)
                        //: "Refuser
                        text: qsTr("dialog_deny")
                        textSize: Utils.getSizeWithScreenRatio(14)
                        textWeight: Utils.getSizeWithScreenRatio(500)
						onClicked: {
							console.debug("[NotificationReceivedCall] Decline click")
							mainItem.call.core.lDecline()
						}
					}
				}
			}
		}
	}

}
