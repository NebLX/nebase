
#include <nebase/time.h>

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static int get_for_tz(const char *tzname, int sec_of_day, time_t *abs_ts, int *delta_sec)
{
	if (setenv("TZ", tzname, 1) == -1) {
		perror("setenv");
		return -1;
	}
	tzset();
	if (neb_daytime_abs_nearest(sec_of_day, abs_ts, delta_sec) != 0) {
		fprintf(stderr, "Failed to get nearest abstime\n");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <hour of day>\n", argv[0]);
		return -1;
	}

	int hour_of_day = atoi(argv[1]);
	int sec_of_day = hour_of_day * 3600;
	time_t gmt_abs_ts, cst_abs_ts;
	int gmt_delta_sec, cst_delta_sec;

	if (get_for_tz("GMT", sec_of_day, &gmt_abs_ts, &gmt_delta_sec) != 0) {
		fprintf(stderr, "Failed to get abstime for GMT\n");
		return -1;
	}
	fprintf(stdout, "GMT %s - delta: %d, ts: %lld\n", ctime(&gmt_abs_ts), gmt_delta_sec, (long long)gmt_abs_ts);

	if (get_for_tz("Asia/Shanghai", sec_of_day, &cst_abs_ts, &cst_delta_sec) != 0) {
		fprintf(stderr, "Failed to get abstime for CST\n");
		return -1;
	}
	fprintf(stdout, "CST %s - delta: %d, ts: %lld\n", ctime(&cst_abs_ts), cst_delta_sec, (long long)cst_abs_ts);

	// we may have 1s differ as we calc them async, divide by 100 to get rid of that
	gmt_abs_ts /= 100;
	cst_abs_ts /= 100;
	if (gmt_abs_ts > cst_abs_ts && gmt_abs_ts - cst_abs_ts == 8 * 36) // both are tomorrow
		return 0;
	if (gmt_abs_ts < cst_abs_ts && cst_abs_ts - gmt_abs_ts == 16 * 36)
		return 0;

	return -1;
}
