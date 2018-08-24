/* Copyright (c) 2018 Trollforge. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Trollforge's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef DEBUG
#define DEBUG 1
#endif /* DEBUG */

#define OUTLN_FNT 0
#define BLOCK_FNT 1
#define COLOR_FNT 2

#define NUM_CHARS 94

/* 4 bytes + '\0' */
#define MAX_UTFSTR	5

#define LEFT_JUSTIFY	0
#define RIGHT_JUSTIFY	1
#define CENTER_JUSTIFY	2

#define DEFAULT_WIDTH	80

#define COLOR_ANSI	0
#define COLOR_MIRC	1

#define ENC_UNICODE	0
#define ENC_ANSI	1

#ifndef FONT_DIR
#define FONT_DIR	"fonts"
#endif /* FONT_DIR */

#ifndef FONT_EXT
#define FONT_EXT	"tdf"
#endif /* FONT_EXT */

#ifndef DEFAULT_FONT
#define DEFAULT_FONT	"brndamgx" /* seems most complete */
#endif /* DEFAULT_FONT */

typedef struct opt_s {
	uint8_t justify;
	uint8_t width;
	uint8_t color;
	uint8_t encoding;
	bool random;
	bool info;
} opt_t;

typedef struct cell_s {
	uint8_t color;
	char utfchar[MAX_UTFSTR];
} cell_t;

typedef struct glyph_s {
	uint8_t width;
	uint8_t height;
	cell_t *cell;
} glyph_t;

typedef struct font_s {
	uint8_t namelen;
	uint8_t *name;
	uint8_t fonttype;
	uint8_t spacing;
	uint16_t blocksize;
	uint16_t *charlist;
	uint8_t *data;
	glyph_t *glyphs[NUM_CHARS];
	uint8_t height;
} font_t;

struct dirname_s {
	char *str;
	SLIST_ENTRY(dirname_s) stuff;
};

const char *charlist = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
		       "PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

opt_t opt;

void usage(void);
font_t *loadfont(char *fn);
void readchar(int i, glyph_t *glyph, font_t *font);

void ibmtoutf8(char *a, char *u);
void printcolor(uint8_t color);
int lookupchar(char c, const font_t *font);

void printcell(char *utfchar, uint8_t color);
void printrow(const glyph_t *glyph, int row);
void printstr(const char *str, font_t *font);

