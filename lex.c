#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <thread.h>
#include <llrb.h>
#include "ccc.h"
#include "y.tab.h"

static void keywords(void);

static int getsym(Rune);
static int getnumber(Rune);

static int Yfmt(Fmt*);
static int Tfmt(Fmt*);
static int Dfmt(Fmt*);

int dbg;

void
threadmain(int argc, char **argv)
{
	ARGBEGIN {
	default:
		usage();
		break;
	} ARGEND

	if(argc > 1)
		usage();

	dbg = 1;
	fmtinstall('Y', Yfmt);
	fmtinstall('T', Tfmt);
	fmtinstall('D', Dfmt);
	if(argc == 0)
		pushstream(0);
	else
		pushfile(argv[0]);
	pushblock();
	keywords();
	setupout();
	yyparse();
}

void
usage(void)
{
	print("ccc [file]\n");
	exits("usage");	
}

typedef struct Keyword Keyword;
struct Keyword {
	Rune *name;
	int lex;
	int tycl;
};

Keyword keys[NKEYS] = {
	[KVOID]	L"void",	LVOID,	BVOID,
	[KCHAR]	L"char",	LCHAR,	BCHAR,
	[KSHORT]	L"short",	LSHORT,	BSHORT,
	[KINT]	L"int",	LINT,	BINT,
	[KLONG]	L"long",	LLONG,	BLONG,
	[KFLOAT]	L"float",	LFLOAT,	BFLOAT,
	[KDOUBLE]	L"double",	LDOUBLE,	BDOUBLE,
	[KSIGNED]	L"signed",	LSIGNED,	BSIGNED,
	[KUNSIGNED]	L"unsigned",	LUNSIGNED,	BUNSIGNED,
	[KCONST]	L"const",	LCONST,	BCONST,
	[KVOLATILE]	L"volatile",	LVOLATILE,	BVOLATILE,
	[KAUTO]	L"auto",	LAUTO,	BAUTO,
	[KREGISTER]	L"register",	LREGISTER,	BREGISTER,
	[KSTATIC]	L"static",	LSTATIC,	BSTATIC,
	[KEXTERN]	L"extern",	LEXTERN,	BEXTERN,
	[KTYPEDEF]	L"typedef",	LTYPEDEF,	BTYPEDEF,
	[KCHAN]	L"chan",	LCHAN,	0,
	[KSTRUCT]	L"struct",	LSTRUCT,	0,
	[KUNION]	L"union",	LUNION,	0,
	[KENUM]	L"enum",	LENUM,	0,
};

static void
keywords(void)
{
	Sym *s;
	Keyword *k;

	for(k = keys; k < keys+NKEYS; k++) {
		s = sym(namespace, k->name);
		s->lex = k->lex;
		s->tycl = k->tycl;
	}
}

int
yylex(void)
{
	Rune r;
	char c;

Again:
	r = getr();
	if(isspacer(r))
		goto Again;
	if(r == -1)
		return -1;

	if(isalphar(r) || r == L'_')
		return getsym(r);
	if(isdigitr(r))
		return getnumber(r);

	c = r;
//	switch(c){
//	case ';':
//	case ',':
//	case '*':
//		return c;
//	}
	return c;
}

static int
getsym(Rune r)
{
	static Rune buf[8192];
	Rune *p;

	p = buf;
Again:
	*p++ = r;
	r = getr();
	if(isalphar(r) || isdigitr(r))
		goto Again;

	*p = L'\0';
	ungetr();
	yylval.sym = sym(namespace, buf);
	return yylval.sym->lex;
}	

static int
getnumber(Rune r)
{
	static char buf[8192];
	char *p;

	p = buf;
Again:
	*p++ = r;
	r = getr();
	if(isdigitr(r))
		goto Again;

	*p = '\0';
	ungetr();
	yylval.ival = strtol(buf, nil, 10);
	return LIVAL;
}

void
yyerror(char *err)
{
	error(err);
}

static int typefmtprint(Fmt*, Type*);

static int
Yfmt(Fmt *f)
{
	Sym *s;

	s = va_arg(f->args, Sym*);
	if(s == nil)
		return -1;

	if(fmtprint(f, "Sym %S: ", s->name) == -1)
		return -1;
	if(typefmtprint(f, s->type) == -1)
		return -1;
	return 0;
}

