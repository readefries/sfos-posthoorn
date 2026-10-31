# -*- mode: sh -*-

# Firejail profile for /usr/bin/posthoorn, from posthoorn-default-mail: answering the mail
# app's D-Bus service in place of Jolla's Mail, so new-mail notifications and mailto: links open
# Posthoorn.
dbus-user.own com.jolla.email.ui
