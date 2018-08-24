
# tdfiglet

Because your figlet ascii sucks.

![screenshot](https://git.trollforge.org/tdfiglet/plain/screenshot.png)

All known TDF fonts (1198) are included.
## Installation

```
make
sudo make install
```

## Usage

If you're just trying to spam irc `tdfiglet -cm yes hello` will suffice.

```
usage: tdfiglet [options] -f [font] input

    -f [font] Specify font file used.
    -j l|r|c  Justify left, right, or center.  Default is left.
    -w n      Set screen width.  Default is 80.
    -c a|m    Color format ANSI or mirc.  Default is ANSI.
    -e u|a    Encode as unicode or ASCII.  Default is unicode.
    -i        Print font details.
    -r        Use random font.
    -h        Print usage.
```
