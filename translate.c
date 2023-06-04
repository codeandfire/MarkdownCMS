#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

struct {
	int lineno;										/* current line number */
	int colno;										/* current column number */
	enum {
		NO_OPERATION, HEADING_NUMBER, HEADING_TEXT
	} operation;									/* current operation in progress */
	int heading_level;								/* level (1, 2, 3, 4, 5, 6) of the current heading */
	enum {
		ONE_SPACE, TWO_SPACES, ONE_TAB, NO_WHITESPACE
	} whitespace;									/* current state of whitespace */
	bool syntax;									/* if the current character is a Markdown syntax character or part of regular text */
	bool hit_eof;									/* have we encountered EOF or not */
} state;

enum error_type { HEADING_LEVEL_TOO_HIGH };

int error(enum error_type et, ...)
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

enum warning_type { MULTIPLE_SPACES_USED, TAB_USED };

void warning(enum warning_type wt)
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

const int HEADING_TEXT_BASE_SIZE = 50;
const int MAX_HEADING_LEVEL = 6;

int main()
{
	int c;

	for (
			state.lineno = state.colno = 1, state.operation = NO_OPERATION, state.hit_eof = false;
			!state.hit_eof;
			state.syntax = false, ++state.colno
	) {
		if ((c = getchar()) != ' ' && c != '\t')
			state.whitespace = NO_WHITESPACE;

		switch (c) {
			case '#':
				state.syntax = true;
				if (state.colno == 1) {
					state.heading_level = 1;
					state.operation = HEADING_NUMBER;
				}
				else if (state.operation == HEADING_NUMBER) {
					if (++state.heading_level > MAX_HEADING_LEVEL)
						return error(HEADING_LEVEL_TOO_HIGH, MAX_HEADING_LEVEL);
				}
				else
					state.syntax = false;
				break;

			case ' ':
				if (state.operation == HEADING_NUMBER) {
					state.syntax = true;
					printf("<h%d>", state.heading_level);
					state.operation = HEADING_TEXT;
				}
				switch (state.whitespace) {
					case NO_WHITESPACE:
					case ONE_TAB:
						state.whitespace = ONE_SPACE;
						break;
					case ONE_SPACE:
						state.whitespace = TWO_SPACES;
						warning(MULTIPLE_SPACES_USED);
						break;
				}
				break;

			case '\t':
				switch (state.whitespace) {
					case NO_WHITESPACE:
					case ONE_SPACE:
					case TWO_SPACES:
						state.whitespace = ONE_TAB;
						warning(TAB_USED);
						break;
				}
				break;

			case EOF:
				state.hit_eof = true;

			case '\n':				/* fall-through intended here: on encountering EOF do the same things that
										would be done on encountering a newline */
				if (state.operation == HEADING_TEXT) {
					printf("</h%d>", state.heading_level);
					state.operation = NO_OPERATION;
				}
				state.colno = 0;
				++state.lineno;
				break;
		}

		if (!state.syntax && c != EOF)
			putchar(c);
	}

	return 0;
}
