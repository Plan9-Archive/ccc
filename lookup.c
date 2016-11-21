#include <u.h>
#include <libc.h>
#include <thread.h>
#include <llrb.h>
#include "ccc.h"
#include "y.tab.h"

int block, suelookup;
Names *namespace;

void
pushblock(void)
{
	namespace = pushnames(namespace);
	block++;
}

void
popblock(void)
{
	namespace = popnames(namespace);
	block = block == 0 ? 0 : block-1;
}

static int
symcmp(Llrb *a, Llrb *b)
{
	Sym *sa, *sb;

	sa = (Sym*)a;
	sb = (Sym*)b;

	if(sa->name[0] == sb->name[0])
		return runestrcmp(sa->name, sb->name);
	return sa->name[0] > sb->name[0] ? 1 : -1;
}

Names*
pushnames(Names *old)
{
	Names *ns;

	ns = emallocz(sizeof(*ns));
	ns->parent = old;
	ns->t = llrbcreate(symcmp);
	ns->block = old == nil ? 0 : old->block++;
	return ns;
}

Names *
addparent(Names *c, Names *p)
{
	Names *op;

	if(p == nil)
		return c;

	op = c->parent;
	c->parent = p;
	while(p->parent != nil)
		p = p->parent;
	p->parent = op;
	return c;
}

static void freesyms(Llrbtree *t);

Names*
popnames(Names *old)
{
	Names *new;

	if(old == nil)
		return nil;

	new = old->parent;
	freesyms(old->t);
	free(old->t);
	free(old);
	return new;
}

static void
freesyms(Llrbtree *t)
{
	Channel *c;
	Sym *s;

	fprint(2, "freeing syms\n");
	c = llrbwalk(t, nil, nil, POST);
	while((s = recvp(c)) != nil) {
		fprint(2, "freeing %S\n", s->name);
		symfree(s);
	}
	chanfree(c);
}

static Sym *newsym(Names*, Rune*);

Sym*
sym(Names *ns, Rune *n)
{
	Sym *s;

	s = lookup(ns, n);
	if(s == nil)
		return newsym(ns, n);
	return s;
}

void
symfree(Sym *s)
{
	Sym **si;
	Symlist *slist;

	if(s == nil)
		return;

	free(s->name);
	typefree(s->type);
	typefree(s->newtype);

	if(s->sulist != nil) {
		slist = s->sulist;
		for(si = slist->sp; si < slist->ep; si++)
			symfree(*si);
		symlistfree(s->sulist);
	}

	if(s->sunames != nil) {
		free(s->sunames->t);
		free(s->sunames);
	}

	free(s);
}

Sym*
lookup(Names *ns, Rune *n)
{
	Sym *s, l;

	if(ns == nil || n == nil)
		return nil;

	s = nil;
	l.name = n;
	for(; ns != nil; ns = ns->parent) {
		s = (Sym*)llrblookup(ns->t, &l);
		if(s != nil) {
			if(suelookup && s->suetype == 0)
				continue;
			break;
		}
	}
	suelookup = 0;
	return s;
}

static Sym*
newsym(Names *ns, Rune *n)
{
	Sym *s, l;

	if(n == nil)
		return emallocz(sizeof(Sym));

	if(ns != nil) {
		l.name = n;
		s = (Sym*)llrblookup(ns->t, &l);
		if(s != nil)
			return s;
	}

	s = emallocz(sizeof(*s));
	s->name = erunestrdup(n);
	s->lex = LNAME;
	if(ns == nil)
		return s;

	s->block = ns->block;
	llrbinsert(ns->t, s);
	return s;
}

void
printsyms(void)
{
	Channel *c;
	Sym *s;

	c = llrbwalk(namespace->t, nil, nil, IN);
	while((s = recvp(c)) != nil) {
		fprint(2, "trying to print %S\n", s->name);
		fprint(2, "%Y\n", s);
	}
	chanfree(c);
}
