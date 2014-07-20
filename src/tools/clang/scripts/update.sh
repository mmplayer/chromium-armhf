#!/usr/bin/env bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will check out llvm and clang into third_party/llvm and build it.

THIS_DIR="$(dirname "${0}")"
LLVM_DIR="${THIS_DIR}/../../../third_party/llvm"
LLVM_BUILD_DIR="${LLVM_DIR}/../llvm-build"
CLANG_DIR="${LLVM_DIR}/tools/clang"
DEPS_FILE="${THIS_DIR}/../../../DEPS"
if [ -e "${THIS_DIR}/../../../chromium_deps/DEPS" ]; then
  # For bare WebKit/chromium checkouts.
  DEPS_FILE="${THIS_DIR}/../../../chromium_deps/DEPS"
fi
STAMP_FILE="${LLVM_BUILD_DIR}/cr_build_revision"

# ${A:-a} returns $A if it's set, a else.
LLVM_REPO_URL=${LLVM_URL:-http://llvm.org/svn/llvm-project}

# Die if any command dies.
set -e

OS="$(uname -s)"

# Parse command line options.
force_local_build=
mac_only=
run_tests=
while [[ $# > 0 ]]; do
  case $1 in
    --force-local-build)
      force_local_build=yes
      ;;
    --mac-only)
      mac_only=yes
      ;;
    --run-tests)
      run_tests=yes
      ;;
    --help)
      echo "usage: $0 [--force-local-build] [--mac-only] [--run-tests] "
      echo "--force-local-build: Don't try to download prebuilt binaries."
      echo "--mac-only: Do nothing on non-Mac systems."
      echo "--run-tests: Run tests after building. Only for local builds."
      exit 1
      ;;
  esac
  shift
done

if [[ -n "$mac_only" ]] && [[ "${OS}" != "Darwin" ]]; then
  exit 0
fi

# TODO(thakis): Remove this after Sept 29 2011. http://crbug.com/96722
for rev in 138188 138417 139029 139473 139990; do
  rm -f clang-$rev.tgz
  rm -rf clang-$rev
done

# Xcode and clang don't get along when predictive compilation is enabled.
# http://crbug.com/96315
if [[ "${OS}" = "Darwin" ]] && xcodebuild -version | grep -q 'Xcode 3.2' ; then
  XCONF=com.apple.Xcode
  if [[ "${GYP_GENERATORS}" != "make" ]] && \
     [ "$(defaults read "${XCONF}" EnablePredictiveCompilation)" != "0" ]; then
    echo
    echo "          HEARKEN!"
    echo "You're using Xcode3 and you have 'Predictive Compilation' enabled."
    echo "This does not work well with clang (http://crbug.com/96315)."
    echo "Disable it in Preferences->Building (lower right), or run"
    echo "    defaults write ${XCONF} EnablePredictiveCompilation -boolean NO"
    echo "while Xcode is not running."
    echo
  fi

  SUB_VERSION=$(xcodebuild -version | sed -Ene 's/Xcode 3\.2\.([0-9]+)/\1/p')
  if [[ "${SUB_VERSION}" < 3 ]]; then
    echo
    echo "          YOUR LD IS BUGGY!"
    echo "Please upgrade Xcode to at least 3.2.3."
    echo
  fi
fi


# Since people need to run this script anyway to compile clang, let it check out
# clang as well if it's not in DEPS, so that people don't have to change their
# DEPS if they just want to give clang a try.
CLANG_REVISION=$(grep 'clang_revision":' "${DEPS_FILE}" | egrep -o [[:digit:]]+)

# Check if there's anything to be done, exit early if not.
if [ -f "${STAMP_FILE}" ]; then
  PREVIOUSLY_BUILT_REVISON=$(cat "${STAMP_FILE}")
  if [[ -z "$force_local_build" ]] && \
       [[ "${PREVIOUSLY_BUILT_REVISON}" = "${CLANG_REVISION}" ]]; then
    echo "Clang already at ${CLANG_REVISION}"
    exit 0
  fi
fi
# To always force a new build if someone interrupts their build half way.
rm -f "${STAMP_FILE}"

# Clobber pch files, since they only work with the compiler version that
# created them.
if [[ "${OS}" = "Darwin" ]]; then
  XCODEBUILD_DIR="${THIS_DIR}/../../../xcodebuild"
  MAKE_DIR="${THIS_DIR}/../../../out"
  for CONFIG in Debug Release; do
    if [[ -d "${MAKE_DIR}/${CONFIG}/obj.target" ]]; then
      echo "Clobbering ${CONFIG} PCH files for make build"
      find "${MAKE_DIR}/${CONFIG}/obj.target" -name '*.gch' -exec rm {} +
    fi

    if [[ -d "${XCODEBUILD_DIR}/${CONFIG}/SharedPrecompiledHeaders" ]]; then
      echo "Clobbering ${CONFIG} PCH files for Xcode build"
      rm -rf "${XCODEBUILD_DIR}/${CONFIG}/SharedPrecompiledHeaders"
    fi
  done
