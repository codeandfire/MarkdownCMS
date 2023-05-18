#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

const int HEADING_TEXT_BASE_SIZE = 50;

enum { REGULAR_TEXT, HEADING_NUMBER, HEADING_TEXT } state;

struct {
	int number;
	char *textptr;
	char *textcurptr;
	int textsize;						/* total amount of space allocated for text */
} heading;

int lineno = 1;
int colno = 1;

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

int main()
{
	int c;
	bool special;			/* the current character is a recognized Markdown syntax character
							   and should be handled appropriately, not echoed to the screen */

	for (; (c = getchar()) != EOF; ++colno, special = false) {
		if (c == '#') {
			if (colno == 1) {
				special = true;
				heading.number = 1;			/* new heading */
				heading.textptr = heading.textcurptr = (char *) calloc(HEADING_TEXT_BASE_SIZE, sizeof(char));
				*heading.textcurptr = '\0';
				heading.textsize = HEADING_TEXT_BASE_SIZE;
				state = HEADING_NUMBER;
			}
			else if (state == HEADING_NUMBER) {
				special = true;
				++heading.number;
			}
		}
		else if (c == '\n') {
			if (state == HEADING_TEXT) {
				trim(heading.textptr);		/* heading done */
				printf("<h%d>%s</h%d>", heading.number, heading.textptr, heading.number);
				free(heading.textptr);
				state = REGULAR_TEXT;
			}
			special = false;
			colno = 0;
			++lineno;
		}
		if (!special) {
			if (state == HEADING_NUMBER || state == HEADING_TEXT) {
				if (state == HEADING_NUMBER)
					state = HEADING_TEXT;
				if (heading.textcurptr + 1 > heading.textptr + heading.textsize - 1) {
					int offset = heading.textcurptr - heading.textptr;
					heading.textptr = (char *) reallocarray(		/* increase the size */
						heading.textptr, (heading.textsize += HEADING_TEXT_BASE_SIZE), sizeof(char));
					heading.textcurptr = heading.textptr + offset;
				}
				*heading.textcurptr++ = c;
				*heading.textcurptr = '\0';
			}
			else
				putchar(c);
		}
	}

	return 0;
}
