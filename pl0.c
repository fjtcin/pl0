// pl0 compiler source code

#pragma warning(disable:4996)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "PL0.h"
#include "set.c"

//////////////////////////////////////////////////////////////////////

// print error message.
// 调用者指定错误类型
void error(int n)
{
	int i;

	printf("      ");
	for (i = 1; i <= cc - 1; i++)
		printf(" ");
	printf("^\n");
	printf("Error %3d: %s\n", n, err_msg[n]);
	err++;
} // error

//////////////////////////////////////////////////////////////////////
void getch(void)
{
	if (cc == ll)// 读完了就读入新的一行
	{
		ll = cc = 0; //new line
		printf("%5d  ", cx);
		// 注意：这里打印原pl0代码，但每行行首用的是编译出来的行数
		while (ch != '.')
		{
			ch = getc(infile);
			if (ch == EOF)
			{
				printf("\nPROGRAM INCOMPLETE\n");
				exit(1);
			}
			if (ch == '\n') break;
			printf("%c", ch);
			line[++ll] = ch;
		} // while
		printf("\n");
		line[++ll] = ' ';
	}
	ch = line[++cc]; // 从当前读的行中取出一个
} // getch

//////////////////////////////////////////////////////////////////////

// gets a symbol from input stream.
void getsym(void)
// 词法识别
{
	int i, k;
	char a[MAXIDLEN + 1];

	while (ch == ' '||ch == '\t')
		getch();

	if (isalpha(ch))
	{ // symbol is a reserved word or an identifier.
		k = 0;
		do
		{
			if (k < MAXIDLEN)
				a[k++] = ch;
			getch();
		}
		while (isalpha(ch) || isdigit(ch));
		a[k] = 0;
		strcpy(id, a);
		word[0] = id; // 设置边界
		i = NRW; // 与关键字一一比较，到0停止
		while (strcmp(id, word[i--]));
		if (++i)
			sym = wsym[i]; // symbol is a reserved word
		else
			sym = SYM_IDENTIFIER;   // symbol is an identifier
	}
	else if (isdigit(ch))
	{ // symbol is a number.
		k = num = 0;
		sym = SYM_NUMBER;
		do
		{
			num = num * 10 + ch - '0';
			k++; // 位数+1
			getch();
		}
		while (isdigit(ch));
		if (k > MAXNUMLEN)
			error(25);     // The number is too great.
	}
	else if (ch == ':')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_BECOMES; // :=
			getch();
		}
		else if (ch == ':')
		{
			sym = SYM_SCOPE; // ::
			getch();
		}
		else
		{
			sym = SYM_NULL;       // ignored
		}
	}
	else if (ch == '>')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_GEQ;     // >=
			getch();
		}
		else
		{
			sym = SYM_GTR;     // >
		}
	}
	else if (ch == '<')
	{
		getch();
		if (ch == '=')
		{
			sym = SYM_LEQ;     // <=
			getch();
		}
		else if (ch == '>')
		{
			sym = SYM_NEQ;     // <>
			getch();
		}
		else
		{
			sym = SYM_LES;     // <
		}
	}
	else
	{ // other tokens
		i = NSYM;
		csym[0] = ch;
		while (csym[i--] != ch);
		// ' ', '+', '-', '*', '/', '(', ')', '=', ',', '.', ';', '[', ']', '&'
		if (++i) // 一一比较
		{
			sym = ssym[i];
			getch();
		}
		else
		{
			printf("Fatal Error: Unknown character.\n");
			exit(1);
		}
	}
} // getsym

//////////////////////////////////////////////////////////////////////

// generates (assembles) an instruction.
void gen(int x, int y, int z)
{
	if (cx > CXMAX)
	{
		printf("Fatal Error: Program too long.\n");
		exit(1);
	}
	code[cx].f = x;
	code[cx].l = y;
	code[cx++].a = z;
} // gen

void regen()
{
	if(code[cx-1].f==LODA)
		cx--;
	else if(code[cx-1].f==LOD)
		code[cx-1].f=LEA;
	else
	{
		error(30);
	}
}

//////////////////////////////////////////////////////////////////////

// tests if error (number n) occurs and skips all symbols that do not belongs to s1 or s2.
void test(symset s1, symset s2, int n)
// s1: 应该出现的symbol
// s2：寻找的symbol(找到s2也意味着语句有错)
{
	symset s;

	if (! inset(sym, s1))
	{
		error(n);
		s = uniteset(s1, s2);
		while(! inset(sym, s))
			getsym();
		destroyset(s);
	}
} // test

//////////////////////////////////////////////////////////////////////

int dx;  // last data offset in a level
// 注意：所有临时数据(用于操作符运算、条件及其他)都在栈顶，且用后释放

