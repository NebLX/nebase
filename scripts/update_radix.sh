#!/bin/sh

set -e

SRC_DIR="$(dirname $0)/.."

temp_file=$(mktemp /tmp/nebase.tmp.XXXXXX)

clean_temp_file()
{
        [ -z "${temp_file}" ] || rm "${temp_file}"
}

trap clean_temp_file EXIT

echo "Fetching radix.c ..."

curl "https://svnweb.freebsd.org/base/head/sys/net/radix.c?view=co" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/net_radix/radix.c"

echo "Fetching radix.h ..."

curl "https://svnweb.freebsd.org/base/head/sys/net/radix.h?view=co" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/include/nebase/compat/net_radix/freebsd/radix.h"

