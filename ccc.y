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
	struct Tspec {
		Btype;
		Dtype *dtype;
	} tspec;
	struct Decl {
		Btype btype;
		Dtype *dtype;
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
%type	<typelist>	paramlist paramlist1
%type	<sym>	declor declor1 name tycl sue
%type	<type>	abdeclor abdeclor1 abdeclor2 abdeclor3 param
%type	<tspec>	tcqspec
%type	<sumembs>	sdecls
%type	<ival>	zival

%token	<sym>	LNAME LVOID LCHAR LSHORT LINT LLONG LFLOAT
%token	<sym>	LDOUBLE LSIGNED LUNSIGNED LCONST LVOLATILE
%token	<sym>	LAUTO LREGISTER LSTATIC LEXTERN LTYPEDEF
%token	<sym>	LUNION LENUM LSTRUCT LTYPE
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
			nocopyout();
		} else {
			settype($1.slist, &$1.btype, $1.dtype);
			copyout();
		}
		symlistfree($1.slist);
	}

xasdecl:
	tcqspec ';'
	{
		btypeclean(&$1);

		$$.btype = $1;
		$$.dtype = $1.dtype;
		$$.slist = nil;
		$$.chantype = nil;
	}
|	tcqspec declors ';'
	{
		btypeclean(&$1);

		$$.btype = $1;
		$$.dtype = $1.dtype;
		$$.slist = $2;
		$$.chantype = nil;
	}
|	LCHAN '(' param ')' declors ';'
	{
		$$.btype = BZ;
		$$.dtype = nil;
		$$.slist = $5;
		$$.chantype = $3;
	}

tcqspec:
	{
		$$ = (struct Tspec){BZ, nil};
	}
|	tcqspec tycl
	{
		$$.tycl = addspec($1.tycl, $2->tycl);
	}
|	tcqspec sue
	{
		$$.sue = addsue(&$1, $2);
	}
|	tcqspec LTYPE
	{
		$$ = *addtypedef(&$1, $2->type);
	}

declors:
	declor
	{
		$$ = addsym(symlist(), $1);
	}
|	declors ',' declor
	{
		$$ = addsym($1, $3);
	}

declor:
	declor1
|	'*' declor
	{
		dtype($2->newtype, TPTR);
		$$ = $2;
	}

declor1:
	LNAME
	{
		$1->newtype = type();
		$$ = $1;
	}
|	'(' declor ')'
	{
		$$ = $2;
	}
|	declor1 '[' zival ']'
	{
		Dtype *d;

		d = dtype($1->newtype, TARR);
		d->alen = $3;
		$$ = $1;
	}
|	declor1 '(' paramlist ')'
	{
		Dtype *d;

		d = dtype($1->newtype, TFUNC);
		d->param = $3;
		$$ = $1;
	}

paramlist:
	paramlist1
|	paramlist1 ',' '.' '.' '.'
	{
		Type *t;

		t = type();
		t->tycl = 1<<TDOTS;
		addtype($1, t);
		$$ = $1;
	}

paramlist1:
	param
	{
		$$ = addtype(typelist(), paramconv($1));
	}
|	paramlist1 ',' param
	{
		$$ = addtype($1, paramconv($3));
	}

param:
	tcqspec declor
	{
		$$ = $2->newtype;
		$2->newtype = nil;
		btypeclean(&$1);
		*(Btype*)$$ = $1;
	}
|	tcqspec abdeclor
	{
		$$ = $2;
		btypeclean(&$1);
		*(Btype*)$$ = $1;
	}	

abdeclor:
	{
		$$ = type();
	}
|	abdeclor1

abdeclor1:
	abdeclor2
|	'*'
	{
		$$ = type();
		dtype($$, TPTR);
	}
|	'*' abdeclor1
	{
		dtype($2, TPTR);
		$$ = $2;
	}

abdeclor2:
	abdeclor3