// enter object(constant, variable or procedre) into table.
void enter(int kind)
{
	mask* mk;

	tx++;
	strcpy(table[tx].name, id);
	table[tx].kind = kind;
	switch (kind)
	{
	case ID_CONSTANT:
		if (num > MAXADDRESS)
		{
			error(25); // The number is too great.
			num = 0;
		}
		table[tx].value = num;
		break;
	case ID_PROCEDURE:
		mk = (mask*) &table[tx];
		mk->level = level;
		break;
	default: // ID_VARIABLE and ID_ARRAY
		mk = (mask*) &table[tx];
		mk->level = level;
		mk->address = dx++;
		break;
	} // switch
} // enter

//////////////////////////////////////////////////////////////////////

// checks if the scope of the identifier is correct.
// 一个对象的*父亲*，即为符号表上方与之 leveldiff = 1 的*最近*过程名。
int check_scopes(int i, int k, int l)
{
	if (k < 0 || l < 0) return 1;
	while (--i)
	{
		mask* mk = (mask*) &table[i];
		if (mk->kind == ID_PROCEDURE && mk->level == l)
			return !strcmp(scopes[k], mk->name) && check_scopes(i, k-1, mk->level-1);
	}
	return 0;
}

// locates identifier in symbol table.
int position()
{
	int i = tx + 1;
	int k = scope_top - 1;
	while (--i)
	{
		mask* mk = (mask*) &table[i];
		if (scopes[0][0] && mk->level >= k || !scopes[0][0] && mk->level == k - 1)
			if (!strcmp(scopes[k], mk->name))
				if (check_scopes(i, k-1, mk->level-1))
					break;
	}
	return i;
} // position

int arrposition(int i)
{
	// i should be greater than 0
	int cnt = 0;
	for (int j = 1; j <= i; ++j) {
		if (!strcmp(table[j].name, table[i].name)) ++cnt;
	}
	for (int j = 1; j <= atx; ++j) {
		if (!strcmp(arraytable[j].name, table[i].name)) --cnt;
		if (!cnt) return j;
	}
	exit(1); // impossible
}

// Scope Resolution
int handle_scopes()
{
	while (sym == SYM_SCOPE)
	{
		getsym();
		if (sym == SYM_IDENTIFIER)
		{
			strcpy(scopes[scope_top++], id);
			getsym();
		}
		else
		{
			error(37); // There must be an identifier to follow '::'.
		}
		if (scope_top >= MAXSCOPE)
		{
			error(38); // There are too many scopes.
		}
	}
	int i = position();
	scope_top = 0;
	return i;
}

//////////////////////////////////////////////////////////////////////
void constdeclaration()
{
	if (sym == SYM_IDENTIFIER)
	{
		getsym();
		if (sym == SYM_EQU || sym == SYM_BECOMES)
		{
			if (sym == SYM_BECOMES)
				error(1); // Found ':=' when expecting '='.
			getsym();
			if (sym == SYM_NUMBER)
			{
				enter(ID_CONSTANT);
				getsym();
			}
			else
			{
				error(2); // There must be a number to follow '='.
			}
		}
		else
		{
			error(3); // There must be an '=' to follow the identifier.
		}
	}
	else if (sym == SYM_SCOPE)
	{
		error(29); // Declarations should be made within the scope.
	}
	else
	{
		error(4);	 // There must be an identifier to follow 'const', 'var', or 'procedure'.
	}
} // constdeclaration

//////////////////////////////////////////////////////////////////////
void vardeclaration(void)
{
	int dimx=0;
	if (sym == SYM_IDENTIFIER)
	{
		enter(ID_VARIABLE);
		getsym();
	}
	else if (sym==SYM_TIMES)
	{
		do
		{
			dimx++;
			getsym();
		} while (sym==SYM_TIMES);
		getsym();
		if(sym!=SYM_LBRACKET)
		{
			enter(ID_POINTER);
			atx++;
			strcpy(arraytable[atx].name, table[tx].name);
			arraytable[atx].dim = dimx;
			// for (int i=0;i<=MAXARRAYDIM;i++)arraytable[atx].dimlen[i]=0;
		}
		else
		{
			enter(ID_VARIABLE);
		}
	}
	else if (sym == SYM_SCOPE)
	{
		error(29); // Declarations should be made within the scope.
	}
	else
	{
		error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
	}
	if(sym == SYM_LBRACKET)
	{
		table[tx].kind = ID_ARRAY;
		atx++;
		int arrdim;
		int arrspace;
		arrdim = 0;
		arrspace = 1;
		strcpy(arraytable[atx].name, table[tx].name);
		do
		{
			getsym();
			if (sym == SYM_IDENTIFIER || sym == SYM_SCOPE)
			{
				int i;
				if (sym == SYM_SCOPE) {
					scopes[scope_top++][0] = 0;
					i = handle_scopes();
				} else {
					strcpy(scopes[scope_top++], id);
					getsym();
					i = handle_scopes();
				}
				if (!i) error(11); // Undeclared identifier.
				else if (table[i].kind != ID_CONSTANT) error(35);
				// In dimension declaration must be a 'constant ID' or a 'number'.
				else arraytable[atx].dimlen[arrdim++] = table[i].value;
				arrspace *= table[i].value;
			}
			else if (sym == SYM_NUMBER)
			{
				arraytable[atx].dimlen[arrdim] = num;
				arrdim++;
				arrspace *= num;
				getsym();
			}
			else error(35); // In dimension declaration must be a 'constant ID' or a 'number'.
			if(sym != SYM_RBRACKET) error(34); // missing ']'
			getsym();
		} while (sym == SYM_LBRACKET);
		dx += arrspace - 1;
		arraytable[atx].ptdim=dimx;
		arraytable[atx].dim = arrdim;
	}
} // vardeclaration

