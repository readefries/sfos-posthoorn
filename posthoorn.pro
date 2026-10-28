TEMPLATE = subdirs

# The Silica UI.
packagesExist(sailfishapp) {
    SUBDIRS += app
}

OTHER_FILES += rpm/posthoorn.spec README.md

license.files = LICENSE
license.path = /usr/share/licenses/posthoorn
INSTALLS += license
