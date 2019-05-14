#!/bin/sh

[ -f /etc/os-release ] || exit 0

. /etc/os-release

eval "echo \$$1"
