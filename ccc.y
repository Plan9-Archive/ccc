%{
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <llrb.h>
#include "ccc.h"
%}

%union {
	Sym *sym;
	Symlist *symlist;
	Type *type;
	Typelist *typelist;
	Btype btype;
	struct Decl {
		Btype btype;
		Symlist *slist;
		Type *chantype;
	} decl;
	struct Sumembs {
		Names *names;
		Symlist *slist;
	} sumembs;
	int ival;
	double dval;
}

%type	<decl>	xasdecl
%type	<symlist>	declors
%type	<typelist>	param-list
%type	<sym>	declor declor2 name tycl sue
%type	<type>	abdeclor abdeclor2
%type	<btype>	tcqspec
%type	<sumembs>	sdecls

%token	<sym>	LNAME LVOID LCHAR LSHORT LINT LLONG LFLOAT
%token	<sym>	LDOUBLE LSIGNED LUNSIGNED LCONST LVOLATILE
%token	<sym>	LAUTO LREGISTER LSTATIC LEXTERN LTYPEDEF
%token	<sym>	LUNION LENUM LSTRUCT LTAG
%token	<ival>	LIVAL
%token	<dval>	LDVAL
%token	LCHAN

%%

unit:
|	unit xadecl

xadecl:
	xasdecl
	{
		if($1.chantype != nil) {
			setchantype($1.slist, $1.chantype);
			printchandecls($1.slist);
		} else {
			settype($1.slist, &$1.btype);
			copyout();
		}
		symlistfree($1.slist);
	}

xasdecl:
	tcqspec ';'
	{
		$$.btype = $1;
		$$.slist = nil;
		$$.chantype = nil;
	}
|	tcqspec declors ';'
	{
		typeclean(&$1);

		$$.btype = $1;
		$$.slist = $2;
		$$.chantype = nil;
	}
|	LCHAN '(' tcqspec abdeclor ')' declors ';'
	{
		typeclean(&$3);
		*(Btype*)$4 = $3;

		$$.btype = (struct Btype){nil, 0};
		$$.slist = $6;
		$$.chantype = $4;
	}

tcqspec:
	{
		$$ = (struct Btype){nil, 0};
	}
|	tcqspec tycl
	{
		$$.tycl = addspec($1.tycl, $2);
	}
|	tcqspec sue
	{
		if($$.sue != nil)
			error("Multiple struct declarations.");
		$$.sue = $2;
	}

declors:
	declor
	{
		$$ = symlist();
		addsym($$, $1);
	}
|	declors ',' declor
	{
		$$ = addsym($1, $3);
	}

declor:
	declor2
|	'*' declor
	{
		dtype($2->newtype, TPTR);
		$$ = $2;
	}

declor2:
	name
	{
		$1->newtype = type();
		$$ = $1;
	}
|	'(' declor ')'
	{
		$$ = $2;
	}
|	declor2 '[' LIVAL ']'
	{
		Dtype *d;

		d = dtype($1->newtype, TARR);
		d->alen = $3;
		$$ = $1;
	}
|	declor2 '[' ']'
	{
		Dtype *d;

		d = dtype($1->newtype, TARR);
		d->alen = -1;
		$$ = $1;
	}
|	declor2 '(' param-list ')'
	{
		Dtype *d;

		d = dtype($1->newtype, TFUNC);
		d->param = $3;
		$$ = $1;
	}
|	declor2 '(' param-list ',' '.' '.' '.' ')'
	{
		Dtype *d;
		Type *t;

		d = dtype($1->newtype, TFUNC);
		d->param = $3;
		t = type();
		t->tycl = 1<<BDOTS;
		addtype(d->param, t);
		$$ = $1;
	}

param-list:
	tcqspec declor
	{
		$$ = typelist();

		typeclean(&$1);
		*(Btype*)$2->newtype = $1;
		addtype($$, $2->newtype);
		$2->newtype = nil;
	}
|	param-list ',' tcqspec declor
	{
		typeclean(&$3);
		*(Btype*)$4->newtype = $3;
		$$ = addtype($1, $4->newtype);
		$4->newtype = nil;
}

abdeclor:
	abdeclor2
|	'*' abdeclor2
	{
		dtype($2, TPTR);
		$$ = $2;
	}

abdeclor2:
	{
		$$ = type();
	}

tycl:
	LVOID
