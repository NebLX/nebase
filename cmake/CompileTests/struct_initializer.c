
#include <stdio.h>

struct test {
	int a;
	char c[4];
};

int main(int argc, char *argv[])
{
	struct test t = {};
	t.c[0] = '0';

	return t.a;
}
