
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/thread.h>
#include <nebase/rbtree.h>
#include <nebase/sem.h>
#include <nebase/time.h>

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#if defined(OS_LINUX)
# include <sys/syscall.h>
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_OPENBSD)
# include <pthread_np.h>
#elif defined(OS_NETBSD)
# include <lwp.h>
#elif defined(OSTYPE_SUN)
# include <sys/lwp.h>
#elif defined(OS_DARWIN)
# include <os/lock.h>
#elif defined(OS_HAIKU)
# include <kernel/OS.h>
#endif

static int thread_rbt_cmp_node(void *context, const void *node1, const void *node2);
static int thread_rbt_cmp_key(void *context, const void *node, const void *key);
struct thread_rbt_node {
	rb_node_t rbtree_context;
	int64_t ptid;
};
_Static_assert(sizeof(pthread_t) <= 64, "Size of pthread_t is more than 64");
static rb_tree_ops_t thread_rbt_ops = {
	.rbto_compare_nodes = thread_rbt_cmp_node,
	.rbto_compare_key = thread_rbt_cmp_key,
	.rbto_node_offset = offsetof(struct thread_rbt_node, rbtree_context),
	.rbto_context = NULL,
};

static pthread_key_t thread_exit_key = 0;
static int thread_exit_key_ok = 0;

#if defined(OS_DARWIN)
static os_unfair_lock thread_rbt_lock = OS_UNFAIR_LOCK_INIT;
#else
static pthread_spinlock_t thread_rbt_lock;;
static int thread_rbt_lock_ok = 0;
#endif

static rb_tree_t thread_rbt;
static int thread_rbt_ok = 0;

#define THREAD_CREATE_TIMEOUT_SEC 1
#define THREAD_DESTROY_TIMEOUT_SEC 1

static neb_sem_t thread_ready_sem = NULL;

static void thread_rbt_lock_lock(void)
{
#if defined(OS_DARWIN)
	os_unfair_lock_lock(&thread_rbt_lock);
#else
	pthread_spin_lock(&thread_rbt_lock);
#endif
}

static void thread_rbt_lock_unlock(void)
{
#if defined(OS_DARWIN)
	os_unfair_lock_unlock(&thread_rbt_lock);
#else
	pthread_spin_unlock(&thread_rbt_lock);
#endif
}

static struct thread_rbt_node *thread_rbt_node_new(int64_t ptid)
{
	struct thread_rbt_node *n = calloc(1, sizeof(struct thread_rbt_node));
	if (!n) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	n->ptid = ptid;
	return n;
}

static void thread_rbt_node_del(struct thread_rbt_node *n)
{
	free(n);
}

static int thread_rbt_cmp_node(void *context _nattr_unused, const void *node1, const void *node2)
{
	const struct thread_rbt_node *e = node1;
	const struct thread_rbt_node *p = node2;
	if (e->ptid < p->ptid)
		return -1;
	else if (e->ptid == p->ptid)
		return 0;
	else
		return 1;
}

static int thread_rbt_cmp_key(void *context _nattr_unused, const void *node, const void *key)
{
	const struct thread_rbt_node *e = node;
	int64_t ptid = *(int64_t *)key;
	if (e->ptid < ptid)
		return -1;
	else if (e->ptid == ptid)
		return 0;
	else
		return 1;
}

// NOTE this function should be independent of thread_rbt
pid_t neb_thread_getid(void)
{
#if defined(OS_LINUX)
# if __GLIBC_PREREQ(2, 30)
	return gettid();
# else
	return syscall(SYS_gettid);
# endif
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD)
	return pthread_getthreadid_np();
#elif defined(OS_NETBSD) || defined(OSTYPE_SUN)
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
#if defined(OS_LINUX) || defined(OSTYPE_SUN) // < 16 for Linux, < 32 for SunOS
	int ret = pthread_setname_np(pthread_self(), name);
	if (ret != 0)
		neb_syslog_en(ret, LOG_ERR, "pthread_setname_np: %m");
#elif defined(OS_FREEBSD) || defined(OS_DFLYBSD) || defined(OS_OPENBSD)
	pthread_set_name_np(pthread_self(), name);
#elif defined(OS_NETBSD) // < PTHREAD_MAX_NAMELEN_NP
	int ret = pthread_setname_np(pthread_self(), "%s", (void *)name);
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

static int thread_rbt_add(pthread_t ptid)
{
	struct thread_rbt_node *node = thread_rbt_node_new((int64_t)ptid);
	if (!node)
		return -1;
	int ret = 0;
	thread_rbt_lock_lock();
	struct thread_rbt_node *tmp = rb_tree_insert_node(&thread_rbt, node);
	if (tmp != node) {
		thread_rbt_node_del(node);
		neb_syslog(LOG_CRIT, "thread %lld already existed", (long long)ptid);
		ret = -1;
	}
	thread_rbt_lock_unlock();
	return ret;
}

