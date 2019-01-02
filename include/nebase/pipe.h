
#ifndef NEB_PIPE_H
#define NEB_PIPE_H 1

#include "cdefs.h"

/**
 * \param[out] pipefd will be nonblock and cloexec
 * \return -1 if failed, 0 if success
 */
extern int neb_pipe_new(int pipefd[2])
	__attribute_warn_unused_result__;

#endif