static int
Tfmt(Fmt *f)
{
	return typefmtprint(f, va_arg(f->args, Type*));
}

static int declfmtprint(Fmt*, Sym*);

static int
Dfmt(Fmt *f)
{
	return declfmtprint(f, va_arg(f->args, Sym*));
}

static int btypefmtprint(Fmt*, u32int);

static int
typefmtprint(Fmt *f, Type *t)
{
	Dtype *d;
	Sym *suetag, **si, *s;
	Symlist *sl;

	if(t == nil)
		return 0;

	for(d = t->dtype; d != nil; d = d->link) switch(d->t) {
	case TPTR:
		if(fmtprint(f, "pointer to ") == -1)
			return -1;
		break;
	case TARR:
		if(fmtprint(f, "array of ") == -1)
			return -1;
		break;
	}

	if(btypefmtprint(f, t->tycl) == -1)
		return -1;
	if(t->sue != nil) {
		suetag = t->sue;
		fmtprint(f, "struct type %S ", suetag->name);
		if(suetag->sulist != nil) {
			fmtprint(f, "with struct members:\n");
			sl = suetag->sulist;
			for(si = sl->sp; si < sl->ep; si++) {
				s = *si;
				fmtprint(f, "\t%S: ", s->name);
				typefmtprint(f, s->type);
				fmtprint(f, "\n");
			}
		}
	}			
	if(t->chantype == nil) 
		return 0;
	if(fmtprint(f, "chan sending ") == -1)
		return -1;
	return typefmtprint(f, t->chantype);
}	

static int
btypefmtprint(Fmt *f, u32int btype)
{
	int i, r;

	r = 0;
	if(btype & 1<<BUNSIGNED)
		r = fmtprint(f, "unsigned ");
	else if(btype & 1<<BSIGNED)
		r = fmtprint(f, "signed ");
	if(r == -1)
		return -1;

	for(i = 0; i < NTYPE; i++) {
		if(btype & 1<<i)
			break;
	}
	switch(i){
	case BCHAR:
		r = fmtprint(f, "char");
		break;
	case BSHORT:
		r = fmtprint(f, "short");
		break;
	case BINT:
		r = fmtprint(f, "int");
		break;
	case BLONG:
		r = fmtprint(f, "long");
		break;
	case BVLONG:
		r = fmtprint(f, "vlong");
		break;
	}
	return r;
}

static int suefmtprint(Sym*);

static int
declfmtprint(Fmt *f, Sym *s)
{
	enum {
		BLEN = 1024
	};
	static Rune buf[BLEN];
	Type *t;
	Dtype *d;
	Rune *p, *e;
	u32int tycl;
	Sym *sue;
	int r, prev;
	long len;

	if((t = s->type) == nil)
		return 0;

	if((tycl = t->tycl) != 0) {
		r = btypefmtprint(f, tycl);
		if(r == -1)
			return -1;
	}
	if((sue = t->sue) != nil) {
		r = suefmtprint(sue);
		if(r == -1)
			return -1;
	}

	p = e = buf;
	prev = !TPTR;
	for(d = t->dtype; d != nil; d = d->link) {
		switch(d->t) {
		case TARR:
			if(prev == TPTR) {
				if(e >= buf+BLEN-2)
					error("No room to print declaration of %S", s->name);
				memmove(buf+1, buf, (e-buf) * sizeof(Rune));
				p++;
				e++;
				*buf = L'(';
				*e++ = L')';
			}
			if(d->alen != -1)
				e = runeseprint(e, buf+BLEN, "[%d]", d->alen);
			else
				e = runeseprint(e, buf+BLEN, "[]");
			break;
		case TPTR:
				if(e >= buf+BLEN-1)
					error("No room to print declaration of %S", s->name);
				memmove(buf+1, buf, (e-buf) * sizeof(Rune));
				p++;
				e++;
				*buf = L'*';
			break;
		case TFUNC:
			break;
		}
		prev = d->t;
	}
	len = runestrlen(s->name);
	if(e >= buf+BLEN-len)
		error("No room to print declaration of %S", s->name);
	memmove(p+len, p, (e-p) * sizeof(Rune));
	e += len;
	memcpy(p, s->name, len*sizeof(Rune));
	*e = L'\0';
	return fmtprint(f, "%S", buf);
}

// TODO
static int
suefmtprint(Sym*)
{
	return 0;
}
