#include <stdio.h>

struct options {
	bool debug;
	bool nodoc;
};
int translate(FILE *, FILE *, FILE *, struct options *);
