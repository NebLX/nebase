
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sem.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <errno.h>

#if defined(OS_LINUX)
union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};
#endif

int neb_sem_proc_create(const char *path, int nsems)
{
	key_t key = ftok(path, 1);
	if (key == -1) {
		neb_syslog(LOG_ERR, "ftok(%s): %m", path);
		return -1;
	}

	int semid = semget(key, nsems, O_CREAT | O_EXCL | 0600);
	if (semid == -1) {
		neb_syslog(LOG_ERR, "semget: %m");
		return -1;
	}

	union semun arg = {.val = 0};
	for (int i = 0; i < nsems; i++) {
		if (semctl(semid, 0, SETVAL, arg) != 0) {
			neb_syslog(LOG_ERR, "semctl: %m");
			neb_sem_proc_destroy(semid);
			return -1;
		}
	}

	return semid;
}

int neb_sem_proc_destroy(int semid)
{
	if (semctl(semid, 0, IPC_RMID) != 0) {
		neb_syslog(LOG_ERR, "semctl(IPC_RMID): %m");
		return -1;
	}
	return 0;
}

int neb_sem_proc_setval(int semid, int subid, int value)
{
	union semun arg = {.val = value};
	if (semctl(semid, subid, SETVAL, arg) != 0) {
		neb_syslog(LOG_ERR, "semctl: %m");
		return -1;
	}
	return 0;
}

int neb_sem_proc_post(int semid, int subid)
{
	struct sembuf sb = {
		.sem_num = subid,
		.sem_op = 1,
		.sem_flg = IPC_NOWAIT,
	};

	for (;;) {
		if (semop(semid, &sb, 1) == -1) {
			if (errno == EINTR)
				continue;
			neb_syslog(LOG_ERR, "semop: %m");
			return -1;
		}
		return 0;
	}
}

int neb_sem_proc_wait_count(int semid, int subid, int count, struct timespec *timeout)
{
	struct sembuf sb = {
		.sem_num = subid,
		.sem_op = 0 - count,
		.sem_flg = 0,
	};

	if (semtimedop(semid, &sb, 1, timeout) == -1) {
		if (errno != EINTR)
			neb_syslog(LOG_ERR, "semtimedop: %m");
		return -1;
	}

	return 0;
}

int neb_sem_proc_wait_zerod(int semid, int subid, struct timespec *timeout)
{
	struct sembuf sb = {
		.sem_num = subid,
		.sem_op = 0,
		.sem_flg = 0,
	};

	if (semtimedop(semid, &sb, 1, timeout) == -1) {
		if (errno != EINTR)
			neb_syslog(LOG_ERR, "semtimedop: %m");
		return -1;
	}

	return 0;
}
