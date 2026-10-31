// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Posthoorn 1.0

// One of an account's virtual folders, above its real ones: Starred, with the number still
// unread, or Snoozed, with the number snoozed. folderId says which.
FolderItem {
    id: item

    name: app.virtualFolderName(folderId)
    unreadCount: folderId === app.snoozedFolderId ? mail.count : mail.unreadCount
    onClicked: app.openVirtualFolder(accountId, folderId, PageStackAction.Animated)

    UnifiedMessageListModel {
        id: mail
        search: app.virtualFolderSearch(item.folderId, item.accountId)
    }
}
