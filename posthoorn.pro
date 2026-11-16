TEMPLATE = subdirs

SUBDIRS = core tests
tests.depends = core

# The Silica UI.
packagesExist(sailfishapp) {
    SUBDIRS += app
    app.depends = core
}

# posthoorn-notify-filter, the messageserver plugin that keeps muted accounts quiet.
SUBDIRS += notifyfilter

OTHER_FILES += rpm/posthoorn.spec README.md docs/ROADMAP.md

license.files = LICENSE
license.path = /usr/share/licenses/posthoorn
INSTALLS += license
