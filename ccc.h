typedef struct Sym Sym;
typedef struct Symlist Symlist;
typedef struct Type Type;
typedef struct Typelist Typelist;
typedef struct Dtype Dtype;
typedef struct Btype Btype;
typedef struct Names Names;

struct Sym {
	Llrb;
	Rune *name;
	Type *type;
	Type *newtype;
	int lex;
	long tycl;
	int block;
	int suetype;
	Names *sunames;
	Symlist *sulist;
};

struct Symlist {
	Sym **sp;
	Sym **ep;
	int cap;
	Symlist *link;
};

struct Btype {
	Sym *sue;
	u32int tycl;
};

struct Type {
	Btype;
	Type *chantype;
	Dtype *dtype;
	Dtype **dtail;
};

struct Dtype {
	int t;
	int alen;
	Typelist *param;
	Dtype *link;
};

struct Typelist {
	Type **sp;
	Type **ep;
	int cap;
	Typelist *link;
};

struct Names {
	Llrbtree *t;
	int block;
	Names *parent;
};

enum {
	KVOID,
	KCHAR,
	KSHORT,
	KINT,
	KLONG,
	KFLOAT,
	KDOUBLE,
	KSIGNED,
	KUNSIGNED,
	KCONST,
	KVOLATILE,
	KAUTO,
	KREGISTER,
	KSTATIC,
	KEXTERN,
	KTYPEDEF,
	KCHAN,
	KSTRUCT,
	KUNION,
	KENUM,
	NKEYS,

	BVOID = 0,
	BCHAR,
	BSHORT,
	BINT,
	BLONG,
	BVLONG,
	BFLOAT,
	BDOUBLE,
	NTYPE,

	BSIGNED = NTYPE,
	BUNSIGNED,
	BCONST,
	BVOLATILE,
	BAUTO,
	BREGISTER,
	BSTATIC,
	BEXTERN,
	BTYPEDEF,
	BDOTS,

	TPTR = 0,
	TARR,
	TFUNC,
	TSTRUCT,
	TUNION,
	TENUM
};

#pragma	varargck type "Y" Sym*
#pragma varargck type "T" Type*
#pragma varargck type "D" Sym*

void usage(void);

int yyparse(void);
int yylex(void);
void yyerror(char*);

Sym *lookup(Names*, Rune*);
Sym *sym(Names*, Rune*);
void symfree(Sym*);
void pushblock(void);
void popblock(void);
Names *pushnames(Names*);
Names *popnames(Names*);
Names *addparent(Names*, Names*);

int typeeq(Type*, Type*);

Symlist *symlist(void);
void symlistfree(Symlist*);
Symlist *addsym(Symlist*, Sym*);
Typelist *typelist(void);
void typelistfree(Typelist*);
Typelist *addtype(Typelist*, Type*);
Type *type(void);
void typefree(Type*);
Dtype *dtype(Type*, int);

void error(char*, ...);
void warn(char*, ...);
void *emalloc(ulong);
void *emallocz(ulong);
void *ecalloc(ulong, ulong);
void *erealloc(void*, ulong);
Rune *erunestrdup(Rune*);

void setupout(void);
void pushfile(char*);
void pushstream(int);
Rune getr(void);
int ungetr(void);
void copyout(void);
void nocopyout(void);
void printchandecls(Symlist*);
int isspacer(Rune);
int isalphar(Rune);
int isdigitr(Rune);

extern int dbg, line, block;
extern char filename[1024];
extern Names *namespace;
