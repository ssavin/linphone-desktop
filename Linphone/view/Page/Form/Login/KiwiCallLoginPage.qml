import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control

import Linphone
import UtilsCpp
import SettingsCpp
import KiwiCallLoginCpp
import 'qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js' as Utils
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle

// Simple KiwiCall email/password login: the desktop counterpart of
// linphone-android's KiwiCallLoginFragment. Entering the same
// email/password used on kiwicall.ru fetches the SIP remote-provisioning
// config from the server and applies it - no manual extension/password/
// domain entry needed. Manual SIP setup remains one click away
// (useSIPButtonClicked -> sipLoginPage) for anyone who still needs it.
LoginLayout {
	id: mainItem
	property bool showBackButton: false
	signal goBack()
	signal useSIPButtonClicked()

	titleContent: [
		BigButton {
			enabled: mainItem.showBackButton
			opacity: mainItem.showBackButton ? 1.0 : 0
            Layout.leftMargin: Utils.getSizeWithScreenRatio(79)
			icon.source: AppIcons.leftArrow
			style: ButtonStyle.noBackground
			onClicked: {
				console.debug("[KiwiCallLoginPage] User: return")
				mainItem.goBack()
			}
			//: Return
			Accessible.name: qsTr("return_accessible_name")
		},
		RowLayout {
            spacing: Utils.getSizeWithScreenRatio(15)
            Layout.leftMargin: Utils.getSizeWithScreenRatio(21)
			EffectImage {
				fillMode: Image.PreserveAspectFit
				imageSource: AppIcons.profile
				colorizationColor: DefaultStyle.main2_600
                Layout.preferredHeight: Utils.getSizeWithScreenRatio(34)
                Layout.preferredWidth: Utils.getSizeWithScreenRatio(34)
			}
			Text {
                text: qsTr("kiwicall_login_title")
				font {
                    pixelSize: Typography.h1.pixelSize
                    weight: Typography.h1.weight
				}
			}
		},
		Item {
			Layout.fillWidth: true
		}
	]
	centerContent: [
		Flickable {
			anchors.left: parent.left
			anchors.top: parent.top
            anchors.leftMargin: Utils.getSizeWithScreenRatio(127)
			anchors.bottom: parent.bottom
			ColumnLayout {
				id: content
                spacing: Utils.getSizeWithScreenRatio(8)

				FormItemLayout {
					id: emailField
                    Layout.preferredWidth: Utils.getSizeWithScreenRatio(346)
                    label: qsTr("kiwicall_login_email")
					mandatory: true
					enableErrorText: true
					contentItem: TextField {
						id: emailEdit
						width: parent.width
                        height: Utils.getSizeWithScreenRatio(49)
						isError: emailField.errorTextVisible
						onAccepted: passwordEdit.forceActiveFocus()
						Accessible.name: qsTr("mandatory_field_accessible_name").arg(qsTr("kiwicall_login_email"))
					}
				}
				Item {
					Layout.preferredHeight: passwordField.implicitHeight
					FormItemLayout {
						id: passwordField
                        width: Utils.getSizeWithScreenRatio(346)
                        label: qsTr("kiwicall_login_password")
						mandatory: true
						enableErrorText: true
						contentItem: TextField {
							id: passwordEdit
							width: parent.width
                            height: Utils.getSizeWithScreenRatio(49)
							isError: passwordField.errorTextVisible
							hidden: true
							onAccepted: loginButton.trigger()
							Accessible.name: qsTr("mandatory_field_accessible_name").arg(qsTr("kiwicall_login_password"))
						}
						TemporaryText {
							id: errorText
							anchors.bottom: parent.bottom
						}
					}
				}
				BigButton {
					id: loginButton
                    Layout.preferredWidth: Utils.getSizeWithScreenRatio(346)
                    Layout.preferredHeight: Utils.getSizeWithScreenRatio(47)
                    Layout.topMargin: Utils.getSizeWithScreenRatio(7)
					style: ButtonStyle.main
					Accessible.name: qsTr("kiwicall_login_button")
					contentItem: StackLayout {
						id: loginButtonContent
						currentIndex: 0
						Text {
                            text: qsTr("kiwicall_login_button")
							horizontalAlignment: Text.AlignHCenter
							verticalAlignment: Text.AlignVCenter
							font {
                                pixelSize: Typography.b1.pixelSize
                                weight: Typography.b1.weight
							}
							color: DefaultStyle.grey_0
						}
						BusyIndicator {
							implicitWidth: parent.height
							implicitHeight: parent.height
							Layout.alignment: Qt.AlignCenter
							indicatorColor: DefaultStyle.grey_0
                            indicatorWidth: Utils.getSizeWithScreenRatio(25)
						}
					}

					function trigger() {
						emailField.errorMessage = ""
						passwordField.errorMessage = ""
						errorText.clear()

						if (emailEdit.text.length == 0 || passwordEdit.text.length == 0) {
							if (emailEdit.text.length == 0)
                                emailField.errorMessage = qsTr("kiwicall_login_missing_email")
							if (passwordEdit.text.length == 0)
                                passwordField.errorMessage = qsTr("kiwicall_login_missing_password")
							return
						}
						KiwiCallLoginCpp.login(emailEdit.text, passwordEdit.text)
					}

					onClicked: loginButton.trigger()

					Connections {
						target: KiwiCallLoginCpp
						function onLoginStarted() {
							loginButton.enabled = false
							loginButtonContent.currentIndex = 1
						}
						function onLoginFailed(message) {
							loginButton.enabled = true
							loginButtonContent.currentIndex = 0
							errorText.setText(message)
						}
						function onLoginSucceeded() {
							loginButton.enabled = true
							loginButtonContent.currentIndex = 0
						}
					}
				}
				BigButton {
                    Layout.preferredWidth: Utils.getSizeWithScreenRatio(346)
                    Layout.preferredHeight: Utils.getSizeWithScreenRatio(47)
                    Layout.topMargin: Utils.getSizeWithScreenRatio(25)
                    text: qsTr("kiwicall_login_manual_sip_link")
					style: ButtonStyle.secondary
					onClicked: {mainItem.useSIPButtonClicked()}
				}
			}
		},
		Image {
			z: -1
			anchors.top: parent.top
			anchors.right: parent.right
            anchors.topMargin: Utils.getSizeWithScreenRatio(129)
            anchors.rightMargin: Utils.getSizeWithScreenRatio(127)
            width: Utils.getSizeWithScreenRatio(395)
            height: Utils.getSizeWithScreenRatio(350)
			fillMode: Image.PreserveAspectFit
			source: AppIcons.loginImage
		}
	]
}