fi

if [ -z "$force_local_build" ]; then
  # Check if there's a prebuilt binary and if so just fetch that. That's faster,
  # and goma relies on having matching binary hashes on client and server too.
  CDS_URL=https://commondatastorage.googleapis.com/chromium-browser-clang
  CDS_FILE="clang-${CLANG_REVISION}.tgz"
  CDS_OUT_DIR=$(mktemp -d -t clang_download.XXXXXX)
  CDS_OUTPUT="${CDS_OUT_DIR}/${CDS_FILE}"
  if [ "${OS}" = "Linux" ]; then
    CDS_FULL_URL="${CDS_URL}/Linux_x64/${CDS_FILE}"
  elif [ "${OS}" = "Darwin" ]; then
    CDS_FULL_URL="${CDS_URL}/Mac/${CDS_FILE}"
  fi
  echo Trying to download prebuilt clang
  if which curl > /dev/null; then
    curl -L --fail "${CDS_FULL_URL}" -o "${CDS_OUTPUT}" || \
        rm -rf "${CDS_OUT_DIR}"
  elif which wget > /dev/null; then
    wget "${CDS_FULL_URL}" -O "${CDS_OUTPUT}" || rm -rf "${CDS_OUT_DIR}"
  else
    echo "Neither curl nor wget found. Please install one of these."
    exit 1
  fi
  if [ -f "${CDS_OUTPUT}" ]; then
    rm -rf "${LLVM_BUILD_DIR}/Release+Asserts"
    mkdir -p "${LLVM_BUILD_DIR}/Release+Asserts"
    tar -xzf "${CDS_OUTPUT}" -C "${LLVM_BUILD_DIR}/Release+Asserts"
    echo clang "${CLANG_REVISION}" unpacked
    echo "${CLANG_REVISION}" > "${STAMP_FILE}"
    rm -rf "${CDS_OUT_DIR}"
    exit 0
  else
    echo Did not find prebuilt clang at r"${CLANG_REVISION}", building
  fi
fi

if grep -q 'src/third_party/llvm":' "${DEPS_FILE}"; then
  echo LLVM pulled in through DEPS, skipping LLVM update step
else
  echo Getting LLVM r"${CLANG_REVISION}" in "${LLVM_DIR}"
  if ! svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" \
                      "${LLVM_DIR}"; then
    echo Checkout failed, retrying
    rm -rf "${LLVM_DIR}"
    svn co --force "${LLVM_REPO_URL}/llvm/trunk@${CLANG_REVISION}" "${LLVM_DIR}"
  fi
fi

if grep -q 'src/third_party/llvm/tools/clang":' "${DEPS_FILE}"; then
  echo clang pulled in through DEPS, skipping clang update step
else
  echo Getting clang r"${CLANG_REVISION}" in "${CLANG_DIR}"
  svn co --force "${LLVM_REPO_URL}/cfe/trunk@${CLANG_REVISION}" "${CLANG_DIR}"
fi

# Echo all commands.
set -x

# Build clang (in a separate directory).
# The clang bots have this path hardcoded in built/scripts/slave/compile.py,
# so if you change it you also need to change these links.
mkdir -p "${LLVM_BUILD_DIR}"
cd "${LLVM_BUILD_DIR}"
if [ ! -f ./config.status ]; then
  ../llvm/configure \
      --enable-optimized \
      --disable-threads \
      --disable-pthreads \
      --without-llvmgcc \
      --without-llvmgxx
fi

NUM_JOBS=3
if [ "${OS}" = "Linux" ]; then
  NUM_JOBS="$(grep -c "^processor" /proc/cpuinfo)"
elif [ "${OS}" = "Darwin" ]; then
  NUM_JOBS="$(sysctl -n hw.ncpu)"
fi
make -j"${NUM_JOBS}"
cd -

# Build plugin.
# Copy it into the clang tree and use clang's build system to compile the
# plugin.
PLUGIN_SRC_DIR="${THIS_DIR}/../plugins"
PLUGIN_DST_DIR="${LLVM_DIR}/tools/clang/tools/chrome-plugin"
PLUGIN_BUILD_DIR="${LLVM_BUILD_DIR}/tools/clang/tools/chrome-plugin"
rm -rf "${PLUGIN_DST_DIR}"
cp -R "${PLUGIN_SRC_DIR}" "${PLUGIN_DST_DIR}"
rm -rf "${PLUGIN_BUILD_DIR}"
mkdir -p "${PLUGIN_BUILD_DIR}"
cp "${PLUGIN_SRC_DIR}/Makefile" "${PLUGIN_BUILD_DIR}"
make -j"${NUM_JOBS}" -C "${PLUGIN_BUILD_DIR}"

if [[ -n "$run_tests" ]]; then
  # Run a few tests.
  "${PLUGIN_SRC_DIR}/tests/test.sh" "${LLVM_BUILD_DIR}/Release+Asserts"
fi

# After everything is done, log success for this revision.
echo "${CLANG_REVISION}" > "${STAMP_FILE}"
