// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Email 0.1

// Picks the folder to move mail to. QMF only moves within an account, so it lists that account's
// folders.
Page {
    id: page

    property int accountId
    // Where the mail is now; can't be picked.
    property int currentFolderId
    property int count: 1

    signal folderChosen(int folderId)

    FolderListModel {
        id: folders
        accountKey: page.accountId
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: folders

        header: PageHeader {
            title: qsTr("Move to")
            description: qsTr("%n message(s)", "", page.count)
        }

        delegate: ListItem {
            id: item
            enabled: model.canHaveMessages && model.folderId !== page.currentFolderId
            contentHeight: Theme.itemSizeSmall

            onClicked: page.folderChosen(model.folderId)

            Label {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin + model.folderNestingLevel * Theme.paddingLarge
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                text: model.folderName
                color: !item.enabled ? Theme.secondaryColor
                                     : item.highlighted ? Theme.highlightColor : Theme.primaryColor
                truncationMode: TruncationMode.Fade
            }
        }

        VerticalScrollDecorator { }
    }
}
