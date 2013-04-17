/* Generated by re2c 0.13.5 on Sun Mar 31 10:48:17 2013 */
#line 1 "ext/date/lib/parse_iso_intervals.re"
/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2013 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Derick Rethans <derick@derickrethans.nl>                    |
   +----------------------------------------------------------------------+
 */

/* $Id$ */

#include "timelib.h"

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if defined(_MSC_VER)
# define strtoll(s, f, b) _atoi64(s)
#elif !defined(HAVE_STRTOLL)
# if defined(HAVE_ATOLL)
#  define strtoll(s, f, b) atoll(s)
# else
#  define strtoll(s, f, b) strtol(s, f, b)
# endif
#endif

#define TIMELIB_UNSET   -99999

#define TIMELIB_SECOND  1
#define TIMELIB_MINUTE  2
#define TIMELIB_HOUR    3
#define TIMELIB_DAY     4
#define TIMELIB_MONTH   5
#define TIMELIB_YEAR    6

#define EOI      257

#define TIMELIB_PERIOD  260
#define TIMELIB_ISO_DATE 261
#define TIMELIB_ERROR   999

typedef unsigned char uchar;

#define   BSIZE	   8192

#define   YYCTYPE      uchar
#define   YYCURSOR     cursor
#define   YYLIMIT      s->lim
#define   YYMARKER     s->ptr
#define   YYFILL(n)    return EOI;

#define   RET(i)       {s->cur = cursor; return i;}

#define timelib_string_free free

#define TIMELIB_INIT  s->cur = cursor; str = timelib_string(s); ptr = str
#define TIMELIB_DEINIT timelib_string_free(str)

#ifdef DEBUG_PARSER
#define DEBUG_OUTPUT(s) printf("%s\n", s);
#define YYDEBUG(s,c) { if (s != -1) { printf("state: %d ", s); printf("[%c]\n", c); } }
#else
#define DEBUG_OUTPUT(s)
#define YYDEBUG(s,c)
#endif

#include "timelib_structs.h"

typedef struct Scanner {
	int           fd;
	uchar        *lim, *str, *ptr, *cur, *tok, *pos;
	unsigned int  line, len;
	struct timelib_error_container *errors;

	struct timelib_time     *begin;
	struct timelib_time     *end;
	struct timelib_rel_time *period;
	int                      recurrences;

	int have_period;
	int have_recurrences;
	int have_date;
	int have_begin_date;
	int have_end_date;
} Scanner;

static void add_warning(Scanner *s, char *error)
{
	s->errors->warning_count++;
	s->errors->warning_messages = realloc(s->errors->warning_messages, s->errors->warning_count * sizeof(timelib_error_message));
	s->errors->warning_messages[s->errors->warning_count - 1].position = s->tok ? s->tok - s->str : 0;
	s->errors->warning_messages[s->errors->warning_count - 1].character = s->tok ? *s->tok : 0;
	s->errors->warning_messages[s->errors->warning_count - 1].message = strdup(error);
}

static void add_error(Scanner *s, char *error)
{
	s->errors->error_count++;
	s->errors->error_messages = realloc(s->errors->error_messages, s->errors->error_count * sizeof(timelib_error_message));
	s->errors->error_messages[s->errors->error_count - 1].position = s->tok ? s->tok - s->str : 0;
	s->errors->error_messages[s->errors->error_count - 1].character = s->tok ? *s->tok : 0;
	s->errors->error_messages[s->errors->error_count - 1].message = strdup(error);
}

static char *timelib_string(Scanner *s)
{
	char *tmp = calloc(1, s->cur - s->tok + 1);
	memcpy(tmp, s->tok, s->cur - s->tok);

	return tmp;
}

static timelib_sll timelib_get_nr(char **ptr, int max_length)
{
	char *begin, *end, *str;
	timelib_sll tmp_nr = TIMELIB_UNSET;
	int len = 0;

	while ((**ptr < '0') || (**ptr > '9')) {
		if (**ptr == '\0') {
			return TIMELIB_UNSET;
		}
		++*ptr;
	}
	begin = *ptr;
	while ((**ptr >= '0') && (**ptr <= '9') && len < max_length) {
		++*ptr;
		++len;
	}
	end = *ptr;
	str = calloc(1, end - begin + 1);
	memcpy(str, begin, end - begin);
	tmp_nr = strtoll(str, NULL, 10);
	free(str);
	return tmp_nr;
}

