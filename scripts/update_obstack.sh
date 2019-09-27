#!/bin/sh

set -e

SRC_DIR="$(dirname $0)/.."

temp_file=$(mktemp /tmp/nebase.tmp.XXXXXX)

clean_temp_file()
{
        [ -z "${temp_file}" ] || rm "${temp_file}"
}

trap clean_temp_file EXIT

echo "Fetching obstack.c ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/obstack.c" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/obstack/obstack.c"

echo "Fetching obstack.h ..."

curl "https://git.savannah.gnu.org/gitweb/?p=gnulib.git;a=blob_plain;f=lib/obstack.h" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/include/nebase/compat/obstack/obstack.h"

