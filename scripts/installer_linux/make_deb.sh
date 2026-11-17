#!/bin/bash

# usage: make_deb.sh <product dir> <source dir> <output .deb> <version>

set -e

INDIR=$1
SOURCEDIR=$2
TARGET_DEB=$3
VERSION=$4

PRODUCT_NAME="Six Sines"
PACKAGE_NAME=six-sines

# releases are X.Y.Z; nightlies sort above the newest release tag and below the next release
TAG_PREFIX=v
if [[ ${VERSION} =~ ^(${TAG_PREFIX})?([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
    DEB_VERSION=${BASH_REMATCH[2]}
else
    BASE=${DEB_BASE_VERSION:-0.0.0}
    if ! [[ ${BASE} =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "ERROR: DEB_BASE_VERSION '${BASE}' is not X.Y.Z"
        exit 1
    fi
    LAST_REL=$(git -C "${SOURCEDIR}" tag --list "${TAG_PREFIX}*" \
        | sed -nE "s/^${TAG_PREFIX}([0-9]+\.[0-9]+\.[0-9]+)$/\1/p" | sort -V | tail -n 1)
    COUNT=$(git -C "${SOURCEDIR}" rev-list --count HEAD || echo 0)
    DATE=$(TZ=UTC git -C "${SOURCEDIR}" log -1 --format=%cd --date=format-local:%Y%m%d || date -u +%Y%m%d)
    HASH=$(git -C "${SOURCEDIR}" rev-parse --short HEAD || echo nohash)
    if [[ $(git -C "${SOURCEDIR}" rev-parse --is-shallow-repository) == true ]]; then
        echo "WARNING: shallow clone; nightly deb versions will not order correctly"
    fi
    if [[ -z ${LAST_REL} ]] || dpkg --compare-versions "${BASE}" gt "${LAST_REL}"; then
        DEB_VERSION="${BASE}~nightly.${DATE}.${COUNT}.${HASH}"
    else
        DEB_VERSION="${LAST_REL}+nightly.${DATE}.${COUNT}.${HASH}"
    fi
fi
if ! [[ ${DEB_VERSION} =~ ^[0-9][A-Za-z0-9.+~]*$ ]]; then
    echo "ERROR: '${DEB_VERSION}' is not a valid deb version"
    exit 1
fi
echo "DEB VERSION is ${DEB_VERSION}"

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
Sndstation-Kind: clap-plugin
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
