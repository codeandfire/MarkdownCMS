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

struct growstr {				/* "growing" string */
	char *ptr;					/* pointer to the start of the string */
	char *curptr;				/* pointer to the current "position" in the string */
	int size;					/* size of total space allocated for the string */
	int growsize;				/* constant increment by which size grows */
};

void init_growstr(struct growstr *pgstr, int initsize, int growsize) {
	pgstr->ptr = (char *) calloc(initsize, sizeof(char));
	pgstr->curptr = pgstr->ptr;
	*pgstr->curptr = '\0';
	pgstr->size = initsize;
	pgstr->growsize = growsize;
}

void push_growstr(struct growstr *pgstr, char c) {
	if (pgstr->curptr + 1 > pgstr->ptr + pgstr->size - 1) {
		int offset = pgstr->curptr - pgstr->ptr;
		pgstr->ptr = (char *) reallocarray(pgstr->ptr, (pgstr->size += pgstr->growsize), sizeof(char));
		pgstr->curptr = pgstr->ptr + offset;
	}
	*pgstr->curptr++ = c;
	*pgstr->curptr = '\0';
}

void free_growstr(struct growstr *pgstr) {
	free(pgstr->ptr);
	pgstr->ptr = pgstr->curptr = NULL;
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
	struct growstr text;		/* heading text */
	bool textempty;				/* text is empty or not, i.e. has at least one non-whitespace character or not */
} heading;

void print_heading(void) {
	trim(heading.text.ptr);
	printf("<h%d>%s</h%d>", heading.number, heading.text.ptr, heading.number);
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
			free_growstr(&heading.text);
			break;
		case NO_HEADING_TEXT:
			fprintf(stderr, "no heading text");
			free_growstr(&heading.text);
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
					heading.number = 1;
					heading.textempty = true;
					init_growstr(&heading.text, HEADING_TEXT_BASE_SIZE, HEADING_TEXT_BASE_SIZE);
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
					free_growstr(&heading.text);
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
				push_growstr(&heading.text, c);
				if (!isspace(c))
					heading.textempty = false;
			}
			else if (c != EOF)
				putchar(c);
		}
	}
	return 0;
}
