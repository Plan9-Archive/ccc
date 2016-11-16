#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <thread.h>
#include <llrb.h>
#include "ccc.h"

Biobuf bin, bout;
int infd;
vlong cur, last;
int fstack, line;
char filename[1024];

void
setupout(void)
{
	Binit(&bout, 1, OWRITE);
}

void
pushfile(char *file)
{
	int fd;

	fd = open(file, OREAD);
	pushstream(fd);
}

void
pushstream(int fd)
{
	cur = 0;
	Binit(&bin, fd, OREAD);
	infd = dup(fd, -1);
	if(fstack++ == 1)
		return;
	line = 1;
	if(fd2path(fd, filename, sizeof(filename)) != 0)
		error("Could not get filename.");
}

Rune
getr(void)
{
	Rune r;

	r = Bgetrune(&bin);
	cur++;
	if(isspacer(r)) {
		if(r == '\n' && fstack == 1)
			line++;
	}
	return r;
}

int
ungetr(void)
{
	cur--;
	return Bungetrune(&bin);
}

void
copyout(void)
{
	static char buf[1024];
	long n, r;

	for(n = cur-last; n > 0; n -= r) {
		r = n < sizeof(buf) ? n : sizeof(buf);
		r = pread(infd, buf, r, cur-n);
		if(r == -1)
			error("Read error.");
		if(write(1, buf, r) != r)
			error("Write error.");
	}
	last = cur;
}

void
nocopyout(void)
{
	last = cur;
}

void
printchandecls(Symlist *slist)
{
	Sym **si, *s;

	if(slist == nil)
		return;

	print("\n#line %d %s\n", line, filename);
	print("Channel ");
	for(si = slist->sp; si < slist->ep; si++) {
		s = *si;
		print("*(%D)%s", s, si+1 < slist->ep ? ", " : ";\n");
	}
}

int
isspacer(Rune r)
{
	if(r < Runeself && isspace(r))
		return 1;
	return 0;
}

int
isalphar(Rune r)
{
	if(r < Runeself && (isalpha(r) || r == L'_'))
		return 1;
	if(r >= Runeself)
		return 1;
	return 0;
}

int
isdigitr(Rune r)
{
	if(r < Runeself && isdigit(r))
		return 1;
	return 0;
}