static timelib_ull timelib_get_unsigned_nr(char **ptr, int max_length)
{
	timelib_ull dir = 1;

	while (((**ptr < '0') || (**ptr > '9')) && (**ptr != '+') && (**ptr != '-')) {
		if (**ptr == '\0') {
			return TIMELIB_UNSET;
		}
		++*ptr;
	}

	while (**ptr == '+' || **ptr == '-')
	{
		if (**ptr == '-') {
			dir *= -1;
		}
		++*ptr;
	}
	return dir * timelib_get_nr(ptr, max_length);
}

static void timelib_eat_spaces(char **ptr)
{
	while (**ptr == ' ' || **ptr == '\t') {
		++*ptr;
	}
}

static void timelib_eat_until_separator(char **ptr)
{
	while (strchr(" \t.,:;/-0123456789", **ptr) == NULL) {
		++*ptr;
	}
}

static long timelib_get_zone(char **ptr, int *dst, timelib_time *t, int *tz_not_found, const timelib_tzdb *tzdb)
{
	long retval = 0;

	*tz_not_found = 0;

	while (**ptr == ' ' || **ptr == '\t' || **ptr == '(') {
		++*ptr;
	}
	if ((*ptr)[0] == 'G' && (*ptr)[1] == 'M' && (*ptr)[2] == 'T' && ((*ptr)[3] == '+' || (*ptr)[3] == '-')) {
		*ptr += 3;
	}
	if (**ptr == '+') {
		++*ptr;
		t->is_localtime = 1;
		t->zone_type = TIMELIB_ZONETYPE_OFFSET;
		*tz_not_found = 0;
		t->dst = 0;

		retval = -1 * timelib_parse_tz_cor(ptr);
	} else if (**ptr == '-') {
		++*ptr;
		t->is_localtime = 1;
		t->zone_type = TIMELIB_ZONETYPE_OFFSET;
		*tz_not_found = 0;
		t->dst = 0;

		retval = timelib_parse_tz_cor(ptr);
	}
	while (**ptr == ')') {
		++*ptr;
	}
	return retval;
}

#define timelib_split_free(arg) {       \
	int i;                         \
	for (i = 0; i < arg.c; i++) {  \
		free(arg.v[i]);            \
	}                              \
	if (arg.v) {                   \
		free(arg.v);               \
	}                              \
}

/* date parser's scan function too large for VC6 - VC7.x
   drop the optimization solves the problem */
