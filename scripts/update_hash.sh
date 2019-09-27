#!/bin/sh

set -e

SRC_DIR="$(dirname $0)/.."

temp_file=$(mktemp /tmp/nebase.tmp.XXXXXX)

clean_temp_file()
{
        [ -z "${temp_file}" ] || rm "${temp_file}"
}

trap clean_temp_file EXIT

echo "Fetching hash.c ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/hash.c" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/hash/hash.c"

echo "Fetching bitrotate.h ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/bitrotate.h" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/hash/bitrotate.h"

echo "Fetching xalloc-oversized.h ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/xalloc-oversized.h" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/hash/xalloc-oversized.h"

echo "Fetching hash.h ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/hash.h" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/include/nebase/compat/hash/sys/hash.h"

