
#include <nebase/cdefs.h>
#include <nebase/syslog.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

static const char *log_file = "/tmp/.nebase.test";
static const char *log_domain = "nebase_test";
static const char log_content[] = "log for test";

static void glog_handler(const gchar *log_domain _nattr_unused, GLogLevelFlags log_level _nattr_unused,
                         const gchar *message, gpointer data)
{
	FILE *stream = data;
	fprintf(stream, "%s", message);
}

int main(void)
{
	unlink(log_file);

	FILE *stream = fopen(log_file, "w");
	if (!stream) {
		perror("fopen(w)");
		return -1;
	}

	guint id = g_log_set_handler(log_domain, G_LOG_LEVEL_MASK, glog_handler, stream);

	neb_syslog_init(NEB_LOG_GLOG, log_domain);
	neb_syslog(LOG_INFO, "%s", log_content);

	g_log_remove_handler(log_domain, id);
	fclose(stream);

	char buf[sizeof(log_content) + 32];
	stream = fopen(log_file, "r");
	if (!stream) {
		perror("fopen(r)");
		return -1;
	}
	size_t nr = fread(buf, 1, sizeof(buf) - 1, stream);
	if (ferror(stream)) {
		perror("fread");
		fclose(stream);
		return -1;
	}
	fclose(stream);
	if (nr == 0) {
		fprintf(stderr, "no data read in file %s\n", log_file);
		return -1;
	}
	buf[nr] = '\0';

	fprintf(stdout, "log_content: %s\n", log_content);
	fprintf(stdout, "read back  : %s\n", buf);
	if (!strstr(buf, log_content)) {
		fprintf(stderr, "content not found in file %s\n", log_file);
		return -1;
	}

	unlink(log_file);
	return 0;
}