int arrayoffset(int iarray, int st) {
	int offset = 1;
	for (int j = st + 1; j < arraytable[iarray].dim; j++) offset *= arraytable[iarray].dimlen[j];
	return offset;
}

// generate the address of an array element and push it to the top of the stack
// return dim - number of index
// dim == number of index (flag == 0): treat as var/number;
// dim != number of index (flag != 0): treat as address/pointer.
int arrayindex(symset fsys, int itable, int iarray)
{
	int expression(symset fsys);
	symset set, set1;
	int i, offset;
	int tempdim = arraytable[iarray].dim;
	mask* mk = (mask*) &table[itable];

	gen(LEA, level - mk->level, mk->address);

	for (i = 0; i < tempdim; i++)
	{
		if (sym != SYM_LBRACKET) break;
		getsym();
		offset = arrayoffset(iarray, i);
		gen(LIT, 0, offset);
		set1 = createset(SYM_RBRACKET);
		set = uniteset(set1, fsys);
		expression(set);
		destroyset(set);
		destroyset(set1);
		gen(OPR, 0, OPR_MUL);
		gen(OPR, 0, OPR_ADD);
		if (sym != SYM_RBRACKET) error(34);
		getsym();
	}
	return tempdim - i;
}

//////////////////////////////////////////////////////////////////////

