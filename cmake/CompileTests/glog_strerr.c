
#include <stdio.h>
#include <string.h>

#include <glib.h>

const char glog_domain[] = "test";

static void glog_handler(const gchar *log_domain, GLogLevelFlags log_level,
                         const gchar *message, gpointer user_data)
{
	int *errmsg_ok = user_data;
	fprintf(stdout, "Message: %s\n", message);
	if (strcmp(message, "m") == 0)
		*errmsg_ok = 0;
	else
		*errmsg_ok = 1;
}

int main(int argc, char *argv)
{
	int errmsg_ok = 0;
	g_log_set_handler(glog_domain, G_LOG_LEVEL_MASK, glog_handler, &errmsg_ok);
	g_log(glog_domain, G_LOG_LEVEL_MESSAGE, "%m");
	if (errmsg_ok)
		return 0;
	else
		return 1;
}