|	abdeclor2 '(' paramlist ')'
	{
		Dtype *d;

		d = dtype($1, TFUNC);
		d->param = $3;
		$$ = $1;
	}
|	abdeclor2 '[' zival ']'
	{
		Dtype *d;

		d = dtype($1, TARR);
		d->alen = $3;
		$$ = $1;
	}

abdeclor3:
	'[' zival ']'
	{
		Dtype *d;

		$$ = type();
		d = dtype($$, TARR);
		d->alen = $2;
	}
|	'(' abdeclor1 ')'
	{
		$$ = $2;
	}

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

zival:
	{
		$$ = -1;
	}
|	LIVAL

name:
	LNAME
|	LTYPE

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

%%

Btype BZ = {0};

void
btypeclean(Btype *btype)
{
	int i, t;
	u32int tycl;
	Sym *sue;

	tycl = btype->tycl;
	sue = btype->sue;

	if(sue != nil) {
		if(tycl & (BTYPE|BSIGN))
			error("Cannot be a basic type and a structure type.");
		return;
	}

	for(i = TVOID; i < NTYPE; i++) {
		if(tycl & 1<<i)
			break;
	}

	switch(i) {
	case NTYPE:
		t = TINT;
		break;
	case TLONG:
		if(tycl & 1<<TDOUBLE) {
			tycl &= ~(1<<TLONG);
			t = TDOUBLE;
			break;
		}
	case TVLONG:
	case TSHORT:
		tycl &= ~(1<<TINT);
	default:
		t = i;
		break;
	}

	for(i = t+1; i < NTYPE; i++) {
		if(tycl & 1<<i)
			error("Overspecified type %d and %d.", t, i);
	}

	if(tycl & BSIGN) switch(t) {
	case TVOID:
	case TFLOAT:
	case TDOUBLE:
		error("%d cannot be signed or unsigned.", t);
	}
	
	btype->tycl = tycl;
}

void
settype(Symlist *symlist, Btype *btype, Dtype *dtype)
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
		*newtype->dtail = dtype;

		if(type != nil) {
			typeeq(type, newtype);
			typefree(type);
		}
		sym->type = newtype;
		sym->newtype = nil;
		if(btype->tycl & 1<<TTYPEDEF)
			sym->lex = LTYPE;
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

Type*
paramconv(Type *t)
{
	Dtype *d;

	if((d = t->dtype) != nil) switch(d->t) {
	case TARR:
		d->alen = 0;
		d->t = TPTR;
		break;
	case TFUNC:
		t->dtype = dtype(nil, TPTR);
		t->dtype->link = d;
		break;
	}
	return t;
}		

int
typeeq(Type*, Type*)
{
	return 1;
}

u32int
addspec(u32int t, int tycl)
{
	if(tycl < NTYPE) {
		if(tycl == TLONG) {
			if(t & 1<<TVLONG)
				error("You already specified long long.");
			else
				t += 1<<TLONG;
			return t;
		}
		if(t & 1<<tycl)
			error("You already specified type %d", tycl);
	} else if(tycl < NSIGN) {
		if(t & BSIGN)
			error("You already specified a sign.");
	} else if(tycl < NCLASS) {
		if(t & BCLASS)
			error("You already specified a class.");
	}
	return t | 1<<tycl;
}

Sym*
addsue(Btype *btype, Sym *sue)
{
	if(btype->sue != nil)
		error("Multiple struct declarations.");
	if(btype->tycl & BTYPE)
		error("Cannot be a struct and a basic type.");

	btype->sue = sue;
	return sue;
}

struct Tspec*
addtypedef(struct Tspec *tspec, Type *t)
{
	if(tspec->sue != nil || tspec->tycl & (BTYPE|BSIGN))
		error("Overspecified typedef application.");

	tspec->dtype = t->dtype;
	tspec->sue = t->sue;
	tspec->tycl |= t->tycl;

	return tspec;
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
		*s->type->dtail = d->dtype;
		t->newtype = nil;
	}
}
