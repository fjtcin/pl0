#include <stdio.h>//123

#define NRW        12     // number of reserved words, 新增ARRAY
#define TXMAX      500    // length of identifier table
#define MAXNUMLEN  14     // maximum number of digits in numbers
#define NSYM       13     // maximum number of symbols in array ssym and csym, 新增左右方括号括号
#define MAXIDLEN   10     // length of identifiers
#define MAXARRAYDIM 10 	  // maximum dimension of an array
#define MAXARRAYNUM 50    // maximum number of arrays

#define MAXADDRESS 32767  // maximum address
#define MAXLEVEL   32     // maximum depth of nesting block
#define CXMAX      500    // size of code array

#define MAXSYM     30     // maximum number of symbols

#define STACKSIZE  1000   // maximum storage

enum symtype
{
	SYM_NULL,
	SYM_IDENTIFIER,
	SYM_NUMBER,
	SYM_PLUS,
	SYM_MINUS,
	SYM_TIMES,
	SYM_SLASH,
	SYM_ODD,
	SYM_EQU,
	SYM_NEQ,
	SYM_LES,
	SYM_LEQ,
	SYM_GTR,
	SYM_GEQ,
	SYM_LPAREN,
	SYM_RPAREN,
	SYM_COMMA,
	SYM_SEMICOLON,
	SYM_PERIOD,
	SYM_BECOMES,
    SYM_BEGIN,
	SYM_END,
	SYM_IF,
	SYM_THEN,
	SYM_WHILE,
	SYM_DO,
	SYM_CALL,
	SYM_CONST,
	SYM_VAR,
	SYM_PROCEDURE,
	SYM_LBRACKET,
	SYM_RBRACKET,
	SYM_PRINT,
	SYM_ADDRESS
};

enum idtype
{
	ID_CONSTANT, ID_VARIABLE, ID_PROCEDURE, ID_ARRAY, ID_POINTER
};

enum opcode
{
	LIT, OPR, LOD, STO, CAL, INT, JMP, JPC, LEA, LODA, STOA, PRT, SWP
};

enum oprcode
{
	OPR_RET, OPR_NEG, OPR_ADD, OPR_MIN,
	OPR_MUL, OPR_DIV, OPR_ODD, OPR_EQU,
	OPR_NEQ, OPR_LES, OPR_LEQ, OPR_GTR,
	OPR_GEQ
};


typedef struct
{
	int f; // function code
	int l; // level
	int a; // displacement address
} instruction;

//////////////////////////////////////////////////////////////////////
char* err_msg[] =
{
/*  0 */    "",
/*  1 */    "Found ':=' when expecting '='.",
/*  2 */    "There must be a number to follow '='.",
/*  3 */    "There must be an '=' to follow the identifier.",
/*  4 */    "There must be an identifier to follow 'const', 'var', 'procedure', or 'array'.",
/*  5 */    "Missing ',' or ';'.",
/*  6 */    "Incorrect procedure name.",
/*  7 */    "Statement expected.",
/*  8 */    "Follow the statement is an incorrect symbol.",
/*  9 */    "'.' expected.",
/* 10 */    "';' expected.",
/* 11 */    "Undeclared identifier.",
/* 12 */    "Illegal assignment.",
/* 13 */    "':=' expected.",
/* 14 */    "There must be an identifier to follow the 'call'.",
/* 15 */    "A constant or variable can not be called.",
/* 16 */    "'then' expected.",
/* 17 */    "';' or 'end' expected.",
/* 18 */    "'do' expected.",
/* 19 */    "Incorrect symbol.",
/* 20 */    "Relative operators expected.",
/* 21 */    "Procedure identifier can not be in an expression.",
/* 22 */    "Missing ')'.",
/* 23 */    "The symbol can not be followed by a factor.",
/* 24 */    "The symbol can not be as the beginning of an expression.",
/* 25 */    "The number is too great.",
/* 26 */    "Invalid type argument of unary '*'",//*后不是指针
/* 27 */    "invalid operands of types pointer",//
/* 28 */    "invalid conversion from pointer/int to int/pointer",
/* 29 */    "lvalue required as unary '&' operand",//&&a
/* 30 */    "Invalid type argument of unary '&'",//不合法取址
/* 31 */    "wrong type argument to unary minus",
/* 32 */    "There are too many levels.",
/* 33 */	"Expecting '[' for array declaration or reference.",
/* 34 */	"Missing ']'.",
/* 35 */	"In dimension declaration must be a 'constant ID' or a 'number'.",
/* 36 */	"There must be a '(' to follow 'print'."
}; // 加下一个错误时记得加逗号

//////////////////////////////////////////////////////////////////////
char ch;         	// last character read
int  sym;        	// last symbol read
char id[MAXIDLEN + 1]; // last identifier read
int  num;        	// last number read
int  cc;         	// character count
int  ll;         	// line length
int  kk;
int  err;			// error counting
int  cx;        	// index of current instruction to be generated.
int  level = 0;		// current level
int  tx = 0;		// table scale (last index of table)
int  atx = 0;		// array table scale (last index of array table)
int  arrid=0;

char line[80];
//原文件的一行

instruction code[CXMAX];

char* word[NRW + 1] =
{
	"", /* place holder */
	"begin", "call", "const", "do", "end","if",
	"odd", "procedure", "then", "var", "while",
	"print"
};

int wsym[NRW + 1] =
{
	SYM_NULL, SYM_BEGIN, SYM_CALL, SYM_CONST, SYM_DO, SYM_END,
	SYM_IF, SYM_ODD, SYM_PROCEDURE, SYM_THEN, SYM_VAR, SYM_WHILE,
	SYM_PRINT
};

int ssym[NSYM + 1] =
{
	SYM_NULL, SYM_PLUS, SYM_MINUS, SYM_TIMES, SYM_SLASH,
	SYM_LPAREN, SYM_RPAREN, SYM_EQU, SYM_COMMA, SYM_PERIOD, SYM_SEMICOLON
	, SYM_LBRACKET, SYM_RBRACKET,SYM_ADDRESS
};

char csym[NSYM + 1] =
{
	' ', '+', '-', '*', '/', '(', ')', '=', ',', '.', ';', '[', ']','&'
};

#define MAXINS   13
char* mnemonic[MAXINS] =
{
	"LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC", "LEA", "LODA", "STOA", "PRT","SWP"
};

// const table entries
typedef struct
{
	char name[MAXIDLEN + 1];
	int  kind;
	int  value;
} comtab;

// id table
comtab table[TXMAX];

// var and procedure table entries
// var adress: 栈内偏移
// proc adress: 汇编块的入口编号
typedef struct
{
	char  name[MAXIDLEN + 1];
	int   kind;
	short level;
	short address;
} mask;
// 相比于常量表项，一个int变成两个short，所以大小一样
// 也用的id表存储

typedef struct
{
	char  name[MAXIDLEN +1];
	int   dim;
	int   dimlen[MAXARRAYDIM];
} arr;

arr arraytable[MAXARRAYNUM + 1];

//int exdim//expression's dim判断当前值是几级指针，0是整数，不为0时不能乘除

FILE* infile;

// EOF PL0.h
