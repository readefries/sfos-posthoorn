// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// An account's virtual Starred folder, above its real folders: its starred mail, wherever it is.
FolderItem {
    id: item

    folderId: app.starredFolderId
    name: qsTr("Starred")
    unreadCount: starredMail.unreadCount
    onClicked: app.openStarred(accountId, PageStackAction.Animated)

    UnifiedMessageListModel {
        id: starredMail
        search: ({ "starred": true, "accountId": item.accountId })
    }
}