void
usage(void)
{
	fprintf(stderr, "usage: tdfiglet [options] input\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -f [font] Specify font file used.\n");
	fprintf(stderr, "    -j l|r|c  Justify left, right, or center.  Default is left.\n");
	fprintf(stderr, "    -w n      Set screen width.  Default is 80.\n");
	fprintf(stderr, "    -c a|m    Color format ANSI or mirc.  Default is ANSI.\n");
	fprintf(stderr, "    -e u|a    Encode as unicode or ASCII.  Default is unicode.\n");
	fprintf(stderr, "    -i        Print font details.\n");
	fprintf(stderr, "    -r        Use random font.\n");
	fprintf(stderr, "    -h        Print usage.\n");
	fprintf(stderr, "\n");
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	font_t *font = NULL;
	int o;

	opt.justify = LEFT_JUSTIFY;
	opt.width = 80;
	opt.info = false;
	opt.encoding = ENC_UNICODE;
	opt.random = false;
	char *fontfile = NULL;

	struct timeval tv;

	DIR *d;
	struct dirent *dir;
	SLIST_HEAD(, dirname_s) head = SLIST_HEAD_INITIALIZER(dirname_s);
	SLIST_INIT(&head);
	struct dirname_s *dp;

	int r = 0;
	int dll = 0;

	while((o = getopt(argc, argv, "f:w:j:c:e:ir")) != -1) {
		switch (o) {
			case 'f':
				fontfile = optarg;
				break;
			case 'w':
				opt.width = atoi(optarg);
				break;
			case 'j':
				switch (optarg[0]) {
					case 'l':
						opt.justify = LEFT_JUSTIFY;
						break;
					case 'r':
						opt.justify = RIGHT_JUSTIFY;
						break;
					case 'c':
						opt.justify = CENTER_JUSTIFY;
						break;
					default:
						usage();
				}
				break;
			case 'c':
				switch (optarg[0]) {
					case 'a':
						opt.color = COLOR_ANSI;
						break;
					case 'm':
						opt.color = COLOR_MIRC;
						break;
					default:
						usage();
				}
				break;
			case 'e':
				switch (optarg[0]) {
					case 'a':
						opt.encoding = ENC_ANSI;
						break;
					case 'u':
						opt.encoding = ENC_UNICODE;
						break;
					default:
						usage();
				}
				break;
			case 'i':
				opt.info = true;
				break;
			case 'r':
				opt.random = true;
				break;
			case 'h':
				/* fallthrough */
			default:
				usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
	}

	if (!fontfile) {
		if (!opt.random) {
			fontfile = DEFAULT_FONT;
		} else {
			d = opendir(FONT_DIR);
			if (!d) {
				fprintf(stderr, "Error: unable to read %s\n",
					FONT_DIR);
				exit(1);
			}

			while ((dir = readdir(d))) {
				if (strstr(dir->d_name, FONT_EXT)) {
					dp = malloc(sizeof(struct dirname_s));
					dp->str = calloc(1, 1024);
					strcpy(dp->str, dir->d_name);
					SLIST_INSERT_HEAD(&head, dp, stuff);
					dll++;
				}
			}
			closedir(d);

			gettimeofday(&tv, NULL);

			srand(tv.tv_usec);
			r = dll ? rand() % dll : 0;

			dp = SLIST_FIRST(&head);
			for (int i = 0; i < r; i++) {
				dp = SLIST_NEXT(dp, stuff);
			}

			if (dp && dp->str) {
				fontfile = dp->str;
			} else {
				fontfile = DEFAULT_FONT;
			}
		}
	}

	font = loadfont(fontfile);

	printf("\n");

	for (int i = 0; i < argc; i++) {
		printstr(argv[i], font);
		printf("\n");
	}

	return(0);
}

font_t
*loadfont(char *fn_arg)
{

	font_t *font;
	uint8_t *map = NULL;
	int fd;
	struct stat st;
	size_t len;
	uint8_t *p;
	char *fn = NULL;

	const char *magic = "\x13TheDraw FONTS file\x1a";

	if (!strchr(fn_arg, '/')) {
		if (strchr(fn_arg, '.')) {
			fn = malloc(strlen(FONT_DIR) + strlen(fn_arg) + 2);
			sprintf(fn, "%s/%s", FONT_DIR, fn_arg);
		} else {
			fn = malloc(strlen(FONT_DIR) + strlen(fn_arg) + \
				strlen(FONT_EXT) + 3);
			sprintf(fn, "%s/%s.%s", FONT_DIR, fn_arg, FONT_EXT);
		}
	} else {
		fn = malloc(strlen(fn_arg) + 1);
		sprintf(fn, "%s", fn_arg);
	}

	if (fn == NULL) {
		perror(NULL);
		exit(EX_OSERR);
	}

	fd = open(fn, O_RDONLY);

	if (fd < 0) {
		perror(NULL);
		exit(EX_NOINPUT);
	}

	if (opt.info) {
		printf("file: %s\n", fn);
	}

	font = malloc(sizeof(font_t));

	if (fstat(fd, &st)) {
		perror(NULL);
		exit(EX_OSERR);
	}

	len = st.st_size;

	map = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

	if (!map) {
		perror(NULL);
		exit(EX_OSERR);
	}

	close(fd);

	if (!font) {
		perror(NULL);
		exit(EX_OSERR);
	}

	font->namelen = map[24];
	font->name = &map[25];
	font->fonttype = map[41];
	font->spacing = map[42];
	font->blocksize = (uint16_t)map[43];
	font->charlist = (uint16_t *)&map[45];
	font->data = &map[233];
	font->height = 0;

	if (strncmp(magic, (const char *)map, strlen(magic)) || font->fonttype != COLOR_FNT) {
		fprintf(stderr, "Invalid font file: %s\n", fn);
		exit(EX_NOINPUT);
	}

	free(fn);

	if (opt.info) {
		printf("font: %s\nchar list: ", font->name);
	}

	for (int i = 0; i < NUM_CHARS; i++) {
		/* check for invalid glyph addresses */
		if (charlist[i] + &map[233] > map + st.st_size) {
			perror(NULL);
			exit(EX_NOINPUT);
		}

		if (lookupchar(charlist[i], font) > -1) {

			if (opt.info) {
				printf("%c", charlist[i]);
			}

			p = font->data + font->charlist[i] + 2;
			if (*p > font->height) {
				font->height = *p;
			}
		}
	}

	if (opt.info) {
		printf("\n");
	}

	for (int i = 0; i < NUM_CHARS; i++) {

		if (lookupchar(charlist[i], font) != -1) {

			font->glyphs[i] = calloc(1, sizeof(glyph_t));

			if (!font->glyphs[i]) {
				perror(NULL);
				exit(EX_OSERR);
			}

			readchar(i, font->glyphs[i], font);

		} else {
			font->glyphs[i] = NULL;
		}
	}

	return font;
}

void
readchar(int i, glyph_t *glyph, font_t *font)
{
	if (font->charlist[i] == 0xffff) {
		printf("char not found\n");
		return;
	}

	uint8_t *p = font->data + font->charlist[i];

	uint8_t ch;
	uint8_t color;

	glyph->width = *p;
	p++;
	glyph->height = *p;
	p++;

	int row = 0;
	int col = 0;
	int width = glyph->width;
	int height = glyph->height;

	if (height > font->height) {
		font->height = height;
	}

	glyph->cell = calloc(width * font->height, sizeof(cell_t));
	if (!glyph->cell) {
		perror(NULL);
		exit(EX_OSERR);
	}

	for (int i = 0; i < width * font->height; i++) {
		glyph->cell[i].utfchar[0] = ' ';
		glyph->cell[i].color = 0;
	}

	while (*p) {

		ch = *p;
		p++;


		if (ch == '\r') {
			ch = ' ';
			row++;
			col = 0;
		} else {
			color = *p;
			p++;
#ifdef DEBUG
			if (ch == 0x09)
				ch = 'T';
			if (ch < 0x20)
				ch = '?';
#else
			if (ch < 0x20)
				ch = ' ';
#endif /* DEBUG */
			if (opt.encoding == ENC_UNICODE) {
				ibmtoutf8((char *)&ch,
					  glyph->cell[row * width + col].utfchar);
			} else {
				glyph->cell[row * width + col].utfchar[0] = ch;
			}

			glyph->cell[row * width + col].color = color;

			col++;
		}
	}
}

int
lookupchar(char c, const font_t *font)
{
	for (int i = 0; i < NUM_CHARS; i++) {
		if (charlist[i] == c && font->charlist[i] != 0xffff)
			return i;
	}

	return -1;
}

void
ibmtoutf8(char *a, char *u)
{
	static iconv_t conv = (iconv_t)0;

	size_t inchsize = 1;
	size_t outchsize = MAX_UTFSTR;

	if (!conv) {
		conv = iconv_open("UTF-8", "CP437");
	}

	iconv(conv, &a, &inchsize, &u, &outchsize);

	return;
}

void
printcolor(uint8_t color)
{

	uint8_t fg = color & 0x0f;
	uint8_t bg = (color & 0xf0) >> 4;

	/* thedraw colors                                     BRT BRT BRT BRT BRT BRT BRT BRT */
	/* thedraw colors     BLK BLU GRN CYN RED MAG BRN GRY BLK BLU GRN CYN RED PNK YLW WHT */
	uint8_t fgacolors[] = {30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97};
	uint8_t bgacolors[] = {40, 44, 42, 46, 41, 45, 43, 47};
	uint8_t fgmcolors[] = { 1,  2,  3, 10,  5,  6,  7, 15, 14,  12, 9, 11,  4, 13,  8,  0};
	uint8_t bgmcolors[] = { 1,  2,  3, 10,  5,  6,  7, 15, 14,  12, 9, 11,  4, 13,  8,  0};

	if (opt.color == COLOR_ANSI) {
		printf("\x1b[");
		printf("%d;", fgacolors[fg]);
		printf("%dm", bgacolors[bg]);
	} else {
		printf("\x03");
		printf("%d,", fgmcolors[fg]);
		printf("%d", bgmcolors[bg]);
	}
}

void
printrow(const glyph_t *glyph, int row)
{
	char *utfchar;
	uint8_t color;
	int i;
	uint8_t lastcolor;

	for (i = 0; i < glyph->width; i++) {
		utfchar = glyph->cell[glyph->width * row + i].utfchar;
		color = glyph->cell[glyph->width * row + i].color;

		if (i == 0 || color != lastcolor) {
			printcolor(color);
			lastcolor = color;
		}

		printf("%s", utfchar);
	}

	if (opt.color == COLOR_ANSI) {
		printf("\x1b[0m");
	} else {
		printf("\x03");
	}
}

void
printstr(const char *str, font_t *font)
{
	int maxheight = 0;
	int linewidth = 0;
	int len = strlen(str);
	int padding = 0;
	int n = 0;

	for (int i = 0; i < len; i++) {
		glyph_t *g;

		n = lookupchar(str[i], font);

		if (n == -1) {
			continue;
		}

		g = font->glyphs[n];

		if (g->height > maxheight) {
			maxheight = g->height;
		}

		linewidth += g->width;
		if (linewidth + 1 < len) {
			linewidth += font->spacing;
		}
	}

	if (opt.justify == CENTER_JUSTIFY) {
		padding = (opt.width - linewidth) / 2;
	} else if (opt.justify == RIGHT_JUSTIFY) {
		padding = (opt.width - linewidth);
	}

	for (int i = 0; i < maxheight; i++) {
		for (int i = 0; i < padding; ++i) {
			printf(" ");
		}

		for (int c = 0; c < strlen(str); c++) {
			n = lookupchar(str[c], font);

			if (n == -1) {
				continue;
			}

			glyph_t *g = font->glyphs[n];
			printrow(g, i);

			if (opt.color == COLOR_ANSI) {
				printf("\x1b[0m");
			} else {
				printf("\x03");
			}

			for (int s = 0; s < font->spacing; s++) {
				printf(" ");
			}
		}

		if (opt.color == COLOR_ANSI) {
			printf("\x1b[0m\n");
		} else {
			printf("\x03\r\n");
		}
	}
}
