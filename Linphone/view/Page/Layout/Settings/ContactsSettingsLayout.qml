
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Control
import QtQuick.Dialogs
import SettingsCpp
import ContactImportCpp
import Linphone
import "qrc:/qt/qml/Linphone/view/Control/Tool/Helper/utils.js" as Utils

AbstractSettingsLayout {
	id: mainItem
	width: parent?.width
	contentModel: [
		{
            //: Annuaires LDAP
            title: qsTr("settings_contacts_ldap_title"),
            //: "Ajouter vos annuaires LDAP pour pouvoir effectuer des recherches dans la barre de recherche."
            subTitle: qsTr("settings_contacts_ldap_subtitle"),
			contentComponent: ldapParametersComponent,
			hideTopMargin: true
		},
		{
            title: qsTr("settings_contacts_carddav_title"),
            subTitle: qsTr("settings_contacts_carddav_subtitle"),
			contentComponent: cardDavParametersComponent,
			hideTopMargin: true
		},
		{
			title: qsTr("settings_contacts_import_title"),
			subTitle: qsTr("settings_contacts_import_subtitle"),
			contentComponent: importContactsComponent,
			hideTopMargin: true
		}
	]

	function layoutUrl(name) {
		return layoutsPath+"/"+name+".qml"
	}
	function createGuiObject(name) {
		return Qt.createQmlObject('import Linphone; '+name+'Gui{}', mainItem)
	}

	// Ldap parameters
	//////////////////

	Component {
		id: ldapParametersComponent
		ContactsSettingsProviderLayout {
            //: "Ajouter un annuaire LDAP"
            addText: qsTr("settings_contacts_add_ldap_server_title")
            //: "Modifier un annuaire LDAP"
            editText: qsTr("settings_contacts_edit_ldap_server_title")
			//: "Editer le serveur LDAP %1"
			accessibleEditButtonText: qsTr("edit_ldap_server_accessible_name")
			//: "Utiliser le serveur LDAP %1"
			accessibleUseButtonText: qsTr("use_ldap_server_accessible_name")
			proxyModel: LdapProxy {}
			newItemGui: createGuiObject('Ldap')
			settingsLayout: layoutUrl("LdapSettingsLayout")
			owner: mainItem
			titleProperty: "serverUrl"
			supportsEnableDisable: true
			showAddButton: true

			Connections {
				target: mainItem
				function onSave() { save()}
				function onUndo() { undo()}
			}
		}
	}

	// CardDAV parameters
	/////////////////////

	Component {
		id: cardDavParametersComponent
		ContactsSettingsProviderLayout {
			id: carddavProvider
            //: "Ajouter un carnet d'adresse CardDAV"
            addText: qsTr("settings_contacts_add_carddav_server_title")
            //: "Modifier un carnet d'adresse CardDAV"
            editText: qsTr("settings_contacts_edit_carddav_server_title")
			//: "Editer le carnet d'adresses CardDAV %1"
			accessibleEditButtonText: qsTr("edit_cardav_server_accessible_name")
			//: "Utiliser le d'adresses CardDAV %1"
			accessibleUseButtonText: qsTr("use_cardav_server_accessible_name")
			proxyModel: CarddavProxy {
				onModelReset:  {
					carddavProvider.showAddButton = carddavProvider.proxyModel.count == 0
					carddavProvider.newItemGui = createGuiObject('Carddav')
				}
			}
			newItemGui: createGuiObject('Carddav')
			settingsLayout: layoutUrl("CarddavSettingsLayout")
			owner: mainItem
			titleProperty: "displayName"
			supportsEnableDisable: false

			Connections {
				target: mainItem
				function onSave() { save()}
				function onUndo() { undo()}
			}
		}
	}

	// Contact import (KiwiCall CRM or a local CSV file)
	/////////////////////////////////////////////////////

	Component {
		id: importContactsComponent
		ColumnLayout {
			id: importRoot
			spacing: Utils.getSizeWithScreenRatio(20)
			property bool busy: false

			FileDialog {
				id: csvFileDialog
				nameFilters: ["CSV files (*.csv)"]
				options: FileDialog.ReadOnly
				onAccepted: {
					importRoot.busy = true
					importStatus.text = ""
					ContactImportCpp.importFromCsv(Utils.getSystemPathFromUri(selectedFile))
				}
			}

			ColumnLayout {
				spacing: Utils.getSizeWithScreenRatio(8)
				Layout.fillWidth: true
				Text {
					text: qsTr("settings_contacts_import_csv_title")
					font: Typography.p2l
					color: DefaultStyle.main2_600
				}
				MediumButton {
					text: qsTr("settings_contacts_import_csv_button")
					enabled: !importRoot.busy
					onClicked: csvFileDialog.open()
				}
			}

			Rectangle {
				Layout.fillWidth: true
				height: Utils.getSizeWithScreenRatio(1)
				color: DefaultStyle.main2_500_main
			}

			ColumnLayout {
				spacing: Utils.getSizeWithScreenRatio(8)
				Layout.fillWidth: true
				Text {
					text: qsTr("settings_contacts_import_kiwicall_title")
					font: Typography.p2l
					color: DefaultStyle.main2_600
				}
				TextField {
					id: kiwiEmail
					Layout.fillWidth: true
					placeholderText: qsTr("settings_contacts_import_kiwicall_email_placeholder")
				}
				TextField {
					id: kiwiPassword
					Layout.fillWidth: true
					hidden: true
					placeholderText: qsTr("password")
				}
				MediumButton {
					text: qsTr("settings_contacts_import_kiwicall_button")
					enabled: !importRoot.busy && kiwiEmail.text.length > 0 && kiwiPassword.text.length > 0
					onClicked: {
						importRoot.busy = true
						importStatus.text = ""
						ContactImportCpp.importFromKiwiCall(kiwiEmail.text, kiwiPassword.text)
					}
				}
			}

			Text {
				id: importStatus
				Layout.fillWidth: true
				wrapMode: Text.WordWrap
				font: Typography.p2
			}

			Connections {
				target: ContactImportCpp
				function onImportStarted() {
					importRoot.busy = true
				}
				function onImportFinished(successCount, errorCount) {
					importRoot.busy = false
					importStatus.color = DefaultStyle.success_500_main
					//: "%1 contacts imported"
					importStatus.text = qsTr("settings_contacts_import_success").arg(successCount)
				}
				function onImportError(message) {
					importRoot.busy = false
					importStatus.color = DefaultStyle.danger_500_main
					importStatus.text = message
				}
			}
		}
	}
}
