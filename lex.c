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
	if(argc == 0)
		pushstream(0);
	else
		pushfile(argv[0]);
	pushblock();
	keywords();
	setupout();
	yyparse();
	if(dbg)
		printsyms();
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

static int typefmtprint(Fmt*, Type*, Rune*);

static int
Yfmt(Fmt *f)
{
	Sym *s;

	s = va_arg(f->args, Sym*);
	if(s == nil)
		return 0;

	return typefmtprint(f, s->type, s->name);
}

static int
Tfmt(Fmt *f)
{
	Type *t;

	t = va_arg(f->args, Type*);
	if(t == nil)
		return 0;
	
	return typefmtprint(f, t, nil);
}

static int suefmtprint(Fmt*, Sym*);
static int tyclfmtprint(Fmt*, u32int);

static int
typefmtprint(Fmt *f, Type *t, Rune *n)
{
	enum {
		BLEN = 512
	};
	Rune *buf;
	Dtype *d;
	Rune *p, *e, *ep;
	u32int tycl;
	Sym *sue;
	Type **ti;
	int r, prev, chan;
	long len;

#define parens \
	if(prev == TPTR) { \
		if(e >= ep-2) \
			error("No room to print declaration of %S", n); \
		memmove(buf+1, buf, (e-buf) * sizeof(Rune)); \
		p++; \
		e++; \
		*buf = L'('; \
		*e++ = L')'; \
		*e = L'\0'; \
	}

	if(t == nil)
		return 0;

	chan = 0;
	if((tycl = t->tycl) != 0) {
		r = tyclfmtprint(f, tycl);
		if(r == -1)
			return -1;
	} else if((sue = t->sue) != nil) {
		r = suefmtprint(f, sue);
		if(r == -1)
			return -1;
	} else if(t->chantype != nil) {
		chan = 1;
		r = fmtprint(f, "%s", "Channel");
		if(r == -1)
			return -1;
	}

	r = fmtprint(f, " ");
	if(r == -1)
		return -1;

	if(chan) {
		r = fmtprint(f, "*(");
		if(r == -1)
			return -1;
	}

	buf = ecalloc(BLEN, sizeof(*buf));
	ep = buf+BLEN;

	p = e = buf;
	prev = !TPTR;
	for(d = t->dtype; d != nil; d = d->link) {
		switch(d->t) {
		case TARR:
			parens
			if(d->alen != -1)
				e = runeseprint(e, ep, "[%d]", d->alen);
			else
				e = runeseprint(e, ep, "[]");
			break;
		case TPTR:
			if(e >= ep-1)
				error("No room to print declaration of %S", n);
			memmove(buf+1, buf, (e-buf) * sizeof(Rune));
			p++;
			e++;
			*e = L'\0';
			*buf = L'*';
			break;
		case TFUNC:
			parens
			if(e >= ep-2)
				error("No room to print declaration of %S", n);
			*e++ = L'(';
			for(ti = d->param->sp; ti < d->param->ep; ti++) {
				e = runeseprint(e, ep, "%T", *ti);
				if(ti+1 < d->param->ep)
					e = runeseprint(e, ep, ", ");
			}
			*e++ = L')';
			break;
		}
		prev = d->t;
	}

	if(n == nil)
		goto End;

	len = runestrlen(n);
	if(e >= ep-len)
		error("No room to print declaration of %S", n);
	memmove(p+len, p, (e-p) * sizeof(Rune));
	e += len;
	memcpy(p, n, len*sizeof(Rune));
	*e = L'\0';

End:
	r = fmtprint(f, "%S", buf);
	if(r == -1)
		return r;
	if(chan)
		r = fmtprint(f, ")");
	free(buf);
	return r;
}

static char *tnames[NTYPE] = {	
	[BVOID] "void",
	[BCHAR] "char",
	[BSHORT] "short",
	[BINT] "int",
	[BLONG] "long",
	[BVLONG] "vlong",
	[BFLOAT] "float",
	[BDOUBLE] "double",
};

static int
tyclfmtprint(Fmt *f, u32int btype)
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
	if(i == NTYPE)
		return 0;
	return fmtprint(f, tnames[i]);
}

// TODO
static int
suefmtprint(Fmt*, Sym*)
{
	return 0;
}
