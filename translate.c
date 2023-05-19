#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

void trim(char *s)
{
	int i, j, l;

	for (i = 0; isspace(s[i]); ++i)		/* i goes to the first non-whitespace character from the left */
		;
	for (j = i; s[j] != '\0'; ++j)
		;
	for (--j; isspace(s[j]); --j)		/* j goes to the first non-whitespace character from the right */
		;
	for (l = i; l <= j; ++l)
		s[l-i] = s[l];
	s[l-i] = '\0';
}

int lineno = 1, colno = 1;

struct {
	int number;
	char *textptr;
	char *textcurptr;
	int textsize;						/* total amount of space allocated for text */
} heading;

void init_heading(int textsize)
{
	heading.number = 0;
	heading.textsize = textsize;
	heading.textptr = (char *) calloc(textsize, sizeof(char));
	heading.textcurptr = heading.textptr;
	*heading.textcurptr = '\0';
}

void print_heading(void)
{
	trim(heading.textptr);
	printf("<h%d>%s</h%d>", heading.number, heading.textptr, heading.number);
	free(heading.textptr);
	heading.number = 0;				/* reset all fields */
	heading.textsize = 0;
	heading.textptr = heading.textcurptr = NULL;
}

void grow_heading(int sizeincr)
{
	int offset = heading.textcurptr - heading.textptr;
	heading.textsize += sizeincr;
	heading.textptr = (char *) reallocarray(heading.textptr, heading.textsize, sizeof(char));
	heading.textcurptr = heading.textptr + offset;
}

enum syntax_errtype { HEADING_NUMBER_TOO_HIGH };
int syntax_err(enum syntax_errtype et, ...)
{
	va_list ap;

	va_start(ap, et);
	fprintf(stderr, "syntax error: line %d column %d: ", lineno, colno);
	switch (et) {
		case HEADING_NUMBER_TOO_HIGH:
			fprintf(stderr, "HTML does not have headings beyond %d levels", va_arg(ap, int));
			break;
	}
	va_end(ap);
	fprintf(stderr, "\n");
	return 1;
}

const int HEADING_TEXT_BASE_SIZE = 50;
const int MAX_HEADING_NUMBER = 6;

enum { NONE, HEADING_NUMBER, HEADING_TEXT } state;

int main()
{
	int c;
	bool syntax;		/* true if the current character is a Markdown syntax character and false if
						   it is regular text */

	for (; (c = getchar()) != EOF; syntax = false, ++colno) {
		switch (c) {
			case '#':
				syntax = true;
				if (colno == 1) {
					init_heading(HEADING_TEXT_BASE_SIZE);
					++heading.number;
					state = HEADING_NUMBER;
				}
				else if (state == HEADING_NUMBER) {
					if (++heading.number > MAX_HEADING_NUMBER)
						return syntax_err(HEADING_NUMBER_TOO_HIGH, MAX_HEADING_NUMBER);
				}
				else
					syntax = false;
				break;

			case '\n':
				if (state == HEADING_TEXT) {
					print_heading();
					state = NONE;
				}
				colno = 0, ++lineno;
				break;
		}
		if (!syntax) {
			if (state == HEADING_NUMBER || state == HEADING_TEXT) {
				if (state == HEADING_NUMBER)
					state = HEADING_TEXT;
				if (heading.textcurptr + 1 > heading.textptr + heading.textsize - 1)
					grow_heading(HEADING_TEXT_BASE_SIZE);
				*heading.textcurptr++ = c;
				*heading.textcurptr = '\0';
			}
			else
				putchar(c);
		}
	}

	return 0;
}
