Name:       posthoorn
Summary:    Email client for Sailfish OS
Version:    0.1.0
Release:    1%{?dev_suffix}
License:    AGPLv3+
URL:        https://github.com/readefries/sfos-posthoorn
Source0:    %{name}-%{version}.tar.bz2

BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(QmfClient)
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  desktop-file-utils
Requires:       sailfishsilica-qt5 >= 0.10.9
# The Qt Messaging Framework QML API and its messageserver, shared with the stock Email app.
Requires:       nemo-qml-plugin-email-qt5

%description
Email client for Sailfish OS, built on the system's mail framework so it
uses the accounts from Settings → Accounts.

%package default-mail
Summary:    Open mail notifications and mail links in Posthoorn
Requires:   %{name} = %{version}-%{release}
# Both answer to com.jolla.email.ui, which the system's new-mail notifications call.
Conflicts:  jolla-email

%description default-mail
Makes Posthoorn the mail app in place of Jolla's Mail, which has to be
uninstalled for it: new-mail notifications and mailto: links open
Posthoorn. Removing Jolla's Mail switches off the mail accounts; switch
them on again in Settings → Accounts. A Sailfish OS upgrade may want
Jolla's Mail back; remove this package for the upgrade and install it
again afterwards.

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5 VERSION=%{version}
%make_build

%check
make check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop || :
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}-mailto.desktop || :

%install
%qmake5_install

%files
%license %{_datadir}/licenses/%{name}/LICENSE
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%files default-mail
%{_datadir}/dbus-1/services/com.jolla.email.ui.service
%{_sysconfdir}/sailjail/permissions/%{name}.profile
%{_datadir}/applications/%{name}-mailto.desktop
