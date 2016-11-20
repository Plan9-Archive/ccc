#include <u.h>
#include <libc.h>
#include <thread.h>
#include <llrb.h>
#include "ccc.h"

static void symlistinit(Symlist*);
static Symlist *sl;

Symlist*
symlist(void)
{
	Symlist *s;

	if(sl == nil) {
		s = emalloc(sizeof(*s));
		symlistinit(s);
	} else {
		s = sl;
		sl = sl->link;
		s->ep = s->sp;
	}
	return s;
}

static void
symlistinit(Symlist *s)
{
	s->sp = ecalloc(5, sizeof(*s->sp));
	s->ep = s->sp;
	s->cap = 5;
}

void
symlistfree(Symlist *s)
{
	s->link = sl;
	sl = s;
}

static Symlist *symlistrealloc(Symlist*);

Symlist*
addsym(Symlist *symlist, Sym *s)
{
	if(symlist->ep == symlist->sp + symlist->cap)
		symlistrealloc(symlist);
	*symlist->ep++ = s;
	return symlist;
}

static Symlist*
symlistrealloc(Symlist *s)
{
	uintptr len;

	len = s->ep - s->sp;
	s->cap *= 2;
	s->sp = erealloc(s->sp, sizeof(Sym) * s->cap);
	s->ep = s->sp + len;
	return s;
}

static void typelistinit(Typelist*);
static Typelist *tl;

Typelist*
typelist(void)
{
	Typelist *t;

	if(tl == nil) {
		t = emalloc(sizeof(*t));
		typelistinit(t);
	} else {
		t = tl;
		tl = tl->link;
		t->ep = t->sp;
	}
	return t;
}

static void
typelistinit(Typelist *t)
{
	t->sp = ecalloc(5, sizeof(*t->sp));
	t->ep = t->sp;
	t->cap = 5;
}

void
typelistfree(Typelist *t)
{
	t->link = tl;
	tl = t;
}

static Typelist *typelistrealloc(Typelist*);

Typelist*
addtype(Typelist *typelist, Type *t)
{
	if(typelist->ep == typelist->sp + typelist->cap)
		typelistrealloc(typelist);
	*typelist->ep++ = t;
	return typelist;
}

static Typelist*
typelistrealloc(Typelist *t)
{
	uintptr len;

	len = t->ep - t->sp;
	t->cap *= 2;
	t->sp = erealloc(t->sp, sizeof(Type) * t->cap);
	t->ep = t->sp + len;
	return t;
}

Dtype*
dtype(Type *t, int val)
{
	Dtype *d;

	d = emallocz(sizeof(*d));
	d->t = val;
	d->link = nil;
	if(t == nil)
		return d;

	*t->dtail = d;
	t->dtail = &d->link;
	return d;
}

Type*
type(void)
{
	Type *t;

	t = emallocz(sizeof(*t));
	t->dtail = &t->dtype;
	return t;
}

void
typefree(Type *t)
{
	Dtype *d, *l;

	if(t == nil)
		return;

	for(d = t->dtype; d != nil; d = l) {
		l = d->link;
		free(d);
	}
	typefree(t->chantype);
	free(t);
}
