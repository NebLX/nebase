
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/thread.h>
#include <nebase/semaphore.h>

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <glib.h>

#if defined(OS_LINUX)
# include <sys/syscall.h>
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
# include <pthread_np.h>
#elif defined(OS_NETBSD)
# include <lwp.h>
#elif defined(OS_SOLARIS)
# include <sys/lwp.h>
#elif defined(OS_DARWIN)
# include <pthread.h>
#elif defined(OS_HAIKU)
# include <kernel/OS.h>
#endif

static pthread_key_t thread_exit_key = 0;
static int thread_exit_key_ok = 0;

static pthread_rwlock_t thread_ht_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int thread_ht_rwlock_ok = 0;

#define THREAD_EXIST ((void *)1)
static GHashTable *thread_ht = NULL;
_Static_assert(sizeof(pthread_t) <= 64, "Size of pthread_t is not 64");

#define THREAD_CREATE_TIMEOUT_SEC 1
#define THREAD_DESTROY_TIMEOUT_SEC 1

static neb_sem_t thread_ready_sem = NULL;

// NOTE this function should be independent of thread_ht
pid_t neb_thread_getid(void)
{
#if defined(OS_LINUX)
	return syscall(SYS_gettid);
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	return pthread_getthreadid_np();
#elif defined(OS_NETBSD) || defined(OS_SOLARIS)
	return _lwp_self();
#elif defined(OS_OPENBSD)
	return getthrid();
#elif defined(OS_DARWIN)
	uint64_t tid; //the system-wide unique integral ID of thread
	int ret = pthread_threadid_np(pthread_self(), &tid);
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_threadid_np: %m");
		return 0;
	}
	return tid;
#elif defined(OS_HAIKU)
	return get_pthread_thread_id(pthread_self());
#else
# error "fix me"
#endif
}

void neb_thread_setname(const char *name)
{
#if defined(OS_LINUX) || defined(OS_SOLARIS) // < 16
	int ret = pthread_setname_np(pthread_self(), name);
	if (ret != 0)
		neb_syslog_en(ret, LOG_ERR, "pthread_setname_np: %m");
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_OPENBSD)
	pthread_set_name_np(pthread_self(), name);
#elif defined(OS_NETBSD) // < PTHREAD_MAX_NAMELEN_NP
	int ret = pthread_setname_np(pthread_self(), "%s", name);
	if (ret != 0)
		neb_syslog_en(ret, LOG_ERR, "pthread_setname_np: %m");
#elif defined(OS_DARWIN)
	int ret = pthread_setname_np(name);
	if (ret != 0)
		neb_syslog_en(ret, LOG_ERR, "pthread_setname_np: %m");
#elif defined(OS_HAIKU) // < B_OS_NAME_LENGTH, 32bytes
	if (rename_thread(get_pthread_thread_id(pthread_self()), name) != B_OK)
		neb_syslog(LOG_ERR, "rename_thread failed");
#else
# error "fix me"
#endif
}

static gboolean thread_ht_k_equal(gconstpointer k1, gconstpointer k2)
{
	const int64_t ptid1 = *(int64_t *)k1;
	const int64_t ptid2 = *(int64_t *)k2;
	return pthread_equal((pthread_t)ptid1, (pthread_t)ptid2) ? TRUE : FALSE;
}

static int thread_ht_add(pthread_t ptid)
{
	int64_t *k = malloc(sizeof(int64_t));
	if (!k) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return -1;
	}
	*k = (int64_t)ptid;
	pthread_rwlock_wrlock(&thread_ht_rwlock);
	g_hash_table_replace(thread_ht, k, THREAD_EXIST);
	pthread_rwlock_unlock(&thread_ht_rwlock);
	return 0;
}

static void thread_ht_del(void *data)
{
	int64_t ptid;
	if (data)
		ptid = (int64_t)data;
	else
		ptid = (int64_t)pthread_self();

	pthread_rwlock_wrlock(&thread_ht_rwlock);
	g_hash_table_remove(thread_ht, &ptid);
	pthread_rwlock_unlock(&thread_ht_rwlock);
}

static int thread_ht_exist(pthread_t ptid)
{
	int64_t k = (int64_t)ptid;
	gpointer v = NULL;
	pthread_rwlock_wrlock(&thread_ht_rwlock);
	v = g_hash_table_lookup(thread_ht, &k);
	pthread_rwlock_unlock(&thread_ht_rwlock);
	return (v == THREAD_EXIST) ? 1 : 0;
}

