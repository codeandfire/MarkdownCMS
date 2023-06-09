#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

#include "translate.h"

struct {
	int lineno;
	int colno;

	enum { NO_HEADING, HEADING_LEVEL, HEADING_TEXT } heading;
	enum { NO_WHITESPACE, ONE_SPACE, TWO_SPACES, ONE_TAB } whitespace;

	int heading_level;
	bool syntax;
	bool escaped;
	bool hit_eof;
} state;

void docstart(FILE *);
void docend(FILE *);

void dumpstate(FILE *, char);

enum error_type { HEADING_LEVEL_TOO_HIGH, HEADING_SYNTAX_SPACE };
int error(FILE *, enum error_type, ...);

enum warning_type { MULTIPLE_SPACES_USED, TAB_USED };
void warning(FILE *, enum warning_type);

const int MAX_HEADING_LEVEL = 6;

bool is_new_utf8char(unsigned char c)
{
	return !(c >= 0x80 && c <= 0xBF);				/* not UTF-8 continuation byte */
}

int translate(FILE *fin, FILE *fout, FILE *ferr, struct options *popt)
{
	char c;

	state.lineno = state.colno = 1;
	state.heading = NO_HEADING;
	state.whitespace = NO_WHITESPACE;
	state.heading_level = 0;
	state.syntax = false;
	state.escaped = false;
	state.hit_eof = false;

#define	html(CALL)	if (!popt->debug) CALL			/* to suppress HTML output if debug is true */

	if (!popt->nodoc)
		html(docstart(fout));

	while (!state.hit_eof) {
		switch (c = getc(fin)) {							/* whitespace handling */
			case ' ':
				if (state.whitespace == ONE_SPACE) {
					state.whitespace = TWO_SPACES;
					warning(ferr, MULTIPLE_SPACES_USED);
				}
				else if (state.whitespace != TWO_SPACES)	/* no preceding space(s) */
					state.whitespace = ONE_SPACE;
				break;

			case '\t':
				if (state.whitespace != ONE_TAB) {			/* no preceding tab */
					state.whitespace = ONE_TAB;
					warning(ferr, TAB_USED);
				}
				break;

			default:
				state.whitespace = NO_WHITESPACE;
		}

		if (state.escaped) {
			state.syntax = false;
			state.escaped = false;							/* escape lasts only for one character */
		}
		else {
			state.syntax = true;							/* syntax characters */
			switch (c) {
				case '#':
					if (state.colno == 1) {
						state.heading_level = 1;
						state.heading = HEADING_LEVEL;
					}
					else if (state.heading == HEADING_LEVEL) {
						if (++state.heading_level > MAX_HEADING_LEVEL)
							return error(ferr, HEADING_LEVEL_TOO_HIGH, MAX_HEADING_LEVEL);
					}
					else
						state.syntax = false;
					break;

				case '\\':
					state.escaped = true;
					break;

				case '\n':
					if (state.heading == HEADING_TEXT) {
						html(fprintf(fout, "</h%d>", state.heading_level));
						state.heading_level = 0;
						state.heading = NO_HEADING;
					}
					state.syntax = false;					/* only syntax character that has to be separately handled
															   as regular text */
					break;

				case ' ':
					if (state.heading == HEADING_LEVEL) {
						html(fprintf(fout, "<h%d>", state.heading_level));
						state.heading = HEADING_TEXT;
					}
					else
						state.syntax = false;
					break;

				default:
					state.syntax = false;
			}
		}

		if (state.heading == HEADING_LEVEL && c != '#')		/* non-space character right after # mark(s) */
			return error(ferr, HEADING_SYNTAX_SPACE);

		if (c == EOF)
			state.hit_eof = true;
		else if (c == '\n') {
			state.colno = 0;								/* start a new line */
			++state.lineno;
		}
		else if (is_new_utf8char(c))
			++state.colno;

		if (popt->debug)
			dumpstate(fout, c);
		else if (!state.syntax && c != EOF)
			html(putc(c, fout));							/* echo the character */
	}

	if (!popt->nodoc)
		html(docend(fout));

	return 0;
}

void docstart(FILE *fout)							/* start of HTML document */
{
	fprintf(fout, "<html>\n");
	fprintf(fout, "<body>\n");
}

void docend(FILE *fout)								/* end of HTML document */
{
	fprintf(fout, "</body>\n");
	fprintf(fout, "</html>\n");
}

void dumpstate(FILE *fout, char c)
{
	static bool first_call = true;

	static const char *heading_str[] = { "NO_HEADING", "HEADING_LEVEL", "HEADING_TEXT" };
	static const char *whitespace_str[] = { "NO_WHITESPACE", "ONE_SPACE", "TWO_SPACES", "ONE_TAB" };

	if (first_call) {
		putc('[', fout);
		first_call = false;
	}
	putc('{', fout);

#define	dump_int(KEY, INT)			fprintf(fout, "\"" KEY "\":%d,", INT)
#define	dump_bool(KEY, BOOL)		fprintf(fout, "\"" KEY "\":%s,", BOOL ? "true" : "false")
#define	dump_str(KEY, STR)			fprintf(fout, "\"" KEY "\":\"%s\",", STR)
#define	dump_hex(KEY, HEX)			fprintf(fout, "\"" KEY "\":\"0x%hhX\",", HEX)

#define	dump_bool_last(KEY, BOOL)	fprintf(fout, "\"" KEY "\":%s", BOOL ? "true": "false")

	dump_hex("charcode", c);
	dump_int("lineno", state.lineno);
	dump_int("colno", state.colno);
	dump_str("heading", heading_str[state.heading]);
	dump_str("whitespace", whitespace_str[state.whitespace]);
	dump_int("heading_level", state.heading_level);
	dump_bool("syntax", state.syntax);
	dump_bool_last("hit_eof", state.hit_eof);

	putc('}', fout);
	
	if (c != EOF)
		putc(',', fout);
	else
		putc(']', fout);
}

int error(FILE *ferr, enum error_type et, ...)		/* variable number of arguments (...) contains data associated with 
													   the error */
{
	va_list ap;

	va_start(ap, et);
	fprintf(ferr, "error: line %d column %d: ", state.lineno, state.colno);
	switch (et) {
		case HEADING_LEVEL_TOO_HIGH:
			fprintf(ferr, "HTML does not have headings beyond %d levels", va_arg(ap, int));
			break;
		case HEADING_SYNTAX_SPACE:
			fprintf(ferr, "# marks must be followed by a single space to indicate a heading");
			break;
	}
	fprintf(ferr, "\n");
	va_end(ap);
	return 1;
}

void warning(FILE *ferr, enum warning_type wt)		/* warnings don't have any associated data, hence warning type is
													   the only argument (apart from ferr) */
{
	fprintf(ferr, "warning: line %d column %d: ", state.lineno, state.colno);
	switch (wt) {
		case MULTIPLE_SPACES_USED:
			fprintf(ferr, "multiple spaces are not rendered");
			break;
		case TAB_USED:
			fprintf(ferr, "tabs are not rendered");
			break;
	}
	fprintf(ferr, "\n");
}
