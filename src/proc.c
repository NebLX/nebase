
#include <nebase/proc.h>

#include <stdio.h>
#include <unistd.h>

void neb_proc_child_exit(int status)
{
	/*
	 * Child process should not call exit(3) if
	 * 1. share stdio with parent
	 * 2. has called atexit(3)
	 */
	_exit(status);
}

void neb_proc_child_flush_exit(int status)
{
	if (stderr)
		fflush(stderr);
	if (stdout)
		fflush(stdout);
	neb_proc_child_exit(status);
}