int neb_thread_init(void)
{
	int ret = pthread_rwlock_init(&thread_ht_rwlock, NULL);
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_rwlock_init: %m");
		return -1;
	}
	thread_ht_rwlock_ok = 1;

	thread_ht = g_hash_table_new_full(g_int64_hash, thread_ht_k_equal, free, NULL);
	if (!thread_ht) {
		neb_syslog(LOG_ERR, "g_hash_table_new_full: failed");
		neb_thread_deinit();
		return -1;
	}

	ret = pthread_key_create(&thread_exit_key, thread_ht_del);
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_key_create: %m");
		neb_thread_deinit();
		return -1;
	}
	thread_exit_key_ok = 1;

	thread_ready_sem = neb_sem_notify_create(0);
	if (!thread_ready_sem) {
		neb_thread_deinit();
		return -1;
	}

	return 0;
}

void neb_thread_deinit(void)
{
	if (thread_ready_sem) {
		neb_sem_notify_destroy(thread_ready_sem);
		thread_ready_sem = NULL;
	}
	if (thread_exit_key_ok) {
		int ret = pthread_key_delete(thread_exit_key);
		if (ret != 0)
			neb_syslog_en(ret, LOG_ERR, "pthread_key_delete: %m");
		thread_exit_key_ok = 0;
	}
	if (thread_ht) {
		g_hash_table_destroy(thread_ht);
		thread_ht = NULL;
	}
	if (thread_ht_rwlock_ok) {
		int ret = pthread_rwlock_destroy(&thread_ht_rwlock);
		if (ret != 0)
			neb_syslog_en(ret, LOG_ERR, "pthread_rwlock_destroy: %m");
		thread_ht_rwlock_ok = 0;
	}
}

int neb_thread_register(void)
{
	thread_pid = neb_thread_getid();
	pthread_t ptid = pthread_self();
	int ret = pthread_setspecific(thread_exit_key, (void *)((int64_t)ptid));
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_setspecific: %m");
		return -1;
	}
	if (thread_ht_add(ptid) != 0) {
		neb_syslog(LOG_ERR, "Failed to register");
		return -1;
	}
	return 0;
}

int neb_thread_set_ready(void)
{
	return neb_sem_notify_signal(thread_ready_sem);
}

int neb_thread_create(pthread_t *ptid, const pthread_attr_t *attr,
                      void *(*start_routine) (void *), void *arg)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
		neb_syslog(LOG_ERR, "clock_gettime: %m");
		return -1;
	}
	int ret = pthread_create(ptid, attr, start_routine, arg);
	if (ret != 0) {
		neb_syslog_en(ret, LOG_ERR, "pthread_create: %m");
		return -1;
	}
	ts.tv_sec += THREAD_CREATE_TIMEOUT_SEC;
	return neb_sem_notify_timedwait(thread_ready_sem, &ts);
}

int neb_thread_is_running(pthread_t ptid)
{
	return thread_ht_exist(ptid);
}

int neb_thread_destroy(pthread_t ptid, int kill_signo, void **retval)
{
	if (!neb_thread_is_running(ptid)) {
		int ret = pthread_join(ptid, retval);
		if (ret != 0) {
			neb_syslog_en(ret, LOG_ERR, "pthread_join: %m");
			return -1;
		}
		return 0;
	} else {
		int ret = pthread_kill(ptid, kill_signo);
		if (ret != 0) {
			neb_syslog_en(ret, LOG_ERR, "pthread_kill: %m");
			return -1;
		}
#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_DFLYBSD)
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			neb_syslog(LOG_ERR, "clock_gettime: %m");
			return -1;
		}
		ts.tv_sec += THREAD_DESTROY_TIMEOUT_SEC;
		ret = pthread_timedjoin_np(ptid, retval, &ts);
		if (ret != 0) {
			neb_syslog_en(ret, LOG_ERR, "pthread_timedjoin_np: %m");
			return -1;
		}
		return 0;
#else
		for (int i = 0; i < THREAD_DESTROY_TIMEOUT_SEC * 100; i++) {
			if (!neb_thread_is_running(ptid)) {
				int ret = pthread_join(ptid, retval);
				if (ret != 0) {
					neb_syslog_en(ret, LOG_ERR, "pthread_join: %m");
					return -1;
				}
				return 0;
			}
			usleep(10000);
		}
		return -1;
#endif
	}
}