// 打印生成的汇编指令
void listcode(int from, int to)
{
	int i;

	printf("\n");
	for (i = from; i < to; i++)
	{
		printf("%5d %s\t%d\t%d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
	}
	printf("\n");
} // listcode

//////////////////////////////////////////////////////////////////////

// factor -> ident | number | -factor | (expr)
// factor -> ident {stack[top] = ident.value}
// factor -> number {stack[top] = number}
// factor -> -factor {stack[top] = -stack[top]}
// factor -> (expr) {}
int factor(symset fsys)
{
	int expression(symset fsys);
	int i, flag=0, iarray;
	symset set;

	test(facbegsys, fsys, 24); // The symbol can not be as the beginning of an expression.

	if (inset(sym, facbegsys))
	// else if: sym在fsys中, 直接跳过factor的分析
	{
		if(sym==SYM_TIMES)
		{
			getsym();
			int dm=factor(fsys);
			if(dm==0)
			{
				error(26);
			}
			else if(dm>0)
			{
				gen(LODA, 0, 0);
				flag=dm-1;
			}
			else
			{
				// dm 如果是负的，就说明当前指针是数组指针
				int arrid=((-dm)&mask1)>>16;
				// arrid: 如果当前指针是数组指针，就是数组编号，否则就是0
				// 也就是 dm < 0 时 arrid 才非 0
				dm=((-dm)&mask2);
				if(dm-1==arraytable[arrid].ptdim)
				{
					// 由数组指针变为普通指针或变量
					gen(LODA,0,0);
					flag=dm-1;
				}
				else
				{
					flag=-(dm|(arrid<<16))+1;
				}
			}
		}
		else if(sym==SYM_ADDRESS)
		{
			getsym();
			int dm=factor(fsys);
			regen();
			flag=dm>=0?dm+1:dm-1;
		}
		else if (sym == SYM_IDENTIFIER || sym == SYM_SCOPE)
		{
			if (sym == SYM_SCOPE) {
				scopes[scope_top++][0] = 0;
				i = handle_scopes();
			} else {
				strcpy(scopes[scope_top++], id);
				getsym();
				i = handle_scopes();
			}
			if (!i)
			{
				error(11); // Undeclared identifier.
			}
			else
			{
				switch (table[i].kind)
				{
					mask* mk;
				case ID_CONSTANT:
				// factor -> ident, 把ident_const值直接置为栈顶
					gen(LIT, 0, table[i].value);
					flag = 0;
					break;
				case ID_VARIABLE:
				// factor -> ident_vari, 把这个值取出来置为栈顶
					mk = (mask*) &table[i];
					gen(LOD, level - mk->level, mk->address);
					flag = 0;
					break;
				case ID_ARRAY:
					iarray = arrposition(i);
					flag = arrayindex(fsys, i, iarray)+arraytable[iarray].ptdim;
					if(flag==arraytable[iarray].ptdim) gen(LODA, 0, 0);
					else flag|=(iarray<<16),flag=-flag;
					break;
				case ID_PROCEDURE:
					error(21); // Procedure identifier can not be in an expression.
					break;
				case ID_POINTER:
					iarray = arrposition(i);
					flag = arraytable[iarray].dim;
					mk = (mask*) &table[i];
					gen(LOD, level - mk->level, mk->address);
					break;
					// factor -> ident_pointer, 把这个指针取出来置为栈顶
				} // switch
			}
		}
		else if (sym == SYM_NUMBER)
		// factor -> number, 把这个数置为栈顶
		{
			if (num > MAXADDRESS)
			{
				error(25); // The number is too great.
				num = 0;
			}
			gen(LIT, 0, num);
			getsym();
			flag = 0;
		}
		else if (sym == SYM_LPAREN)
		// factor -> (expr), 把表达式的值置为栈顶
		{
			getsym();
			set = uniteset(createset(SYM_RPAREN, SYM_NULL), fsys); // 遇到 ')' 和 NULL 即可错误恢复
			flag=expression(set);
			destroyset(set);
			if (sym == SYM_RPAREN)
			{
				getsym();
			}
			else
			{
				error(22); // Missing ')'.
			}
		}
		else if(sym == SYM_MINUS)
		// factor -> -factor
		{
			getsym();
			flag=factor(fsys);
			if(flag>0)
			{
				error(31);
			}
			else
				gen(OPR, 0, OPR_NEG); //这里是OPR_NEG，注意OPR_NEG和OPR_MIN的区别
		}
		test(fsys, createset(SYM_LPAREN, SYM_RPAREN, SYM_NULL), 23); // ')' added for print
	} // if
	return flag;
} // factor

//////////////////////////////////////////////////////////////////////

// term -> factor ((*|/) factor)*
// term -> factor {}
// term -> term*factor { top--; stack[top] = stack[top]*stack[top+1]}
// term -> term/factor { top--; stack[top] = stack[top]/stack[top+1]}
int term(symset fsys)
{
	int mulop;
	symset set;

	set = uniteset(fsys, createset(SYM_TIMES, SYM_SLASH, SYM_NULL));

	int dm=factor(set);
	int arrid=(dm>=0 ? 0 : ((-dm)&mask1)>>16);
	dm=(dm>=0?(dm&mask2):-((-dm)&mask2));
	while (sym == SYM_TIMES || sym == SYM_SLASH)
	{
		if(dm!=0)
		{
			error(27);
		}
		mulop = sym;
		getsym();
		dm=factor(set);
		dm=(dm>=0?(dm&mask2):-((-dm)&mask2));
		if(dm!=0)
		{
			error(27);
		}
		if (mulop == SYM_TIMES)
		{
			gen(OPR, 0, OPR_MUL);
		}
		else
		{
			gen(OPR, 0, OPR_DIV);
		}
	} // while
	destroyset(set);
	return dm>=0?dm:-((-dm)|(arrid<<16));
} // term

//////////////////////////////////////////////////////////////////////

// expression -> term ((+|-) term)*
// expression -> term {}
// expression -> expression+term { top--; stack[top] = stack[top]+stack[top+1]}
// expression -> expression-term { top--; stack[top] = stack[top]-stack[top+1]}
int expression(symset fsys)
{
	int addop,arrid;
	symset set;

	set = uniteset(fsys, createset(SYM_PLUS, SYM_MINUS, SYM_NULL));

	int dm=term(set),dm1=0;
	arrid=(dm>=0 ? 0 : ((-dm)&mask1)>>16);
	dm=(dm>=0?(dm&mask2):-((-dm)&mask2));
	while (sym == SYM_PLUS || sym == SYM_MINUS)
	{
		addop = sym;
		getsym();
		dm1=term(set);
		if (addop == SYM_PLUS)
		{
			arrid|=(dm1>=0 ? 0 : ((-dm1)&mask1)>>16);
			dm1=(dm1>=0?(dm1&mask2):-((-dm1)&mask2));
			if(dm1==0&&dm==0||dm1>0&&dm==0||dm>0&&dm1==0)
				gen(OPR, 0, OPR_ADD);
			else if(dm1==0&&dm<0||dm1<0&&dm==0)
			{
				if(dm<0)
				{
					int offset = arrayoffset(arrid, arraytable[arrid].dim+dm);
					gen(LIT,0,offset);
					gen(OPR,0,OPR_MUL);
					gen(OPR,0,OPR_ADD);
				}
				else
				{
					gen(SWP,0,0);
					int offset = arrayoffset(arrid, arraytable[arrid].dim+dm);
					gen(LIT,0,offset);
					gen(OPR,0,OPR_MUL);
					gen(OPR,0,OPR_ADD);
				}
			}
			else
			{
				error(27);
			}
		}
		else
		{
			dm1=(dm1>=0?(dm1&mask2):-((-dm1)&mask2));
			if (dm1 == 0)
			{
				if(dm >= 0)
					gen(OPR, 0, OPR_MIN);
				else
				{
					int offset = arrayoffset(arrid, arraytable[arrid].dim+dm);
					gen(LIT,0,offset);
					gen(OPR,0,OPR_MUL);
					gen(OPR,0,OPR_MIN);
				}
			}
			else
			{
				if(dm1!=dm&&dm1!=-dm)
				{
					error(27);
				}
				dm=0;
				gen(OPR, 0, OPR_MIN);
			}
		}
	} // while

	destroyset(set);
	return dm>=0?dm:-((-dm)|(arrid<<16));
} // expression

//////////////////////////////////////////////////////////////////////

// condition -> odd expression | expression (= | <> | ...) expression
// condition -> odd expression { stack[top] = stack[top]%2 }
// condition -> expression = expression { top--; stack[top] = (stack[top] == stack[top-1]); }
// ... ('=', '<>', '<', '>=', '>', '<=')
void condition(symset fsys)
{
	int relop;
	symset set;

	if (sym == SYM_ODD)
	{
		getsym();
		expression(fsys);
		gen(OPR, 0, OPR_ODD);
	}
	else
	{
		set = uniteset(relset, fsys);
		expression(set);
		destroyset(set);
		if (! inset(sym, relset))
		{
			error(20);
		}
		else
		{
			relop = sym;
			getsym();
			expression(fsys);
			switch (relop)
			{
			case SYM_EQU:
				gen(OPR, 0, OPR_EQU);
				break;
			case SYM_NEQ:
				gen(OPR, 0, OPR_NEQ);
				break;
			case SYM_LES:
				gen(OPR, 0, OPR_LES);
				break;
			case SYM_GEQ:
				gen(OPR, 0, OPR_GEQ);
				break;
			case SYM_GTR:
				gen(OPR, 0, OPR_GTR);
				break;
			case SYM_LEQ:
				gen(OPR, 0, OPR_LEQ);
				break;
			} // switch
		} // else
	} // else
} // condition

//////////////////////////////////////////////////////////////////////

// statement -> ident := expr | call ident | begin statement (;statement)* ; end
// | if condition then statement | while condition do statement
// statement -> ident := expr { stack[position_of_ident] = stack[top] }
// statement -> ident arrayindex := expr
// statement -> call ident { call position_of_ident }
// statement -> begin statement { } (; statement { })* ; end
// statement -> if condition then statement { ... }
// statement -> while condition do statement { ... }
// statement -> print(expression+)
void statement(symset fsys)
{
	int i, iarray, cx1, cx2, flag, dm, dm1;
	symset set1, set;

	if(sym==SYM_TIMES)
	{
		getsym();
		dm=abs(expression(fsys))&mask2;
		if(sym!=SYM_BECOMES)
		{
			error(13);
		}
		getsym();
		dm1=abs(expression(fsys))&mask2;
		if(dm-1!=dm1)
		{
			error(28);
		}
		if(dm==0)
		{
			error(26);
		}
		gen(STOA,0,0);
	}
	else if (sym == SYM_IDENTIFIER || sym == SYM_SCOPE)
	{ // variable assignment
		mask* mk;
		if (sym == SYM_SCOPE) {
			scopes[scope_top++][0] = 0;
			i = handle_scopes();
		} else {
			strcpy(scopes[scope_top++], id);
			getsym();
			i = handle_scopes();
		}
		if (!i)
		{
			error(11); // Undeclared identifier.
		}
		else if (table[i].kind == ID_POINTER)
		{
			int idpt=arrposition(i);
			if(sym!=SYM_BECOMES)
			{
				error(13);
			}
			getsym();
			int dim=abs(expression(fsys))&mask2;
			if(arraytable[idpt].dim!=dim)
			{
				error(28);
			}
			mk = (mask*) &table[i];
			gen(STO, level-mk->level, mk->address);
		}
		else if (table[i].kind == ID_VARIABLE)
		{
			if (sym == SYM_BECOMES)
			{
				getsym();
			}
			else
			{
				error(13); // ':=' expected.
			}
			if(expression(fsys)!=0)
			{
				error(28);
			}
			mk = (mask*) &table[i];
			gen(STO, level - mk->level, mk->address);
		}
		else if (table[i].kind == ID_ARRAY)
		{
			iarray = arrposition(i);
			flag = arrayindex(fsys, i, iarray);
			if (flag) error(12);
			else if (sym == SYM_BECOMES)
			{
				getsym();
			}
			else
			{
				error(13); // ':=' expected.
			}
			if((abs(expression(fsys))&mask2)!=arraytable[iarray].ptdim)
			{
				error(28);
			}
			gen(STOA, 0, 0);
		}
		else
		{
			error(12); // Illegal assignment.
			i = 0;
		}
	}
	else if (sym == SYM_CALL)
	{ // procedure call
		getsym();
		if (sym == SYM_IDENTIFIER || sym == SYM_SCOPE)
		{
			if (sym == SYM_SCOPE) {
				scopes[scope_top++][0] = 0;
				i = handle_scopes();
			} else {
				strcpy(scopes[scope_top++], id);
				getsym();
				i = handle_scopes();
			}
			if (!i)
			{
				error(11); // Undeclared identifier.
			}
			else if (table[i].kind == ID_PROCEDURE)
			{
				mask* mk;
				mk = (mask*) &table[i];
				gen(CAL, level - mk->level, mk->address);
			}
			else
			{
				error(15); // A constant or variable can not be called.
			}
		}
		else
		{
			error(14); // There must be an identifier to follow the 'call'.
		}
	}
	else if (sym == SYM_IF)
	{ 	// statement -> if condition then statement
		// cx1: condition完成时的位置
		getsym();
		set1 = createset(SYM_THEN, SYM_DO, SYM_NULL);
		set = uniteset(set1, fsys);
		condition(set);
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_THEN)
		{
			getsym();
		}
		else
		{
			error(16); // 'then' expected.
		}
		cx1 = cx;
		gen(JPC, 0, 0); // JPC: jump if stack[top] == 0
		statement(fsys);
		code[cx1].a = cx;	// 回写
	}
	else if (sym == SYM_BEGIN)
	{ 	// begin statement (; statement)* ; end
		getsym();
		set1 = createset(SYM_SEMICOLON, SYM_END, SYM_NULL);
		set = uniteset(set1, fsys);
		statement(set);
		while (sym == SYM_SEMICOLON || inset(sym, statbegsys))
		{
			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(10); // ';' expected.
			}
			statement(set);
		} // while
		destroyset(set1);
		destroyset(set);
		if (sym == SYM_END)
		{
			getsym();
		}
		else
		{
			error(17); // ';' or 'end' expected.
		}
	}
	else if (sym == SYM_WHILE)
	{ 	// while condition do statement
		// cx1 condition语句起始位置
		// cx2 condition语句完成时的位置
		cx1 = cx;
		getsym();
		set1 = createset(SYM_DO, SYM_NULL);
		set = uniteset(set1, fsys);
		condition(set);
		destroyset(set1);
		destroyset(set);
		cx2 = cx;
		gen(JPC, 0, 0); // condition 不满足，跳到 do 之后
		if (sym == SYM_DO)
		{
			getsym();
		}
		else
		{
			error(18); // 'do' expected.
		}
		statement(fsys);
		gen(JMP, 0, cx1); // do 完无条件跳回 condition
		code[cx2].a = cx; // 回写
	}
	else if (sym == SYM_PRINT)
	{
		getsym();
		if(sym != SYM_LPAREN) error(36); // "There must be a '(' to follow 'print'."
		do
		{
			getsym();
			set1 = createset(SYM_COMMA, SYM_RPAREN);
			set = uniteset(set1, fsys);
			expression(set);
			destroyset(set1);
			destroyset(set);
			gen(PRT, 0, 0);
		} while (sym == SYM_COMMA);
		if (sym != SYM_RPAREN) error(22); // missing ')'
		getsym();
		gen(PRT, 0, 1);
	}
	test(fsys, phi, 19); // "Incorrect symbol."
} // statement

