
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/thread.h>

#include <unistd.h>
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

static GHashTable *thread_ht = NULL;

_Static_assert(sizeof(pthread_t) <= 64, "Size of pthread_t is not 64");

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
# error "fixme"
#endif
}

static gboolean thread_ht_k_equal(gconstpointer k1, gconstpointer k2)
{
	const pthread_t ptid1 = *(int64_t *)k1;
	const pthread_t ptid2 = *(int64_t *)k2;
	return pthread_equal(ptid1, ptid2) ? TRUE : FALSE;
}

static int thread_ht_add(pthread_t ptid)
{
	int64_t *k = malloc(sizeof(int64_t));
	if (!k) {
		neb_syslog(LOG_ERR, "malloc: %m");
		return -1;
	}
	*k = ptid;
	gpointer v = GINT_TO_POINTER(1);
	pthread_rwlock_wrlock(&thread_ht_rwlock);
	g_hash_table_replace(thread_ht, k, v);
	pthread_rwlock_unlock(&thread_ht_rwlock);
	return 0;
}

static void thread_ht_del(void *data)
{
	int64_t ptid;
	if (data)
		ptid = GPOINTER_TO_INT(data);
	else
		ptid = pthread_self();

	pthread_rwlock_wrlock(&thread_ht_rwlock);
	g_hash_table_remove(thread_ht, &ptid);
	pthread_rwlock_unlock(&thread_ht_rwlock);
}

static int thread_ht_exist(pthread_t ptid)
{
	int64_t k = ptid;
	int exist = 0;
	pthread_rwlock_wrlock(&thread_ht_rwlock);
	gpointer v = g_hash_table_lookup(thread_ht, &k);
	exist = GPOINTER_TO_INT(v) ? 1 : 0;
	pthread_rwlock_unlock(&thread_ht_rwlock);
	return exist;
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

	return 0;
}

void neb_thread_deinit(void)
{
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
	int ret = pthread_setspecific(thread_exit_key, GINT_TO_POINTER((int64_t)ptid));
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

int neb_thread_is_running(pthread_t ptid)
{
	return thread_ht_exist(ptid);
}