static void thread_rbt_del(void *data)
{
	int64_t key;
	if (data)
		key = (int64_t)data;
	else
		key = (int64_t)pthread_self();

	thread_rbt_lock_lock();
	void *node = rb_tree_find_node(&thread_rbt, &key);
	rb_tree_remove_node(&thread_rbt, node);
	thread_rbt_node_del(node);
	thread_rbt_lock_unlock();
}

static bool thread_rbt_exist(pthread_t ptid)
{
	void *node;
	int64_t key = (int64_t)ptid;
	thread_rbt_lock_lock();
	node = rb_tree_find_node(&thread_rbt, &key);
	thread_rbt_lock_unlock();
	return node != NULL;
}

int neb_thread_init(void)
{
#if defined(OS_DARWIN)
	int ret;
#else
	int ret = pthread_spin_init(&thread_rbt_lock, PTHREAD_PROCESS_PRIVATE);
	if (ret != 0) {
		neb_syslogl_en(ret, LOG_ERR, "pthread_spin_init: %m");
		return -1;
	}
	thread_rbt_lock_ok = 1;
#endif

	rb_tree_init(&thread_rbt, &thread_rbt_ops);
	thread_rbt_ok = 1;

	ret = pthread_key_create(&thread_exit_key, thread_rbt_del);
	if (ret != 0) {
		neb_syslogl_en(ret, LOG_ERR, "pthread_key_create: %m");
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
			neb_syslogl_en(ret, LOG_ERR, "pthread_key_delete: %m");
		thread_exit_key_ok = 0;
	}
	if (thread_rbt_ok) {
		struct thread_rbt_node *node, *next;
		RB_TREE_FOREACH_SAFE(node, &thread_rbt, next) {
			rb_tree_remove_node(&thread_rbt, node);
			thread_rbt_node_del(node);
		}
		thread_rbt_ok = 0;
	}
#if defined(OS_DARWIN)
#else
	if (thread_rbt_lock_ok) {
		int ret = pthread_spin_destroy(&thread_rbt_lock);
		if (ret != 0)
			neb_syslogl_en(ret, LOG_ERR, "pthread_spin_destroy: %m");
		thread_rbt_lock_ok = 0;
	}
#endif
}

int neb_thread_register(void)
{
	thread_pid = neb_thread_getid();
	pthread_t ptid = pthread_self();
	int ret = pthread_setspecific(thread_exit_key, (void *)((int64_t)ptid));
	if (ret != 0) {
		neb_syslogl_en(ret, LOG_ERR, "pthread_setspecific: %m");
		return -1;
	}
	if (thread_rbt_add(ptid) != 0) {
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
	if (neb_time_gettimeofday(&ts) != 0)
		return -1;
	int ret = pthread_create(ptid, attr, start_routine, arg);
	if (ret != 0) {
		neb_syslogl_en(ret, LOG_ERR, "pthread_create: %m");
		return -1;
	}
	ts.tv_sec += THREAD_CREATE_TIMEOUT_SEC;
	return neb_sem_notify_timedwait(thread_ready_sem, &ts);
}

bool neb_thread_is_running(pthread_t ptid)
{
	return thread_rbt_exist(ptid);
}

int neb_thread_destroy(pthread_t ptid, int kill_signo, void **retval)
{
	if (!neb_thread_is_running(ptid)) {
		int ret = pthread_join(ptid, retval);
		if (ret != 0) {
			neb_syslogl_en(ret, LOG_ERR, "pthread_join: %m");
			return -1;
		}
		return 0;
	} else {
		int ret = pthread_kill(ptid, kill_signo);
		if (ret != 0) {
			neb_syslogl_en(ret, LOG_ERR, "pthread_kill: %m");
			return -1;
		}
		// TODO use pthread_clockjoin_np from GLIBC 2.31
#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_DFLYBSD)
		struct timespec ts;
		if (neb_time_gettimeofday(&ts) != 0)
			return -1;
		ts.tv_sec += THREAD_DESTROY_TIMEOUT_SEC;
		ret = pthread_timedjoin_np(ptid, retval, &ts);
		if (ret != 0) {
			neb_syslogl_en(ret, LOG_ERR, "pthread_timedjoin_np: %m");
			return -1;
		}
		return 0;
#else
		for (int i = 0; i < THREAD_DESTROY_TIMEOUT_SEC * 100; i++) {
			if (!neb_thread_is_running(ptid)) {
				int ret = pthread_join(ptid, retval);
				if (ret != 0) {
					neb_syslogl_en(ret, LOG_ERR, "pthread_join: %m");
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
