#include <ctype.h>
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
#include <sys/stat.h>
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

typedef struct opt_s {
	uint8_t justify;
	uint8_t width;
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

typedef struct __attribute__((packed)) font_s {
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

char *magic = "\x13TheDraw FONTS file\x1a";

char *charlist = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
		 "PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

uint8_t fgcolors[] = {30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97};
uint8_t bgcolors[] = {40, 44, 42, 46, 41, 45, 43, 47};

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
	fprintf(stderr, "usage: tdfiglet [options] [font.tdf] input\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -j l|r|c  Justify left, right, or center.  Default is left\n");
	fprintf(stderr, "    -w n      set screen width, default is 80.\n");
	exit(EX_USAGE);

	printf("welp\n");

	return;
}

opt_t opt;

int
main(int argc, char *argv[])
{
	font_t *font = NULL;
	int o;

	opt.justify = LEFT_JUSTIFY;
	opt.width = 80;

	while((o = getopt(argc, argv, "w:j:")) != -1) {
		switch (o) {
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
						exit(EX_USAGE);
				}
				break;
			default:
				usage();
				exit(EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	font = loadfont(argv[0]);

	/* TODO: add support for the others */
	if (font->fonttype != COLOR_FNT) {
		return 0;
	}

	printf("\n");

	for (int i = 1; i < argc; i++) {
		printstr(argv[i], font);
		printf("\n");
	}

	return(0);
}

font_t
*loadfont(char *fn) {

	font_t *font;
	uint8_t *map = NULL;
	int fd;
	struct stat st;
	size_t len;
	uint8_t *p;

	fd = open(fn, O_RDONLY);

#ifdef DEBUG
	printf("file: %s\n", fn);
#endif /* DEBUG */

	font = malloc(sizeof(font_t));

	if (fd < 0) {
		perror(NULL);
		exit(EX_NOINPUT);
	}

	stat(fn, &st);

	len = st.st_size;

	map = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);

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

	if (strncmp(magic, (const char *)map, strlen(magic))) {
		perror("Invalid font file");
		exit(EX_NOINPUT);
	}

#ifdef DEBUG
	printf("font: %s\nchar list: ", font->name);
#endif /* DEBUG */

	for (int i = 0; i < NUM_CHARS; i++)
		if (lookupchar(charlist[i], font) > -1) {
#ifdef DEBUG
			printf("%c", charlist[i]);
#endif /* DEBUG */
			p = font->data + font->charlist[i] + 2;
			if (*p > font->height) {
				font->height = *p;
			}
		}

#ifdef DEBUG
	printf("\n");
#endif /* DEBUG */

	for (int i = 0; i < NUM_CHARS; i++) {

		if (lookupchar(charlist[i], font) > -1) {

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
		glyph->cell[i].utfchar[0] = 'X';
		glyph->cell[i].color = 0;
	}

	/* unsure why tbqh */
	p += 2;

	while (*p) {

		ch = *p;
		p++;

		color = *p;
		p++;

		if (ch == '\r') {
			row++;
			col = 0;
			p++;
		} else {
#ifdef DEBUG
			if (ch == 0x09)
				ch = 'T';
			if (ch < 0x20)
				ch = '?';
#else
			if (ch < 0x20)
				ch = ' ';
#endif /* DEBUG */
			ibmtoutf8((char *)&ch,
				  glyph->cell[row * width + col].utfchar);
			glyph->cell[row * width + col].color = color;

			col++;
		}
	}
}

int
lookupchar(char c, const font_t *font)
{
	for (int i = 0; i < NUM_CHARS; i++) {
		if (c == charlist[i] && font->charlist[i] != 0xffff)
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

	printf("\x1b[");
	printf("%d;", fgcolors[fg]);
	printf("%dm", bgcolors[bg]);
}

void
printrow(const glyph_t *glyph, int row)
{
	char *utfchar;
	uint8_t color;
	int i;
	for (i = 0; i < glyph->width; i++) {
		utfchar = glyph->cell[glyph->width * row + i].utfchar;			
		color = glyph->cell[glyph->width * row + i].color;
		printcolor(color);

		printf("%s", utfchar);
	}
	printf("\x1b[0m");
}

void
printstr(const char *str, font_t *font)
{
	int maxheight = 0;
	int linewidth = 0;
	int len = strlen(str);
	int padding = 0;

	for (int i = 0; i < len; i++) {
		glyph_t *g;

		 g = font->glyphs[lookupchar(str[i], font)];

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
			glyph_t *g = font->glyphs[lookupchar(str[c], font)];
			printrow(g, i);
			printf("\x1b[0m");
		}
		printf("\x1b[0m\n");
	}
}
