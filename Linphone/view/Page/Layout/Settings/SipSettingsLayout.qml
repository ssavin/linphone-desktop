import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control
import UtilsCpp 1.0
import Linphone
import 'qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js' as Utils
import 'qrc:/qt/qml/Linphone/view/Style/buttonStyle.js' as ButtonStyle

// Quick shortcut to replace SIP settings: one tap signs the current
// account out and lands on the KiwiCall login screen (email/password,
// QR code or manual SIP entry) - the same result as going through
// "Моя учётная запись" > "Настройки учётной записи" > "Удалить учётную
// запись", just reachable directly from the top of Settings instead of
// being buried behind the confusing linphone.org-style account page.
AbstractSettingsLayout {
	width: parent?.width
	saveButtonVisible: false

	// account.core.removeAccount() runs on the core thread and returns
	// immediately - the actual removal (and accountProxy.haveAccount
	// updating) happens slightly later. AccountSettingsPage.qml listens for
	// this same account.core.removed() signal before navigating away; mirror
	// that here instead of calling initStackViewItem() synchronously (too
	// early - accountProxy wouldn't have updated yet) or relying on
	// MainLayout's accountRemoved signal chain, which only AccountSettingsPage
	// (reached via "Моя учётная запись") is wired into, not this page
	// (reached via "Настройки").
	property var pendingRemovedAccount: null
	Connections {
		target: pendingRemovedAccount ? pendingRemovedAccount.core : null
		function onRemoved() {
			pendingRemovedAccount = null
			UtilsCpp.getMainWindow().initStackViewItem()
		}
	}

	contentModel: [
		{
            //: "Изменение SIP-настроек"
            title: qsTr("settings_sip_description_title"),
            //: "Если вам нужно войти с другим номером или паролем..."
            subTitle: qsTr("settings_sip_description_subtitle"),
			contentComponent: sipSettingsButtonComponent,
			hideTopSeparator: true
		}
	]

	Component {
		id: sipSettingsButtonComponent
		BigButton {
            //: "Заменить SIP-настройки"
            text: qsTr("settings_sip_replace_button")
			style: ButtonStyle.main
			onClicked: {
				var mainWin = UtilsCpp.getMainWindow()
				mainWin.showConfirmationLambdaPopup("",
                    //: "Выйти из учётной записи?"
                    qsTr("settings_sip_confirm_title"),
                    //: "Текущий SIP-аккаунт будет отключён, и откроется экран входа для новых параметров."
                    qsTr("settings_sip_replace_confirm_message"),
					function (confirmed) {
						if (!confirmed || !mainWin.accountProxy) return
						// defaultAccount can be a stale/incomplete reference in some
						// states (seen in production: "Cannot read property
						// 'removeAccount' of null") - firstAccount() is the same
						// fallback MainLayout.qml already uses when opening account
						// settings, so mirror it here instead of failing silently.
						var account = mainWin.accountProxy.defaultAccount
						if (!account || !account.core) account = mainWin.accountProxy.firstAccount()
						if (account && account.core) {
							pendingRemovedAccount = account
							account.core.removeAccount()
						} else {
							console.warn("[SipSettingsLayout] No usable account found to remove")
						}
					}
				)
			}
		}
	}
}
