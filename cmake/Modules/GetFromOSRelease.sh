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

get_for_dragonfy()
{
	case ${ENTRY} in
	'NAME')
		sysctl -n kern.ostype
		;;
	'ID')
		sysctl -n kern.ostype | tr 'A-Z' 'a-z'
		;;
	'VERSION')
		uname -r
		;;
	'VERSION_ID')
		awk '$1 == "#define" && $2 == "__DragonFly_version" {print $3}' /usr/include/sys/param.h
		;;
	*)
		;;
	esac
}

get_for_freebsd()
{
	case ${ENTRY} in
	'NAME')
		sysctl -n kern.ostype
		;;
	'ID')
		sysctl -n kern.ostype | tr 'A-Z' 'a-z'
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
		sysctl -n kern.ostype
		;;
	'ID')
		sysctl -n kern.ostype | tr 'A-Z' 'a-z'
		;;
	'VERSION')
		uname -r
		;;
	'VERSION_ID')
		sysctl -n kern.osrevision
		;;
	*)
		;;
	esac
}

case $(uname -s) in
'DragonFly')
	get_for_dragonfy
	;;
'FreeBSD')
	get_for_freebsd
	;;
'NetBSD')
	get_for_netbsd
	;;
*)
	;;
esac
