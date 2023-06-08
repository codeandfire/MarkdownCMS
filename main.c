#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "translate.h"

#define	streq(STR1, STR2)	(strcmp(STR1, STR2) == 0)

int main(int argc, char *argv[])
{
	struct options opt;

	opt.debug = false;
	opt.nodoc = false;

	while (--argc > 0)
		if ((*++argv)[0] == '-') {
			if (streq(++*argv, "debug"))
				opt.debug = true;
			else if (streq(*argv, "nodoc"))
				opt.nodoc = true;
		}

	return translate(stdin, stdout, stderr, &opt);
}
