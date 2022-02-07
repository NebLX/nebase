#!/bin/sh

set -e

SRC_DIR="$(dirname $0)/.."

temp_file=$(mktemp /tmp/nebase.tmp.XXXXXX)

clean_temp_file()
{
	[ -z "${temp_file}" ] || rm "${temp_file}"
}

trap clean_temp_file EXIT

echo "Checking XNU Version ..."

xnu_version=$(curl "https://opensource.apple.com/source/xnu/" | sed -n 's/.*>\(xnu-[1-9][0-9.]*\)<.*/\1/p' | sort -V | tail -n 1)

echo "XNU Version: ${xnu_version}"

echo "Fetching sys/unpcb.h ..."

curl "https://opensource.apple.com/source/xnu/${xnu_version}/bsd/sys/unpcb.h" -o "${temp_file}"

echo "Installing sys/unpcb.h ..."
install -d -m 755 "${SRC_DIR}/private/include/sys"
install -Cv -m 644 "${temp_file}" "${SRC_DIR}/private/include/sys/unpcb.h"

