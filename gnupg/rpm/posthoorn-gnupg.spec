# A current GnuPG for Posthoorn alone: Sailfish OS ships GnuPG 2.0, which knows no ECC keys
# (ed25519, cv25519). It's linked statically with its own libraries and lives in Posthoorn's
# directory, so the system's GnuPG and its users aren't touched.
#
# scripts/update-gnupg.sh keeps these versions, and the sums in gnupg/SHA256SUMS, current with
# GnuPG's signed release list; scripts/fetch-gnupg.sh downloads the sources next to this file.
%global gnupg_version 2.4.9
%global libgpg_error_version 1.61
%global libgcrypt_version 1.12.3
%global libassuan_version 3.0.2
%global libksba_version 1.8.1
%global npth_version 1.8
%global gnupg_prefix %{_libexecdir}/posthoorn/gnupg

Name:       posthoorn-gnupg
Summary:    GnuPG for Posthoorn
Version:    %{gnupg_version}
# Raised when only a library changes; back to 1 with a new GnuPG.
Release:    1
License:    GPLv3+ and LGPLv2+
URL:        https://gnupg.org
Source0:    https://gnupg.org/ftp/gcrypt/gnupg/gnupg-%{gnupg_version}.tar.bz2
Source1:    https://gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-%{libgpg_error_version}.tar.bz2
Source2:    https://gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-%{libgcrypt_version}.tar.bz2
Source3:    https://gnupg.org/ftp/gcrypt/libassuan/libassuan-%{libassuan_version}.tar.bz2
Source4:    https://gnupg.org/ftp/gcrypt/libksba/libksba-%{libksba_version}.tar.bz2
Source5:    https://gnupg.org/ftp/gcrypt/npth/npth-%{npth_version}.tar.bz2
BuildRequires:  pkgconfig(zlib)
# Everything it needs is inside; it must not stand in for the system's GnuPG.
AutoProv:   no

%description
GnuPG %{gnupg_version} for Posthoorn, with libgpg-error %{libgpg_error_version},
libgcrypt %{libgcrypt_version}, libassuan %{libassuan_version}, libksba
%{libksba_version} and npth %{npth_version}, in %{gnupg_prefix}.

%prep
%setup -q -c -T

%build
# Unpacked here rather than in %%prep, which the SDK's in-place builds skip.
for source in %{SOURCE0} %{SOURCE1} %{SOURCE2} %{SOURCE3} %{SOURCE4} %{SOURCE5}; do
    [ -d "$(basename "$source" .tar.bz2)" ] || tar -xjf "$source"
done
deps=$PWD/deps
export PATH=$deps/bin:$PATH

# The libraries: static, only for linking GnuPG.
library() {
    directory=$1
    shift
    (cd "$directory" &&
     ./configure --prefix="$deps" --disable-shared --enable-static --with-pic --disable-doc "$@" &&
     make %{?_smp_mflags} && make install)
}
library libgpg-error-%{libgpg_error_version} --disable-nls --disable-languages --disable-tests
library libgcrypt-%{libgcrypt_version} --with-libgpg-error-prefix="$deps" \
%ifarch %{ix86}
    --disable-asm \
%endif
    --disable-padlock-support
library libassuan-%{libassuan_version} --with-libgpg-error-prefix="$deps"
library libksba-%{libksba_version} --with-libgpg-error-prefix="$deps"
library npth-%{npth_version} --disable-tests

# gpg and gpg-agent: no smartcards, keyservers (dirmngr), LDAP, TLS or SQLite.
cd gnupg-%{gnupg_version}
./configure --prefix=%{gnupg_prefix} \
    --with-libgpg-error-prefix="$deps" --with-libgcrypt-prefix="$deps" \
    --with-libassuan-prefix="$deps" --with-ksba-prefix="$deps" --with-npth-prefix="$deps" \
    --disable-scdaemon --disable-dirmngr --disable-keyboxd --disable-tpm2d \
    --disable-card-support --disable-ccid-driver --disable-ldap --disable-sqlite --disable-tofu \
    --disable-gnutls --disable-ntbtls --disable-wks-tools --disable-gpgtar \
    --disable-photo-viewers --disable-bzip2 --without-readline --disable-nls --disable-doc
make %{?_smp_mflags}

%install
cd gnupg-%{gnupg_version}
make install DESTDIR=%{buildroot}
# Only what Posthoorn uses: gpg and its agent, with the agent's helpers. No S/MIME (gpgsm),
# smartcards, Web Key Directory tools or debugging aids.
rm -rf %{buildroot}%{gnupg_prefix}/share %{buildroot}%{gnupg_prefix}/sbin
for tool in gpgsm gpg-card gpg-mail-tube gpg-wks-client gpgparsemail gpgscm gpgsplit kbxutil \
            watchgnupg; do
    rm -f %{buildroot}%{gnupg_prefix}/bin/$tool
done
for tool in gpg-auth gpg-pair-tool gpg-wks-client; do
    rm -f %{buildroot}%{gnupg_prefix}/libexec/$tool
done

%check
%{buildroot}%{gnupg_prefix}/bin/gpg --version

%files
%license gnupg-%{gnupg_version}/COPYING
%{gnupg_prefix}