//////////////////////////////////////////////////////////////////////

// block -> const (ident = number (,ident = number)* ;)+ block
// block -> var (ident (,ident)* ;)+ block
// block -> procedure ident ; block ; block
// block -> statement
// 最高层的block没有名字
void block(symset fsys)
{
	int cx0; // initial code index
	mask* mk;
	int block_dx;
	int savedTx;
	symset set1, set;

	dx = 3;
	block_dx = dx;
	mk = (mask*) &table[tx];
	mk->address = cx;
	gen(JMP, 0, 0);
	if (level > MAXLEVEL)
	{
		error(32); // There are too many levels.
	}
	do
	{
		if (sym == SYM_CONST)
		{ 	// block -> (ident = number (,ident = number)* ;)+
			getsym();
			do
			{
				constdeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					constdeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
			if (sym == SYM_SCOPE) error(29); // Declarations should be made within the scope.
		} // if

		if (sym == SYM_VAR)
		{  // block -> var (ident (,ident)* ;)+ block
			getsym();
			do
			{
				vardeclaration();
				while (sym == SYM_COMMA)
				{
					getsym();
					vardeclaration();
				}
				if (sym == SYM_SEMICOLON)
				{
					getsym();
				}
				else
				{
					error(5); // Missing ',' or ';'.
				}
			}
			while (sym == SYM_IDENTIFIER);
			if (sym == SYM_SCOPE) error(29); // Declarations should be made within the scope.
		} // if
		block_dx = dx; //save dx before handling procedure call!
		while (sym == SYM_PROCEDURE)
		{ // procedure declarations
			getsym();
			if (sym == SYM_IDENTIFIER)
			{
				enter(ID_PROCEDURE);
				getsym();
			}
			else if (sym == SYM_SCOPE)
			{
				error(29); // Declarations should be made within the scope.
			}
			else
			{
				error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
			}


			if (sym == SYM_SEMICOLON)
			{
				getsym();
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}

			level++;
			savedTx = tx;
			set1 = createset(SYM_SEMICOLON, SYM_NULL);
			set = uniteset(set1, fsys);
			block(set);
			destroyset(set1);
			destroyset(set);
			tx = savedTx; // 只能调用儿子，不能调用孙子及以下，因为孙子以下的函数记录全部被覆盖了
			level--;

			if (sym == SYM_SEMICOLON)
			{
				getsym();
				// set1 = createset(SYM_IDENTIFIER, SYM_PROCEDURE, SYM_NULL);
				// set = uniteset(statbegsys, set1);
				// test(set, fsys, 6); // Incorrect procedure name.
				// destroyset(set1);
				// destroyset(set);
			}
			else
			{
				error(5); // Missing ',' or ';'.
			}
		} // while
		dx = block_dx; //restore dx after handling procedure call!
		// set1 = createset(SYM_IDENTIFIER, SYM_NULL);
		// set = uniteset(statbegsys, set1);
		// test(set, declbegsys, 7); //Statement expected.
		// destroyset(set1);
		// destroyset(set);
	}
	while (inset(sym, declbegsys));

	code[mk->address].a = cx; // 所有定义完成，从这里开始执行(回写第一句JMP)
	mk->address = cx;
	cx0 = cx;
	gen(INT, 0, block_dx);
	set1 = createset(SYM_SEMICOLON, SYM_BECOMES, SYM_END, SYM_NULL);
	set = uniteset(set1, fsys);
	statement(set);
	destroyset(set1);
	destroyset(set);
	gen(OPR, 0, OPR_RET); // return
	test(fsys, phi, 8); // test for error: Follow the statement is an incorrect symbol.
	listcode(cx0, cx); // 打印这个函数的汇编块
} // block

//////////////////////////////////////////////////////////////////////

// 根据层差找到基址
int base(int stack[], int currentLevel, int levelDiff)
{
	int b = currentLevel;

	while (levelDiff--)
		b = stack[b];
	return b;
} // base

//////////////////////////////////////////////////////////////////////

// interprets and executes codes.
void interpret()
{
	int pc;        // program counter
	int stack[STACKSIZE];
	int top;       // top of stack
	int b;         // program, base, and top-stack register
	instruction i; // instruction register

	printf("Begin executing PL/0 program.\n");

	pc = 0;
	b = 1;
	top = 3;
	stack[1] = stack[2] = stack[3] = 0;
	// 栈帧前三项：访问链、控制链、返回地址
	do
	{
		i = code[pc++]; // 用pc调用
		switch (i.f)
		{
		case LIT: // load Immediate to the top
			stack[++top] = i.a;
			break;
		case OPR:
			switch (i.a) // operator
			{
			case OPR_RET:
				top = b - 1;
				pc = stack[top + 3];
				b = stack[top + 2];
				break;
			case OPR_NEG:
				stack[top] = -stack[top];
				break;
			case OPR_ADD:
				top--;
				stack[top] += stack[top + 1];
				break;
			case OPR_MIN:
				top--;
				stack[top] -= stack[top + 1];
				break;
			case OPR_MUL:
				top--;
				stack[top] *= stack[top + 1];
				break;
			case OPR_DIV:
				top--;
				if (stack[top + 1] == 0)
				{
					fprintf(stderr, "Runtime Error: Divided by zero.\n");
					fprintf(stderr, "Program terminated.\n");
					continue;
				}
				stack[top] /= stack[top + 1];
				break;
			case OPR_ODD:
				stack[top] %= 2;
				break;
			case OPR_EQU:
				top--;
				stack[top] = stack[top] == stack[top + 1];
				break;
			case OPR_NEQ:
				top--;
				stack[top] = stack[top] != stack[top + 1];
				break;
			case OPR_LES:
				top--;
				stack[top] = stack[top] < stack[top + 1];
				break;
			case OPR_GEQ:
				top--;
				stack[top] = stack[top] >= stack[top + 1];
				break;
			case OPR_GTR:
				top--;
				stack[top] = stack[top] > stack[top + 1];
				break;
			case OPR_LEQ:
				top--;
				stack[top] = stack[top] <= stack[top + 1];
				break;
			} // switch
			break;
		case LOD:
			stack[++top] = stack[base(stack, b, i.l) + i.a];
			break;
		case STO:
			stack[base(stack, b, i.l) + i.a] = stack[top];
			// printf("%d\n", stack[top]);
			top--;
			break;
		case CAL:
			stack[top + 1] = base(stack, b, i.l); // 访问链(被调用者的上层，注意l.i的意义)，被调用者的b
			stack[top + 2] = b; // 控制链
			stack[top + 3] = pc; // 返回地址
			b = top + 1;
			pc = i.a; // 注意precedure的address
			break;
		case INT:
			top += i.a; //开辟空间，当且仅当函数的变量声明完的时候用
			break;
		case JMP:
			pc = i.a;
			break;
		case JPC:
			if (stack[top] == 0)
				pc = i.a;
			top--;
			break;
		case LEA:
			stack[++top] = base(stack, b, i.l) + i.a;
			break;
		case LODA:
			stack[top] = stack[stack[top]];
			break;
		case STOA:
			stack[stack[top - 1]] = stack[top];
			top -= 2;
			break;
		case PRT:
			switch (i.a)
			{
			case 0:
				printf("%d\t", stack[top--]);
				break;
			case 1:
				printf("\n");
				break;
			}
			break;
		case SWP:
			int tmp;
			tmp=stack[top];
			stack[top]=stack[top-1];
			stack[top-1]=tmp;
			break;
		} // switch
	}
	while (pc);

	printf("End executing PL/0 program.\n");
} // interpret

//////////////////////////////////////////////////////////////////////

// program -> block.
void main ()
{
	FILE* hbin;
	char s[80]; // file name to be compiled
	int i;
	symset set, set1, set2;

	printf("Please input source file name: "); // get file name to be compiled
	scanf("%s", s);
	if ((infile = fopen(s, "r")) == NULL)
	{
		printf("File %s can't be opened.\n", s);
		exit(1);
	}

	phi = createset(SYM_NULL);
	relset = createset(SYM_EQU, SYM_NEQ, SYM_LES, SYM_LEQ, SYM_GTR, SYM_GEQ, SYM_NULL);

	// create begin symbol sets
	declbegsys = createset(SYM_CONST, SYM_VAR, SYM_PROCEDURE, SYM_NULL);
	statbegsys = createset(SYM_BEGIN, SYM_CALL, SYM_IF, SYM_WHILE, SYM_NULL);
	facbegsys = createset(SYM_SCOPE, SYM_IDENTIFIER, SYM_NUMBER, SYM_TIMES, SYM_LPAREN, SYM_MINUS, SYM_ADDRESS, SYM_NULL);

	err = cc = cx = ll = 0; // initialize global variables
	ch = ' ';
	kk = MAXIDLEN;

	getsym();

	set1 = createset(SYM_PERIOD, SYM_NULL);
	set2 = uniteset(declbegsys, statbegsys);
	set = uniteset(set1, set2);
	block(set); // program -> block.
	destroyset(set1);
	destroyset(set2);
	destroyset(set);
	destroyset(phi);
	destroyset(relset);
	destroyset(declbegsys);
	destroyset(statbegsys);
	destroyset(facbegsys);

	if (sym != SYM_PERIOD)
		error(9); // '.' expected.
	if (err == 0)
	{
		hbin = fopen("hbin.txt", "w");
		for (i = 0; i < cx; i++)
			fwrite(&code[i], sizeof(instruction), 1, hbin);
		fclose(hbin);
	}
	if (err == 0)
		interpret();
	else
		printf("There are %d error(s) in PL/0 program.\n", err);
	listcode(0, cx);
} // main

//////////////////////////////////////////////////////////////////////
// eof pl0.c