|	LCHAR
|	LSHORT
|	LINT
|	LLONG
|	LFLOAT
|	LDOUBLE
|	LSIGNED
|	LUNSIGNED
|	LCONST
|	LVOLATILE
|	LAUTO
|	LREGISTER
|	LSTATIC
|	LEXTERN
|	LTYPEDEF

sue:
	LSTRUCT name
	{
		$2->suetype = TSTRUCT;
		$$ = $2;
	}
|	LSTRUCT name '{' sdecls '}'
	{
		$2->sunames = $4.names;
		$2->sulist = $4.slist;
		$2->suetype = TSTRUCT;
		$$ = $2;
	}
|	LUNION name
	{
		$2->suetype = TUNION;
		$$ = $2;
	}
|	LENUM name
	{
		$2->suetype = TENUM;
		$$ = $2;
	}

sdecls:
	{
		$$.names = pushnames(nil);
		$$.slist = symlist();
	}
|	sdecls xasdecl
	{
		if($2.slist == nil)
			anonmemb(&$1, $2.btype.sue);
		else
			addsumembs(&$1, &$2);
		symlistfree($2.slist);
		$$ = $1;
	}

name:
	LNAME
|	LTAG

%%

void
typeclean(Btype *btype)
{
	int i, t;
	u32int tycl;
	Sym *sue;

	tycl = btype->tycl;
	sue = btype->sue;

	if(sue != nil) {
		if(tycl != 0)
			error("Cannot be a basic type and a structure type\n");
		return;
	}

	for(i = BVOID; i < NTYPE; i++) {
		if(tycl & 1<<i)
			break;
	}

	switch(i) {
	case NTYPE:
		t = BINT;
		break;
	case BSHORT:
		t = BSHORT;
		tycl &= ~(1<<BINT);
		break;
	case BINT:
		if(tycl & 1<<BLONG)
			t = BLONG;
		else if (tycl & 1<<BVLONG)
			t = BVLONG;
		else {
			t = BINT;
			break;
		}
		tycl &= ~(1<<BINT);
		break;
	default:
		t = i;
		break;
	}

	for(i = t+1; i < NTYPE; i++) {
		if(tycl & 1<<i)
			error("Overspecified type %d and %d.", t, i);
	}

	if(tycl & 1<<BSIGNED && tycl & 1<<BUNSIGNED)
		error("Cannot be signed and unsigned.");

	btype->tycl = tycl;
}

void
settype(Symlist *symlist, Btype *btype)
{
	Sym **s, *sym;
	Type *type, *newtype;

	if(symlist == nil)
		return;

	for(s = symlist->sp; s < symlist->ep; s++) {
		sym = *s;
		type = sym->type;
		newtype = sym->newtype;

		*(Btype*)newtype = *btype;

		if(type != nil) {
			typeeq(type, newtype);
			typefree(type);
		}
		sym->type = newtype;
		sym->newtype = nil;
	}
}

void
setchantype(Symlist *symlist, Type *t)
{
	Sym **s, *sym;
	Type *type, *newtype;

	if(symlist == nil)
		return;

	for(s = symlist->sp; s < symlist->ep; s++) {
		sym = *s;
		type = sym->type;
		newtype = sym->newtype;

		newtype->chantype = t;

		if(type != nil) {
			typeeq(type, newtype);
			typefree(type);
		}
		sym->type = newtype;
	}
}

int
typeeq(Type*, Type*)
{
	return 1;
}

u32int
addspec(u32int t, Sym *s)
{
	long tycl;

	if(s->tycl == BLONG) {
		if(t & 1<<BVLONG)
			warn("You already specified long long.");
		else
			t += 1<<BLONG;
		return t;
	}
	tycl = s->tycl;
	if(t & 1<<tycl)
		warn("You already specified %S.", s->name);
	return t | 1<<tycl;
}

void
anonmemb(struct Sumembs *m, Sym *s)
{
	if(s == nil || s->suetype == TENUM)
		error("Anonymous member must be a struct or union.");
	addparent(m->names, s->sunames);
	addsym(m->slist, s);
}

void
addsumembs(struct Sumembs *m, struct Decl *d)
{
	Sym **si, *s, *t;

	for(si = d->slist->sp; si < d->slist->ep; si++) {
		t = *si;
		s = lookup(m->names, t->name);
		if(s != nil)
			error("Structure member already defined: %S", s->name);

		s = sym(m->names, t->name);
		addsym(m->slist, s);

		s->type = t->newtype;
		*(Btype*)s->type = d->btype;
		t->newtype = nil;
	}
}
