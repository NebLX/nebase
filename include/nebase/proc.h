
#ifndef NEB_PROC_H
#define NEB_PROC_H 1

#include "cdefs.h"

extern void neb_proc_child_exit(int status)
	_nattr_noreturn;
extern void neb_proc_child_flush_exit(int status)
	_nattr_noreturn;

#endif
