#!/bin/bash

# usage: make_deb.sh <product dir> <source dir> <output .deb> <version>

set -e

INDIR=$1
SOURCEDIR=$2
TARGET_DEB=$3
VERSION=$4

PRODUCT_NAME="Six Sines"
PACKAGE_NAME=six-sines

# valid deb versions start with a digit
DEB_VERSION=$VERSION
if [[ ${DEB_VERSION:0:1} =~ [0-9] ]]; then
    echo "DEB VERSION same as version (${DEB_VERSION})"
elif [[ ${DEB_VERSION} = NIGHTLY* ]]; then
    DEB_VERSION="9.${VERSION}"
    echo "NIGHTLY; DEB VERSION is ${DEB_VERSION}"
else
    DEB_VERSION="0.${VERSION}"
    echo "DEB VERSION is ${DEB_VERSION}"
fi

STAGE=$(mktemp -d)
trap 'rm -rf "${STAGE}"' EXIT

# dpkg-shlibdeps wants the tree at debian/<package> next to a debian/control
PKG="${STAGE}/debian/${PACKAGE_NAME}"
DOC="${PKG}/usr/share/doc/${PACKAGE_NAME}"
mkdir -p "${PKG}/usr/lib/clap" "${PKG}/usr/lib/vst3" "${PKG}/usr/bin" "${DOC}" "${PKG}/DEBIAN"
touch "${STAGE}/debian/control"

cp "${INDIR}/${PRODUCT_NAME}.clap" "${PKG}/usr/lib/clap/"
cp -r "${INDIR}/${PRODUCT_NAME}.vst3" "${PKG}/usr/lib/vst3/"
cp "${INDIR}/${PRODUCT_NAME}" "${PKG}/usr/bin/"

find "${PKG}" -type d -exec chmod 0755 {} +
find "${PKG}/usr/lib" -type f -exec chmod 0644 {} +
chmod 0755 "${PKG}/usr/bin/${PRODUCT_NAME}"

cp "${SOURCEDIR}/resources/LICENSE_GPL3" "${DOC}/copyright"
MSG=$(git -C "${SOURCEDIR}" log -n 1 --pretty="%s (git hash %H)" || echo "Build ${VERSION}")
cat <<EOT > "${DOC}/changelog.Debian"
${PACKAGE_NAME} (${DEB_VERSION}) stable; urgency=medium

  * ${MSG}
  * For more details, see: https://github.com/baconpaul/six-sines

 -- BaconPaul <noreply@github.com>  $(date --rfc-email)
EOT
gzip -9 -n "${DOC}/changelog.Debian"
chmod 0644 "${DOC}"/*

# linked libs, with version floors, read from the binaries themselves
SHLIBS=""
ELFS=()
while IFS= read -r -d '' f; do
    if [[ "$(head -c 4 "$f")" == $'\x7f'ELF ]]; then
        ELFS+=(-e"$f")
    fi
done < <(find "${PKG}/usr/lib" "${PKG}/usr/bin" -type f -print0)

if command -v dpkg-shlibdeps > /dev/null && [[ ${#ELFS[@]} -gt 0 ]]; then
    SHLIBS=$(cd "${STAGE}" && dpkg-shlibdeps -O "${ELFS[@]}" | sed -n 's/^shlibs:Depends=//p') || SHLIBS=""
fi
if [[ -z "${SHLIBS}" ]]; then
    echo "WARNING: no dpkg-shlibdeps result; using a static Depends floor"
    SHLIBS="libc6 (>= 2.27), libstdc++6, libgcc-s1, libfontconfig1, libfreetype6, libx11-6"
fi

# juce dlopens these for the ui and fails without any of them
DEPENDS="${SHLIBS}"
for p in libx11-6 libxext6 libxcursor1 libxinerama1 libxrandr2; do
    if ! [[ ", ${DEPENDS}," =~ ,\ ${p}[\ ,] ]]; then
        DEPENDS="${DEPENDS}, ${p}"
    fi
done

# the standalone dlopens jack, its only audio backend in ci, and alsa for midi
cat <<EOT > "${PKG}/DEBIAN/control"
Source: ${PACKAGE_NAME}
Package: ${PACKAGE_NAME}
Version: ${DEB_VERSION}
Architecture: $(dpkg --print-architecture)
Maintainer: BaconPaul <noreply@github.com>
Depends: ${DEPENDS}
Recommends: libjack-jackd2-0 | libjack0, libasound2, xdg-utils, libxss1, zenity | kdialog
Provides: vst-plugin
Section: sound
Priority: optional
Homepage: https://github.com/baconpaul/six-sines
Description: Audio rate inter-modulation synthesizer
 Six Sines is a small synthesizer which explores audio rate inter-modulation
 of signals. This package contains the CLAP and VST3 plugins and a standalone
 executable.
EOT

cat "${PKG}/DEBIAN/control"

echo "----- PACKAGE CONTENTS -----"
find "${PKG}/usr" -print

mkdir -p "$(dirname "${TARGET_DEB}")"
dpkg-deb --verbose --root-owner-group -Zxz --build "${PKG}" "${TARGET_DEB}"
