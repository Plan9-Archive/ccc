typedef struct Sym Sym;
typedef struct Symlist Symlist;
typedef struct Type Type;
typedef struct Typelist Typelist;
typedef struct Dtype Dtype;
typedef struct Btype Btype;
typedef struct Names Names;

struct Sym {
	Llrb;
	int lex;
	Rune *name;
	Type *type;
	Type *newtype;
	Names *sunames;
	Symlist *sulist;
	uchar tycl;
	uchar block;
	uchar suetype;
};

struct Symlist {
	Sym **sp, Sym **ep;
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
	Dtype *dtype, Dtype **dtail;
};

struct Dtype {
	int t;
	int alen;
	Typelist *param;
	Dtype *link;
};

struct Typelist {
	Type **sp, **ep;
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

	TVOID = 0,
	TSHORT,
	TLONG,
	TVLONG,
	TCHAR,
	TINT,
	TDOUBLE,
	TFLOAT,
	NTYPE,
	BTYPE = NTYPE - 1,

	TSIGNED = NTYPE,
	TUNSIGNED,
	NSIGN,
	BSIGN = NSIGN-1 ^ BTYPE,

	TCONST = TUNSIGNED + 1,
	TAUTO,
	TSTATIC,
	TEXTERN,
	TTYPEDEF,
	NCLASS,
	BCLASS = NCLASS-1 ^ (BSIGN|BTYPE),

	TVOLATILE = NCLASS,
	TREGISTER,

	TDOTS,

	TPTR = 0,
	TARR,
	TFUNC,
	TSTRUCT,
	TUNION,
	TENUM
};

#pragma	varargck type "Y" Sym*
#pragma varargck type "T" Type*
#pragma varargck type "D" Type*

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
void printsyms(void);

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
void efmtprint(Fmt*, char*, ...);

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

extern int dbg, line, block, suelookup;
extern char filename[1024];
extern Names *namespace;
