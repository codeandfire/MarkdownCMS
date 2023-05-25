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
		NO_OPERATION,
		HEADING_NUMBER,
		HEADING_TEXT
	} operation;				/* current operation in progress */
	enum {
		SPACETAB_SEQUENCE_OVER,
		ONE_SPACE,
		TWO_SPACES,
		ONE_TAB
	} cons_whitespace;			/* consecutive whitespace: a sequence of consecutive spaces or tabs
								   (NOTE: we can have a sequence of either spaces or either tabs, we don't count
								   mixed space/tab sequences) */
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

enum syntaxerr_type {
	HEADING_NUMBER_TOO_HIGH,
	NO_HEADING_TEXT
};

int syntaxerr(enum syntaxerr_type et, ...)
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

enum warning_type {
	MULTIPLE_SPACES_USED,
	TAB_USED
};

void warning(enum warning_type wt)
{
	fprintf(stderr, "warning: line %d column %d: ", state.lineno, state.colno);
	switch (wt) {
		case MULTIPLE_SPACES_USED:
			fprintf(stderr, "use of multiple spaces for increased spacing does not work since HTML collapses them down to one space while rendering");
			break;
		case TAB_USED:
			fprintf(stderr, "use of tab for increased spacing / alignment does not work since HTML converts tabs to one space while rendering");
	}
	fprintf(stderr, "\n");
}

const int HEADING_TEXT_BASE_SIZE = 50;
const int MAX_HEADING_NUMBER = 6;

int main()
{
	int c;

	for (
			state.lineno = state.colno = 1, state.operation = NO_OPERATION, state.cons_whitespace = SPACETAB_SEQUENCE_OVER, state.hit_eof = false;
			!state.hit_eof;
			state.syntax = false, ++state.colno
	) {
		c = getchar();
		if (
				((state.cons_whitespace == ONE_SPACE || state.cons_whitespace == TWO_SPACES) && c != ' ')
				|| (state.cons_whitespace == ONE_TAB && c != '\t')
		)
			/* if we are coming from a space sequence and encountering a non-space character, if we are coming
			 * from a tab sequence and encountering a non-tab character, in both cases end the sequence */
			state.cons_whitespace = SPACETAB_SEQUENCE_OVER;

		switch (c) {
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
						return syntaxerr(HEADING_NUMBER_TOO_HIGH, MAX_HEADING_NUMBER);
				}
				else
					state.syntax = false;
				break;

			case ' ':
				switch (state.cons_whitespace) {
					case SPACETAB_SEQUENCE_OVER:
						state.cons_whitespace = ONE_SPACE;
						break;
					case ONE_SPACE:
						state.cons_whitespace = TWO_SPACES;
						warning(MULTIPLE_SPACES_USED);
						break;
					case TWO_SPACES:
						break;		/* do nothing: no further warnings required */
				}
				break;

			case '\t':
				switch (state.cons_whitespace) {
					case SPACETAB_SEQUENCE_OVER:
						state.cons_whitespace = ONE_TAB;
						warning(TAB_USED);
						break;
					case ONE_TAB:
						break;		/* do nothing: no further warnings required */
				}
				break;

			case EOF:
				state.hit_eof = true;
				--state.colno;		/* EOF should not be counted as a character; this matters for the syntaxerr()
									   calls below because they print out state.colno */
			case '\n':				/* fall-through intended here: on encountering EOF do the same things that
										would be done on encountering a newline */
				if (state.operation == HEADING_NUMBER || state.operation == HEADING_TEXT) {
					if (heading.textempty)		/* if state.operation == HEADING_NUMBER then certainly heading.textempty == true */
						return syntaxerr(NO_HEADING_TEXT);
					print_heading();
					free_growstr(&heading.text);
					state.operation = NO_OPERATION;
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
