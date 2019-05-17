#!/bin/sh

set -e

ENTRY=$1
[ -n "${ENTRY}" ] || exit 0

if [ -f /etc/os-release ]
then
	. /etc/os-release

	eval "echo \$${ENTRY}"
	exit 0
fi

get_for_freebsd()
{
	case ${ENTRY} in
	'NAME')
		sysctl kern.ostype | awk '{print $2}'
		;;
	'ID')
		sysctl kern.ostype | awk '{print $2}' | tr 'A-Z' 'a-z'
		;;
	'VERSION')
		uname -r
		;;
	'VERSION_ID')
		uname -K
		;;
	*)
		;;
	esac
}

get_for_netbsd()
{
	case ${ENTRY} in
	'NAME')
		sysctl kern.ostype | awk '{print $2}'
		;;
	'ID')
		sysctl kern.ostype | awk '{print $2}' | tr 'A-Z' 'a-z'
		;;
	'VERSION')
		uname -r
		;;
	'VERSION_ID')
		sysctl kern.osrevision
		;;
	*)
		;;
	esac
}

case $(uname -s) in
'FreeBSD')
	get_for_freebsd
	;;
'NetBSD')
	get_for_netbsd
	;;
*)
	;;
esac
