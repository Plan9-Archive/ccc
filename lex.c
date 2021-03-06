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

int mainstacksize = 16*1024;

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
	[KVOID]	L"void",	LVOID,	TVOID,
	[KCHAR]	L"char",	LCHAR,	TCHAR,
	[KSHORT]	L"short",	LSHORT,	TSHORT,
	[KINT]	L"int",	LINT,	TINT,
	[KLONG]	L"long",	LLONG,	TLONG,
	[KFLOAT]	L"float",	LFLOAT,	TFLOAT,
	[KDOUBLE]	L"double",	LDOUBLE,	TDOUBLE,
	[KSIGNED]	L"signed",	LSIGNED,	TSIGNED,
	[KUNSIGNED]	L"unsigned",	LUNSIGNED,	TUNSIGNED,
	[KCONST]	L"const",	LCONST,	TCONST,
	[KVOLATILE]	L"volatile",	LVOLATILE,	TVOLATILE,
	[KAUTO]	L"auto",	LAUTO,	TAUTO,
	[KREGISTER]	L"register",	LREGISTER,	TREGISTER,
	[KSTATIC]	L"static",	LSTATIC,	TSTATIC,
	[KEXTERN]	L"extern",	LEXTERN,	TEXTERN,
	[KTYPEDEF]	L"typedef",	LTYPEDEF,	TTYPEDEF,
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

static void typefmtprint(Fmt*, Type*, Rune*, int);

static int
Yfmt(Fmt *f)
{
	Sym *s;

	s = va_arg(f->args, Sym*);
	if(s == nil)
		return 0;

	typefmtprint(f, s->type, s->name, 0);
	return 0;
}

static int
Tfmt(Fmt *f)
{
	Type *t;

	t = va_arg(f->args, Type*);
	if(t == nil)
		return 0;
	
	typefmtprint(f, t, nil, 0);
	return 0;
}

static int
Dfmt(Fmt *f)
{
	Type *t;

	t = va_arg(f->args, Type*);
	if(t == nil)
		return 0;
	
	typefmtprint(f, t, nil, 1);
	return 0;
}
	

static void suefmtprint(Fmt*, Sym*);
static void tyclfmtprint(Fmt*, u32int);

static void
typefmtprint(Fmt *f, Type *t, Rune *n, int dbg)
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
	int prev;
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
		return;

	if((tycl = t->tycl) != 0)
		tyclfmtprint(f, tycl);
	if((sue = t->sue) != nil)
		suefmtprint(f, sue);
	if(t->chantype != nil) {
		if(dbg)
			efmtprint(f, "chan(%D)", t->chantype);
		else
			efmtprint(f, "Channel*");
	}

	buf = ecalloc(BLEN, sizeof(*buf));
	ep = buf+BLEN;

	if((d = t->dtype) != nil)
		efmtprint(f, " ");
	p = e = buf;
	prev = !TPTR;
	for(; d != nil; d = d->link) {
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
	efmtprint(f, "%S", buf);
	free(buf);
}

static char *tnames[] = {
	[TVOID] "void",
	[TCHAR] "char",
	[TSHORT] "short",
	[TINT] "int",
	[TLONG] "long",
	[TVLONG] "vlong",
	[TFLOAT] "float",
	[TDOUBLE] "double",
};

static void
tyclfmtprint(Fmt *f, u32int btype)
{
	int i;

	if(btype & 1<<TUNSIGNED)
		efmtprint(f, "unsigned ");
	else if(btype & 1<<TSIGNED)
		efmtprint(f, "signed ");

	for(i = 0; i < NTYPE; i++) {
		if(btype & 1<<i)
			break;
	}
	if(i == NTYPE)
		return;
	efmtprint(f, tnames[i]);
}

static void
suefmtprint(Fmt *f, Sym *s)
{
	Sym **si;
	int r;

	if(s->name != nil) {
		efmtprint(f, "struct %S", s->name);
		return;
	}

	efmtprint(f, "struct {\n");

	for(si = s->sulist->sp; si < s->sulist->ep; si++)
		efmtprint(f, "\t%Y;\n", *si);

	efmtprint(f, "}");
}
