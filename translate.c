#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

struct {
	int lineno;					/* current line and column numbers */
	int colno;

	/* states of individual elements */
	enum { NO_HEADING, HEADING_NUMBER, HEADING_TEXT } heading;
	enum { ONE_SPACE, TWO_SPACES, ONE_TAB, NO_WHITESPACE } whitespace;

	int heading_level;			/* level of current heading (1, 2, 3, 4, 5, 6) */
	bool syntax;				/* current character is a syntax character or not */
	bool hit_eof;				/* EOF encountered or not */
} state;

enum error_type { HEADING_LEVEL_TOO_HIGH };
int error(enum error_type, ...);

enum warning_type { MULTIPLE_SPACES_USED, TAB_USED };
void warning(enum warning_type);

const int MAX_HEADING_LEVEL = 6;

int main()
{
	int c;

	state.lineno = state.colno = 1;
	state.heading = NO_HEADING;
	state.whitespace = NO_WHITESPACE;
	state.syntax = false;
	state.hit_eof = false;

	for (; !state.hit_eof; state.syntax = false, ++state.colno) {
		switch (c = getchar()) {							/* whitespace handling */
			case ' ':
				if (state.whitespace == ONE_SPACE) {
					state.whitespace = TWO_SPACES;
					warning(MULTIPLE_SPACES_USED);
				}
				else if (state.whitespace != TWO_SPACES)	/* no preceding space(s) */
					state.whitespace = ONE_SPACE;
				break;

			case '\t':
				if (state.whitespace != ONE_TAB) {			/* no preceding tab */
					state.whitespace = ONE_TAB;
					warning(TAB_USED);
				}
				break;

			default:
				state.whitespace = NO_WHITESPACE;
		}

		state.syntax = true;								/* syntax characters */
		switch (c) {
			case '#':
				if (state.colno == 1) {
					state.heading_level = 1;
					state.heading = HEADING_NUMBER;
				}
				else if (state.heading == HEADING_NUMBER) {
					if (++state.heading_level > MAX_HEADING_LEVEL)
						return error(HEADING_LEVEL_TOO_HIGH, MAX_HEADING_LEVEL);
				}
				else
					state.syntax = false;
				break;

			case ' ':
				if (state.heading == HEADING_NUMBER) {
					printf("<h%d>", state.heading_level);
					state.heading = HEADING_TEXT;
				}
				else
					state.syntax = false;
				break;

			default:
				state.syntax = false;
		}

		if (c == EOF) {
			state.hit_eof = true;
			--state.colno;					/* EOF is not to be counted as a character */
		}
		else if (c == '\n') {
			state.colno = 0;				/* start a new line */
			++state.lineno;
		}
		if (c == '\n' || c == EOF)
			if (state.heading == HEADING_TEXT) {
				printf("</h%d>", state.heading_level);
				state.heading = NO_HEADING;
			}
		if (!state.syntax && c != EOF)
			putchar(c);
	}

	return 0;
}

int error(enum error_type et, ...)				/* variable number of arguments (...) contains data associated with
												   the error */
{
	va_list ap;

	va_start(ap, et);
	fprintf(stderr, "error: line %d column %d: ", state.lineno, state.colno);
	switch (et) {
		case HEADING_LEVEL_TOO_HIGH:
			fprintf(stderr, "HTML does not have headings beyond %d levels", va_arg(ap, int));
			break;
	}
	fprintf(stderr, "\n");
	va_end(ap);
	return 1;
}

void warning(enum warning_type wt)				/* warnings don't have any associated data, hence warning type is the
												   only argument */
{
	fprintf(stderr, "warning: line %d column %d: ", state.lineno, state.colno);
	switch (wt) {
		case MULTIPLE_SPACES_USED:
			fprintf(stderr, "multiple spaces are not rendered");
			break;
		case TAB_USED:
			fprintf(stderr, "tabs are not rendered");
			break;
	}
	fprintf(stderr, "\n");
}