#ifdef PHP_WIN32
#pragma optimize( "", off )
#endif
static int scan(Scanner *s)
{
	uchar *cursor = s->cur;
	char *str, *ptr = NULL;
		
std:
	s->tok = cursor;
	s->len = 0;
#line 276 "ext/date/lib/parse_iso_intervals.re"



#line 256 "ext/date/lib/parse_iso_intervals.c"
{
	YYCTYPE yych;
	unsigned int yyaccept = 0;
	static const unsigned char yybm[] = {
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		128, 128, 128, 128, 128, 128, 128, 128, 
		128, 128,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
	};

	YYDEBUG(0, *YYCURSOR);
	if ((YYLIMIT - YYCURSOR) < 20) YYFILL(20);
	yych = *YYCURSOR;
	if (yych <= ',') {
		if (yych <= '\n') {
			if (yych <= 0x00) goto yy9;
			if (yych <= 0x08) goto yy11;
			if (yych <= '\t') goto yy7;
			goto yy9;
		} else {
			if (yych == ' ') goto yy7;
			if (yych <= '+') goto yy11;
			goto yy7;
		}
	} else {
		if (yych <= 'O') {
			if (yych <= '-') goto yy11;
			if (yych <= '/') goto yy7;
			if (yych <= '9') goto yy4;
			goto yy11;
		} else {
			if (yych <= 'P') goto yy5;
			if (yych != 'R') goto yy11;
		}
	}
	YYDEBUG(2, *YYCURSOR);
	++YYCURSOR;
	if ((yych = *YYCURSOR) <= '/') goto yy3;
	if (yych <= '9') goto yy98;
yy3:
	YYDEBUG(3, *YYCURSOR);
#line 389 "ext/date/lib/parse_iso_intervals.re"
	{
		add_error(s, "Unexpected character");
		goto std;
	}
#line 331 "ext/date/lib/parse_iso_intervals.c"
yy4:
	YYDEBUG(4, *YYCURSOR);
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy3;
	if (yych <= '9') goto yy59;
	goto yy3;
yy5:
	YYDEBUG(5, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych <= '9') goto yy12;
	if (yych == 'T') goto yy14;
yy6:
	YYDEBUG(6, *YYCURSOR);
#line 316 "ext/date/lib/parse_iso_intervals.re"
	{
		timelib_sll nr;
		int         in_time = 0;
		DEBUG_OUTPUT("period");
		TIMELIB_INIT;
		ptr++;
		do {
			if ( *ptr == 'T' ) {
				in_time = 1;
				ptr++;
			}
			if ( *ptr == '\0' ) {
				add_error(s, "Missing expected time part");
				break;
			}

			nr = timelib_get_unsigned_nr((char **) &ptr, 12);
			switch (*ptr) {
				case 'Y': s->period->y = nr; break;
				case 'W': s->period->d = nr * 7; break;
				case 'D': s->period->d = nr; break;
				case 'H': s->period->h = nr; break;
				case 'S': s->period->s = nr; break;
				case 'M': 
					if (in_time) {
						s->period->i = nr;
					} else {
						s->period->m = nr; 
					}
					break;
				default:
					add_error(s, "Undefined period specifier");
					break;
			}
			ptr++;
		} while (*ptr);
		s->have_period = 1;
		TIMELIB_DEINIT;
		return TIMELIB_PERIOD;
	}
#line 389 "ext/date/lib/parse_iso_intervals.c"
yy7:
	YYDEBUG(7, *YYCURSOR);
	++YYCURSOR;
	YYDEBUG(8, *YYCURSOR);
#line 378 "ext/date/lib/parse_iso_intervals.re"
	{
		goto std;
	}
#line 398 "ext/date/lib/parse_iso_intervals.c"
yy9:
	YYDEBUG(9, *YYCURSOR);
	++YYCURSOR;
	YYDEBUG(10, *YYCURSOR);
#line 383 "ext/date/lib/parse_iso_intervals.re"
	{
		s->pos = cursor; s->line++;
		goto std;
	}
#line 408 "ext/date/lib/parse_iso_intervals.c"
yy11:
	YYDEBUG(11, *YYCURSOR);
	yych = *++YYCURSOR;
	goto yy3;
yy12:
	YYDEBUG(12, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= 'L') {
		if (yych <= '9') {
			if (yych >= '0') goto yy25;
		} else {
			if (yych == 'D') goto yy24;
		}
	} else {
		if (yych <= 'W') {
			if (yych <= 'M') goto yy27;
			if (yych >= 'W') goto yy26;
		} else {
			if (yych == 'Y') goto yy28;
		}
	}
yy13:
	YYDEBUG(13, *YYCURSOR);
	YYCURSOR = YYMARKER;
	if (yyaccept <= 0) {
		goto yy3;
	} else {
		goto yy6;
	}
yy14:
	YYDEBUG(14, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yybm[0+yych] & 128) {
		goto yy15;
	}
	goto yy6;
yy15:
	YYDEBUG(15, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	YYDEBUG(16, *YYCURSOR);
	if (yybm[0+yych] & 128) {
		goto yy15;
	}
	if (yych <= 'L') {
		if (yych == 'H') goto yy19;
		goto yy13;
	} else {
		if (yych <= 'M') goto yy18;
		if (yych != 'S') goto yy13;
	}
yy17:
	YYDEBUG(17, *YYCURSOR);
	yych = *++YYCURSOR;
	goto yy6;
yy18:
	YYDEBUG(18, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych <= '9') goto yy22;
	goto yy6;
yy19:
	YYDEBUG(19, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych >= ':') goto yy6;
yy20:
	YYDEBUG(20, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	YYDEBUG(21, *YYCURSOR);
	if (yych <= 'L') {
		if (yych <= '/') goto yy13;
		if (yych <= '9') goto yy20;
		goto yy13;
	} else {
		if (yych <= 'M') goto yy18;
		if (yych == 'S') goto yy17;
		goto yy13;
	}
yy22:
	YYDEBUG(22, *YYCURSOR);
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	YYDEBUG(23, *YYCURSOR);
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy22;
	if (yych == 'S') goto yy17;
	goto yy13;
yy24:
	YYDEBUG(24, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych == 'T') goto yy14;
	goto yy6;
yy25:
	YYDEBUG(25, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= 'L') {
		if (yych <= '9') {
			if (yych <= '/') goto yy13;
			goto yy35;
		} else {
			if (yych == 'D') goto yy24;
			goto yy13;
		}
	} else {
		if (yych <= 'W') {
			if (yych <= 'M') goto yy27;
			if (yych <= 'V') goto yy13;
		} else {
			if (yych == 'Y') goto yy28;
			goto yy13;
		}
	}
yy26:
	YYDEBUG(26, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych <= '9') goto yy33;
	if (yych == 'T') goto yy14;
	goto yy6;
yy27:
	YYDEBUG(27, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych <= '9') goto yy31;
	if (yych == 'T') goto yy14;
	goto yy6;
yy28:
	YYDEBUG(28, *YYCURSOR);
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= '/') goto yy6;
	if (yych <= '9') goto yy29;
	if (yych == 'T') goto yy14;
	goto yy6;
yy29:
	YYDEBUG(29, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	YYDEBUG(30, *YYCURSOR);
	if (yych <= 'D') {
		if (yych <= '/') goto yy13;
		if (yych <= '9') goto yy29;
		if (yych <= 'C') goto yy13;
		goto yy24;
	} else {
		if (yych <= 'M') {
			if (yych <= 'L') goto yy13;
			goto yy27;
		} else {
			if (yych == 'W') goto yy26;
			goto yy13;
		}
	}
yy31:
	YYDEBUG(31, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	YYDEBUG(32, *YYCURSOR);
	if (yych <= 'C') {
		if (yych <= '/') goto yy13;
		if (yych <= '9') goto yy31;
		goto yy13;
	} else {
		if (yych <= 'D') goto yy24;
		if (yych == 'W') goto yy26;
		goto yy13;
	}
yy33:
	YYDEBUG(33, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	YYDEBUG(34, *YYCURSOR);
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy33;
	if (yych == 'D') goto yy24;
	goto yy13;
yy35:
	YYDEBUG(35, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= 'L') {
		if (yych <= '9') {
			if (yych <= '/') goto yy13;
		} else {
			if (yych == 'D') goto yy24;
			goto yy13;
		}
	} else {
		if (yych <= 'W') {
			if (yych <= 'M') goto yy27;
			if (yych <= 'V') goto yy13;
			goto yy26;
		} else {
			if (yych == 'Y') goto yy28;
			goto yy13;
		}
	}
	YYDEBUG(36, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != '-') goto yy39;
	YYDEBUG(37, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '0') goto yy40;
	if (yych <= '1') goto yy41;
	goto yy13;
yy38:
	YYDEBUG(38, *YYCURSOR);
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
yy39:
	YYDEBUG(39, *YYCURSOR);
	if (yych <= 'L') {
		if (yych <= '9') {
			if (yych <= '/') goto yy13;
			goto yy38;
		} else {
			if (yych == 'D') goto yy24;
			goto yy13;
		}
	} else {
		if (yych <= 'W') {
			if (yych <= 'M') goto yy27;
			if (yych <= 'V') goto yy13;
			goto yy26;
		} else {
			if (yych == 'Y') goto yy28;
			goto yy13;
		}
	}
yy40:
	YYDEBUG(40, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy42;
	goto yy13;
yy41:
	YYDEBUG(41, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '3') goto yy13;
yy42:
	YYDEBUG(42, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != '-') goto yy13;
	YYDEBUG(43, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '0') goto yy44;
	if (yych <= '2') goto yy45;
	if (yych <= '3') goto yy46;
	goto yy13;
yy44:
	YYDEBUG(44, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy47;
	goto yy13;
yy45:
	YYDEBUG(45, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy47;
	goto yy13;
yy46:
	YYDEBUG(46, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '2') goto yy13;
yy47:
	YYDEBUG(47, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != 'T') goto yy13;
	YYDEBUG(48, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '1') goto yy49;
	if (yych <= '2') goto yy50;
	goto yy13;
yy49:
	YYDEBUG(49, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy51;
	goto yy13;
yy50:
	YYDEBUG(50, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '5') goto yy13;
yy51:
	YYDEBUG(51, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != ':') goto yy13;
	YYDEBUG(52, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(53, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(54, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != ':') goto yy13;
	YYDEBUG(55, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(56, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(57, *YYCURSOR);
	++YYCURSOR;
	YYDEBUG(58, *YYCURSOR);
#line 358 "ext/date/lib/parse_iso_intervals.re"
	{
		DEBUG_OUTPUT("combinedrep");
		TIMELIB_INIT;
		s->period->y = timelib_get_unsigned_nr((char **) &ptr, 4);
		ptr++;
		s->period->m = timelib_get_unsigned_nr((char **) &ptr, 2);
		ptr++;
		s->period->d = timelib_get_unsigned_nr((char **) &ptr, 2);
		ptr++;
		s->period->h = timelib_get_unsigned_nr((char **) &ptr, 2);
		ptr++;
		s->period->i = timelib_get_unsigned_nr((char **) &ptr, 2);
		ptr++;
		s->period->s = timelib_get_unsigned_nr((char **) &ptr, 2);
		s->have_period = 1;
		TIMELIB_DEINIT;
		return TIMELIB_PERIOD;
	}
#line 757 "ext/date/lib/parse_iso_intervals.c"
yy59:
	YYDEBUG(59, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(60, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(61, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') {
		if (yych == '-') goto yy64;
		goto yy13;
	} else {
		if (yych <= '0') goto yy62;
		if (yych <= '1') goto yy63;
		goto yy13;
	}
yy62:
	YYDEBUG(62, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '0') goto yy13;
	if (yych <= '9') goto yy85;
	goto yy13;
yy63:
	YYDEBUG(63, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '2') goto yy85;
	goto yy13;
yy64:
	YYDEBUG(64, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '0') goto yy65;
	if (yych <= '1') goto yy66;
	goto yy13;
yy65:
	YYDEBUG(65, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '0') goto yy13;
	if (yych <= '9') goto yy67;
	goto yy13;
yy66:
	YYDEBUG(66, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '3') goto yy13;
yy67:
	YYDEBUG(67, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != '-') goto yy13;
	YYDEBUG(68, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '0') goto yy69;
	if (yych <= '2') goto yy70;
	if (yych <= '3') goto yy71;
	goto yy13;
yy69:
	YYDEBUG(69, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '0') goto yy13;
	if (yych <= '9') goto yy72;
	goto yy13;
yy70:
	YYDEBUG(70, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy72;
	goto yy13;
yy71:
	YYDEBUG(71, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '2') goto yy13;
yy72:
	YYDEBUG(72, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != 'T') goto yy13;
	YYDEBUG(73, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '1') goto yy74;
	if (yych <= '2') goto yy75;
	goto yy13;
yy74:
	YYDEBUG(74, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy76;
	goto yy13;
yy75:
	YYDEBUG(75, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '5') goto yy13;
yy76:
	YYDEBUG(76, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != ':') goto yy13;
	YYDEBUG(77, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(78, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(79, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != ':') goto yy13;
	YYDEBUG(80, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(81, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(82, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != 'Z') goto yy13;
yy83:
	YYDEBUG(83, *YYCURSOR);
	++YYCURSOR;
	YYDEBUG(84, *YYCURSOR);
#line 292 "ext/date/lib/parse_iso_intervals.re"
	{
		timelib_time *current;

		if (s->have_date || s->have_period) {
			current = s->end;
			s->have_end_date = 1;
		} else {
			current = s->begin;
			s->have_begin_date = 1;
		}
		DEBUG_OUTPUT("datetimebasic | datetimeextended");
		TIMELIB_INIT;
		current->y = timelib_get_nr((char **) &ptr, 4);
		current->m = timelib_get_nr((char **) &ptr, 2);
		current->d = timelib_get_nr((char **) &ptr, 2);
		current->h = timelib_get_nr((char **) &ptr, 2);
		current->i = timelib_get_nr((char **) &ptr, 2);
		current->s = timelib_get_nr((char **) &ptr, 2);
		s->have_date = 1;
		TIMELIB_DEINIT;
		return TIMELIB_ISO_DATE;
	}
#line 909 "ext/date/lib/parse_iso_intervals.c"
yy85:
	YYDEBUG(85, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '0') goto yy86;
	if (yych <= '2') goto yy87;
	if (yych <= '3') goto yy88;
	goto yy13;
yy86:
	YYDEBUG(86, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '0') goto yy13;
	if (yych <= '9') goto yy89;
	goto yy13;
yy87:
	YYDEBUG(87, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy89;
	goto yy13;
yy88:
	YYDEBUG(88, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '2') goto yy13;
yy89:
	YYDEBUG(89, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych != 'T') goto yy13;
	YYDEBUG(90, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '1') goto yy91;
	if (yych <= '2') goto yy92;
	goto yy13;
yy91:
	YYDEBUG(91, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych <= '9') goto yy93;
	goto yy13;
yy92:
	YYDEBUG(92, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '5') goto yy13;
yy93:
	YYDEBUG(93, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(94, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(95, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= '6') goto yy13;
	YYDEBUG(96, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy13;
	if (yych >= ':') goto yy13;
	YYDEBUG(97, *YYCURSOR);
	yych = *++YYCURSOR;
	if (yych == 'Z') goto yy83;
	goto yy13;
yy98:
	YYDEBUG(98, *YYCURSOR);
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	YYDEBUG(99, *YYCURSOR);
	if (yych <= '/') goto yy100;
	if (yych <= '9') goto yy98;
yy100:
	YYDEBUG(100, *YYCURSOR);
#line 281 "ext/date/lib/parse_iso_intervals.re"
	{
		DEBUG_OUTPUT("recurrences");
		TIMELIB_INIT;
		ptr++;
		s->recurrences = timelib_get_unsigned_nr((char **) &ptr, 9);
		TIMELIB_DEINIT;
		s->have_recurrences = 1;
		return TIMELIB_PERIOD;
	}
#line 997 "ext/date/lib/parse_iso_intervals.c"
}
#line 393 "ext/date/lib/parse_iso_intervals.re"

}
#ifdef PHP_WIN32
#pragma optimize( "", on )
#endif

#define YYMAXFILL 20

void timelib_strtointerval(char *s, int len, 
                           timelib_time **begin, timelib_time **end, 
						   timelib_rel_time **period, int *recurrences, 
						   struct timelib_error_container **errors)
{
	Scanner in;
	int t;
	char *e = s + len - 1;

	memset(&in, 0, sizeof(in));
	in.errors = malloc(sizeof(struct timelib_error_container));
	in.errors->warning_count = 0;
	in.errors->warning_messages = NULL;
	in.errors->error_count = 0;
	in.errors->error_messages = NULL;

	if (len > 0) {
		while (isspace(*s) && s < e) {
			s++;
		}
		while (isspace(*e) && e > s) {
			e--;
		}
	}
	if (e - s < 0) {
		add_error(&in, "Empty string");
		if (errors) {
			*errors = in.errors;
		} else {
			timelib_error_container_dtor(in.errors);
		}
		return;
	}
	e++;

	/* init cursor */
	in.str = malloc((e - s) + YYMAXFILL);
	memset(in.str, 0, (e - s) + YYMAXFILL);
	memcpy(in.str, s, (e - s));
	in.lim = in.str + (e - s) + YYMAXFILL;
	in.cur = in.str;

	/* init value containers */
	in.begin = timelib_time_ctor();
	in.begin->y = TIMELIB_UNSET;
	in.begin->d = TIMELIB_UNSET;
	in.begin->m = TIMELIB_UNSET;
	in.begin->h = TIMELIB_UNSET;
	in.begin->i = TIMELIB_UNSET;
	in.begin->s = TIMELIB_UNSET;
	in.begin->f = 0;
	in.begin->z = 0;
	in.begin->dst = 0;
	in.begin->is_localtime = 0;
	in.begin->zone_type = TIMELIB_ZONETYPE_OFFSET;

	in.end = timelib_time_ctor();
	in.end->y = TIMELIB_UNSET;
	in.end->d = TIMELIB_UNSET;
	in.end->m = TIMELIB_UNSET;
	in.end->h = TIMELIB_UNSET;
	in.end->i = TIMELIB_UNSET;
	in.end->s = TIMELIB_UNSET;
	in.end->f = 0;
	in.end->z = 0;
	in.end->dst = 0;
	in.end->is_localtime = 0;
	in.end->zone_type = TIMELIB_ZONETYPE_OFFSET;

	in.period = timelib_rel_time_ctor();
	in.period->y = 0;
	in.period->d = 0;
	in.period->m = 0;
	in.period->h = 0;
	in.period->i = 0;
	in.period->s = 0;
	in.period->weekday = 0;
	in.period->weekday_behavior = 0;
	in.period->first_last_day_of = 0;
	in.period->days = TIMELIB_UNSET;

	in.recurrences = 1;

	do {
		t = scan(&in);
#ifdef DEBUG_PARSER
		printf("%d\n", t);
#endif
	} while(t != EOI);

	free(in.str);
	if (errors) {
		*errors = in.errors;
	} else {
		timelib_error_container_dtor(in.errors);
	}
	if (in.have_begin_date) {
		*begin = in.begin;
	} else {
		timelib_time_dtor(in.begin);
	}
	if (in.have_end_date) {
		*end   = in.end;
	} else {
		timelib_time_dtor(in.end);
	}
	if (in.have_period) {
		*period = in.period;
	} else {
		timelib_rel_time_dtor(in.period);
	}
	if (in.have_recurrences) {
		*recurrences = in.recurrences;
	}
}


/*
 * vim: syntax=c
 */
