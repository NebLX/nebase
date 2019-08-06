#!/bin/sh

set -e

SRC_DIR="$(dirname $0)/.."

temp_file=$(mktemp /tmp/nebase.tmp.XXXXXX)

clean_temp_file()
{
        [ -z "${temp_file}" ] || rm "${temp_file}"
}

trap clean_temp_file EXIT

echo "Fetching rb.c ..."

curl "http://cvsweb.netbsd.org/bsdweb.cgi/~checkout~/src/common/lib/libc/gen/rb.c?content-type=text/plain&only_with_tag=MAIN" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/compat/rbtree/rb.c"

echo "Fetching sys/rbtree.h ..."

curl "http://cvsweb.netbsd.org/bsdweb.cgi/~checkout~/src/sys/sys/rbtree.h?content-type=text/plain&only_with_tag=MAIN" -o "${temp_file}"

install -Cv -m 644 "${temp_file}" "${SRC_DIR}/include/nebase/compat/rbtree/sys/rbtree.h"

