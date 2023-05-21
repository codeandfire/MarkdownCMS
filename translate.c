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

struct {
	int lineno;					/* current line number */
	int colno;					/* current column number */
	enum {
		NONE,
		HEADING_NUMBER,
		HEADING_TEXT
	} operation;				/* current operation in progress */
	bool syntax;				/* if the current character is a Markdown syntax character or part of regular text */
	bool hit_eof;				/* have we encountered EOF or not */
} state;

struct {
	int number;					/* heading number (1, 2, 3, 4, 5, 6) */
	char *textptr;				/* pointer to (starting position of) text */
	char *textcurptr;			/* pointer to latest position in text */
	int textsize;				/* size of space allocated for text */
	bool textempty;				/* text is empty or not, i.e. has at least one non-whitespace character or not */
} heading;

void init_heading(int textsize)
{
	heading.number = 0;
	heading.textsize = textsize;
	heading.textptr = (char *) calloc(textsize, sizeof(char));
	heading.textcurptr = heading.textptr;
	*heading.textcurptr = '\0';
	heading.textempty = true;
}

void print_heading(void)
{
	trim(heading.textptr);
	printf("<h%d>%s</h%d>", heading.number, heading.textptr, heading.number);
	free(heading.textptr);
	heading.number = 0;				/* reset all fields */
	heading.textsize = 0;
	heading.textptr = heading.textcurptr = NULL;
	heading.textempty = true;
}

void grow_heading(int sizeincr)
{
	int offset = heading.textcurptr - heading.textptr;
	heading.textsize += sizeincr;
	heading.textptr = (char *) reallocarray(heading.textptr, heading.textsize, sizeof(char));
	heading.textcurptr = heading.textptr + offset;
}

enum syntax_errtype {
	HEADING_NUMBER_TOO_HIGH,
	NO_HEADING_TEXT
};

int syntax_err(enum syntax_errtype et, ...)
{
	va_list ap;

	va_start(ap, et);
	fprintf(stderr, "syntax error: line %d column %d: ", state.lineno, state.colno);
	switch (et) {
		case HEADING_NUMBER_TOO_HIGH:
			fprintf(stderr, "HTML does not have headings beyond %d levels", va_arg(ap, int));
			break;
		case NO_HEADING_TEXT:
			fprintf(stderr, "no heading text");
			break;
	}
	fprintf(stderr, "\n");
	va_end(ap);
	return 1;
}

const int HEADING_TEXT_BASE_SIZE = 50;
const int MAX_HEADING_NUMBER = 6;

int main()
{
	int c;

	for (state.lineno = state.colno = 1, state.operation = NONE, state.hit_eof = false; !state.hit_eof; state.syntax = false, ++state.colno) {
		switch (c = getchar()) {
			case '#':
				state.syntax = true;
				if (state.colno == 1) {
					init_heading(HEADING_TEXT_BASE_SIZE);
					++heading.number;
					state.operation = HEADING_NUMBER;
				}
				else if (state.operation == HEADING_NUMBER) {
					if (++heading.number > MAX_HEADING_NUMBER)
						return syntax_err(HEADING_NUMBER_TOO_HIGH, MAX_HEADING_NUMBER);
				}
				else
					state.syntax = false;
				break;

			case EOF:
				state.hit_eof = true;
				--state.colno;		/* EOF should not be counted as a character; this matters for the syntax_err()
									   calls below because they print out state.colno */
			case '\n':				/* fall-through intended here: on encountering EOF do the same things that
										would be done on encountering a newline */
				if (state.operation == HEADING_NUMBER || state.operation == HEADING_TEXT) {
					if (heading.textempty)		/* if state.operation == HEADING_NUMBER then certainly heading.textempty == true */
						return syntax_err(NO_HEADING_TEXT);
					print_heading();
					state.operation = NONE;
				}
				state.colno = 0;
				++state.lineno;
				break;
		}
		if (!state.syntax) {
			if (state.operation == HEADING_NUMBER || state.operation == HEADING_TEXT) {
				if (state.operation == HEADING_NUMBER)
					state.operation = HEADING_TEXT;
				if (heading.textcurptr + 1 > heading.textptr + heading.textsize - 1)
					grow_heading(HEADING_TEXT_BASE_SIZE);
				if (!isspace(c))
					heading.textempty = false;
				*heading.textcurptr++ = c;
				*heading.textcurptr = '\0';
			}
			else if (c != EOF)
				putchar(c);
		}
	}
	return 0;
}
