/*
 * meant as an example target
 *
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

extern char **environ;

static
int
wonk(char *p, char *q)
{
	printf("T> %s::%s: wonk says p=%s and q=%s\n", __FILE__, __func__, p, q);
	return 0;
}

int
main(int argc, char **argv)
{
	unsigned i;

//	__builtin_debugtrap();
//	asm("int $0x3\n");

	printf("T>\n");
	for (i = 0; i < argc; i++) {
		printf("T> argv[%d]='%s'\n", i, argv[i]);
	}
	printf("T>\n");
	wonk(argv[0], argv[1]);
	printf("T>\n");
	return 0;
}
