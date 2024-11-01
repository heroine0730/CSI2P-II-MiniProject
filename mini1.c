#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
For the language grammar, please refer to Grammar section on the github page:
  https://github.com/lightbulb12294/CSI2P-II-Mini1#grammar
*/

#define MAX_LENGTH 200
typedef enum {
	ASSIGN, ADD, SUB, MUL, DIV, REM, PREINC, PREDEC, POSTINC, POSTDEC, IDENTIFIER, CONSTANT, LPAR, RPAR, PLUS, MINUS, END
} Kind;
typedef enum {
	STMT, EXPR, ASSIGN_EXPR, ADD_EXPR, MUL_EXPR, UNARY_EXPR, POSTFIX_EXPR, PRI_EXPR
} GrammarState;
typedef struct TokenUnit {
	Kind kind;
	int val; // record the integer value or variable name
	struct TokenUnit *next;
} Token;
typedef struct ASTUnit {
	Kind kind;
	int val; // record the integer value or variable name
	struct ASTUnit *lhs, *mid, *rhs;
} AST;

/// utility interfaces

// err marco should be used when a expression error occurs.
#define err(x) {\
	puts("Compile Error!");\
	if(DEBUG) {\
		fprintf(stderr, "Error at line: %d\n", __LINE__);\
		fprintf(stderr, "Error message: %s\n", x);\
	}\
	exit(0);\
}
// You may set DEBUG=1 to debug. Remember setting back to 0 before submit.
#define DEBUG 0
// Split the input char array into token linked list.
Token *lexer(const char *in);
// Create a new token.
Token *new_token(Kind kind, int val);
// Translate a token linked list into array, return its length.
size_t token_list_to_arr(Token **head);
// Parse the token array. Return the constructed AST.
AST *parser(Token *arr, size_t len);
// Parse the token array. Return the constructed AST.
AST *parse(Token *arr, int l, int r, GrammarState S);
// Create a new AST node.
AST *new_AST(Kind kind, int val);
// Find the location of next token that fits the condition(cond). Return -1 if not found. Search direction from start to end.
int findNextSection(Token *arr, int start, int end, int (*cond)(Kind));
// Return 1 if kind is ASSIGN.
int condASSIGN(Kind kind);
// Return 1 if kind is ADD or SUB.
int condADD(Kind kind);
// Return 1 if kind is MUL, DIV, or REM.
int condMUL(Kind kind);
// Return 1 if kind is RPAR.
int condRPAR(Kind kind);
// Check if the AST is semantically right. This function will call err() automatically if check failed.
void semantic_check(AST *now);

// Generate ASM code.
int codegen(AST *root, int DI);
int regUsed[256]; 
int xc, yc, zc;
int regUsedMax;

// Free the whole AST.
void freeAST(AST *now);

/// debug interfaces

// Print token array.
void token_print(Token *in, size_t len);
// Print AST tree.
void AST_print(AST *head);

char input[MAX_LENGTH];

int main() {
	for(int i=0; i<256; i++){
		regUsed[i] = 0;
	}
	xc = 0;
	yc = 0;
	zc = 0;
	while (fgets(input, MAX_LENGTH, stdin) != NULL) {
		regUsedMax = 4;

		Token *content = lexer(input);
		size_t len = token_list_to_arr(&content);
		if (len == 0) continue;

		AST *ast_root = parser(content, len);
		semantic_check(ast_root);
		// AST_print(ast_root);

		codegen(ast_root, 0);
		free(content);
		freeAST(ast_root);
		// clear regUsed[4]~
		for(int i=4; i<=regUsedMax; i++) regUsed[i] = 0;
	}
	// store the varible values to mem in the end
	if(xc) printf("store [0] r1\n");
	if(yc) printf("store [4] r2\n");
	if(zc) printf("store [8] r3\n");
	return 0;
}

Token *lexer(const char *in) {
	Token *head = NULL;
	Token **now = &head;
	for (int i = 0; in[i]; i++) {
		if (isspace(in[i])) // ignore space characters
			continue;
		else if (isdigit(in[i])) {
			(*now) = new_token(CONSTANT, atoi(in + i));
			while (in[i+1] && isdigit(in[i+1])) i++;
		}
		else if ('x' <= in[i] && in[i] <= 'z') // variable
			(*now) = new_token(IDENTIFIER, in[i]);
		else switch (in[i]) {
			case '=':
				(*now) = new_token(ASSIGN, 0);
				break;
			case '+':
				if (in[i+1] && in[i+1] == '+') {
					i++;
					// In lexer scope, all "++" will be labeled as PREINC.
					(*now) = new_token(PREINC, 0);
				}
				// In lexer scope, all single "+" will be labeled as PLUS.
				else (*now) = new_token(PLUS, 0);
				break;
			case '-':
				if (in[i+1] && in[i+1] == '-') {
					i++;
					// In lexer scope, all "--" will be labeled as PREDEC.
					(*now) = new_token(PREDEC, 0);
				}
				// In lexer scope, all single "-" will be labeled as MINUS.
				else (*now) = new_token(MINUS, 0);
				break;
			case '*':
				(*now) = new_token(MUL, 0);
				break;
			case '/':
				(*now) = new_token(DIV, 0);
				break;
			case '%':
				(*now) = new_token(REM, 0);
				break;
			case '(':
				(*now) = new_token(LPAR, 0);
				break;
			case ')':
				(*now) = new_token(RPAR, 0);
				break;
			case ';':
				(*now) = new_token(END, 0);
				break;
			default:
				err("Unexpected character.");
		}
		now = &((*now)->next);
	}
	return head;
}

Token *new_token(Kind kind, int val) {
	Token *res = (Token*)malloc(sizeof(Token));
	res->kind = kind;
	res->val = val;
	res->next = NULL;
	return res;
}

size_t token_list_to_arr(Token **head) {
	size_t res;
	Token *now = (*head), *del;
	for (res = 0; now != NULL; res++) now = now->next;
	now = (*head);
	if (res != 0) (*head) = (Token*)malloc(sizeof(Token) * res);
	for (int i = 0; i < res; i++) {
		(*head)[i] = (*now);
		del = now;
		now = now->next;
		free(del);
	}
	return res;
}

AST *parser(Token *arr, size_t len) {
	for (int i = 1; i < len; i++) {
		// correctly identify "ADD" and "SUB"
		if (arr[i].kind == PLUS || arr[i].kind == MINUS) {
			switch (arr[i - 1].kind) {
				case PREINC:
				case PREDEC:
				case IDENTIFIER:
				case CONSTANT:
				case RPAR:
					arr[i].kind = arr[i].kind - PLUS + ADD;
				default: break;
			}
		}
	}
	// token_print(arr, len);
	return parse(arr, 0, len - 1, STMT);
}

AST *parse(Token *arr, int l, int r, GrammarState S) {
	// printf("Grammar state: %d, kind: %d\n", S, arr[l].kind);
	AST *now = NULL;
	if (l > r)
		err("Unexpected parsing range.");
	int nxt;
	switch (S) {
		case STMT:
			if (l == r && arr[l].kind == END)
				return NULL;
			else if (arr[r].kind == END)
				return parse(arr, l, r - 1, EXPR);
			else err("Expected \';\' at the end of line.");
		case EXPR:
			return parse(arr, l, r, ASSIGN_EXPR);
		case ASSIGN_EXPR:
			if ((nxt = findNextSection(arr, l, r, condASSIGN)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, UNARY_EXPR);
				now->rhs = parse(arr, nxt + 1, r, ASSIGN_EXPR);
				return now;
			}
			return parse(arr, l, r, ADD_EXPR);
		case ADD_EXPR:
			if((nxt = findNextSection(arr, r, l, condADD)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, ADD_EXPR);
				now->rhs = parse(arr, nxt + 1, r, MUL_EXPR);
				return now;
			}
			return parse(arr, l, r, MUL_EXPR);
		case MUL_EXPR:
			// TODO: Implement MUL_EXPR.
			// hint: Take ADD_EXPR as reference.
			if((nxt = findNextSection(arr, r, l, condMUL)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, MUL_EXPR);
				now->rhs = parse(arr, nxt + 1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, UNARY_EXPR);
		case UNARY_EXPR:
			// TODO: Implement UNARY_EXPR.
			// hint: Take POSTFIX_EXPR as reference.
			if ((arr[l].kind == PREINC) || (arr[l].kind == PREDEC) || (arr[l].kind == PLUS) || (arr[l].kind == MINUS)) {
				now = new_AST(arr[l].kind, 0);
				now->mid = parse(arr, l+1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, POSTFIX_EXPR);
		case POSTFIX_EXPR:
			if (arr[r].kind == PREINC || arr[r].kind == PREDEC) {
				// translate "PREINC", "PREDEC" into "POSTINC", "POSTDEC"
				now = new_AST(arr[r].kind - PREINC + POSTINC, 0);
				now->mid = parse(arr, l, r - 1, POSTFIX_EXPR);
				return now;
			}
			return parse(arr, l, r, PRI_EXPR);
		case PRI_EXPR:
			if (findNextSection(arr, l, r, condRPAR) == r) {
				now = new_AST(LPAR, 0);
				now->mid = parse(arr, l + 1, r - 1, EXPR);
				return now;
			}
			if (l == r) {
				if (arr[l].kind == IDENTIFIER || arr[l].kind == CONSTANT)
					return new_AST(arr[l].kind, arr[l].val);
				err("Unexpected token during parsing.");
			}
			err("No token left for parsing.");
		default:
			err("Unexpected grammar state.");
	}
	AST_print(now);
}

AST *new_AST(Kind kind, int val) {
	AST *res = (AST*)malloc(sizeof(AST));
	res->kind = kind;
	res->val = val;
	res->lhs = res->mid = res->rhs = NULL;
	return res;
}

int findNextSection(Token *arr, int start, int end, int (*cond)(Kind)) {
	int par = 0;
	int d = (start < end) ? 1 : -1;
	for (int i = start; (start < end) ? (i <= end) : (i >= end); i += d) {
		if (arr[i].kind == LPAR) par++;
		if (arr[i].kind == RPAR) par--;
		if (par == 0 && cond(arr[i].kind) == 1) return i;
	}
	return -1;
}

int condASSIGN(Kind kind) {
	return kind == ASSIGN;
}

int condADD(Kind kind) {
	return kind == ADD || kind == SUB;
}

int condMUL(Kind kind) {
	return kind == MUL || kind == DIV || kind == REM;
}

int condRPAR(Kind kind) {
	return kind == RPAR;
}

void semantic_check(AST *now) {
	if (now == NULL) return;
	// Left operand of '=' must be an identifier or identifier with one or more parentheses.
	if (now->kind == ASSIGN) {
		AST *tmp = now->lhs;
		while (tmp->kind == LPAR) tmp = tmp->mid;
		if (tmp->kind != IDENTIFIER){
			err("Lvalue is required as left operand of assignment.");
		}else{
			// 直接把去除括號的id放在now的左子樹下
			now->lhs = tmp;
		}
	}
	// Operand of INC/DEC must be an identifier or identifier with one or more parentheses.
	// TODO: Implement the remaining semantic_check code.
	// hint: Follow the instruction above and ASSIGN-part code to implement.
	// hint: Semantic of each node needs to be checked recursively (from the current node to lhs/mid/rhs node).
	if ((now->kind == PREINC)||(now->kind == POSTINC)||(now->kind == PREDEC)||(now->kind == POSTDEC)) {
		AST *tmp = now->mid;
		while (tmp->kind == LPAR) tmp = tmp->mid;
		if (tmp->kind != IDENTIFIER){
			err("Operand of INC/DEC must be an identifier.");
		} else {
			// 直接把去除括號的id放在now的中子樹下
			now->mid = tmp;
		}
	}
	semantic_check(now->lhs);
	semantic_check(now->mid);
	semantic_check(now->rhs);
}

// Goals:
// 1. go through the AST
// 2. print out the assembly code
// 3. return the register index that stores the return value
// Store 	[x] in r1
//			[y] in r2
//			[z] in r3
// Description for Identifier:
// 在assign之前不能動到Identifier的值
// 0 -> not identifier
// 1 -> 'l'= ''
// 2 -> '' = 'r'
int codegen(AST *root, int DI) {
	// TODO: Implement your codegen in your own way.
	// You may modify the function parameter or the return type, even the whole structure as you wish.
	if (root == NULL) return 0;
	int l_result, r_result, m_result;
	int i;
	// v ASSIGN, 
	// v ADD, SUB, MUL, DIV, REM, 
	// v PREINC, PREDEC, 
	// v POSTINC, POSTDEC, 
	// v IDENTIFIER, CONSTANT, 
	// v LPAR, RPAR, 
	// v PLUS, MINUS, 
	// END(no this case)
	switch (root->kind) {
		case ASSIGN:
			// l_result: 	directly find the identifier register
			// r_result: 	the register returned
			// return:		the identifier register
			r_result = codegen(root->rhs, 2);
			l_result = codegen(root->lhs, 1);
			printf("add r%d 0 r%d\n", l_result, r_result);
			if(r_result>3) regUsed[r_result] = 0;
			else regUsed[l_result] = 1;

			if(l_result == 1) xc=1;
			if(l_result == 2) yc = 1;
			if(l_result == 3) zc = 1;
			return l_result;
		break;
		case ADD:
			// l_result: 	the register returned
			// r_result:	the register returned
			// return:		the left register to save the usage of the registers,
			//				and make the right register available.
			l_result = codegen(root->lhs, DI);
			r_result = codegen(root->rhs, DI);
			if((l_result<=3)||(r_result<=3)){
				printf("add r%d r%d r%d\n", regUsedMax, l_result, r_result);
				regUsedMax++;
				return regUsedMax-1;
			}
			else{
				printf("add r%d r%d r%d\n", l_result, l_result, r_result);
				if(r_result>3) regUsed[r_result] = 0;
				return l_result;
			}
		break;
		case SUB:
			// similar to case ADD
			l_result = codegen(root->lhs, DI);
			r_result = codegen(root->rhs, DI);
			if((l_result<=3)||(r_result<=3)){
				printf("sub r%d r%d r%d\n", regUsedMax, l_result, r_result);
				regUsedMax++;
				return regUsedMax-1;
			}
			else{
				printf("sub r%d r%d r%d\n", l_result, l_result, r_result);
				if(r_result>3) regUsed[r_result] = 0;
				return l_result;
			}
		break;
		case MUL:
			// similar to case ADD
			l_result = codegen(root->lhs, DI);
			r_result = codegen(root->rhs, DI);
			if((l_result<=3)||(r_result<=3)){
				printf("mul r%d r%d r%d\n", regUsedMax, l_result, r_result);
				regUsedMax++;
				return regUsedMax-1;
			}
			else{
				printf("mul r%d r%d r%d\n", l_result, l_result, r_result);
				if(r_result>3) regUsed[r_result] = 0;
				return l_result;
			}
		break;
		case DIV:
			// similar to case ADD
			l_result = codegen(root->lhs, DI);
			r_result = codegen(root->rhs, DI);
			if((l_result<=3)||(r_result<=3)){
				printf("div r%d r%d r%d\n", regUsedMax, l_result, r_result);
				regUsedMax++;
				return regUsedMax-1;
			}
			else{
				printf("div r%d r%d r%d\n", l_result, l_result, r_result);
				if(r_result>3) regUsed[r_result] = 0;
				return l_result;
			}
		break;
		case REM:
			// similar to case ADD
			l_result = codegen(root->lhs, DI);
			r_result = codegen(root->rhs, DI);
			if((l_result<=3)||(r_result<=3)){
				printf("rem r%d r%d r%d\n", regUsedMax, l_result, r_result);
				regUsedMax++;
				return regUsedMax-1;
			}
			else{
				printf("rem r%d r%d r%d\n", l_result, l_result, r_result);
				if(r_result>3) regUsed[r_result] = 0;
				return l_result;
			}
		break;
		case PREINC:
			// m_result:	the identifier returned
			// return:		the identifier
			m_result = codegen(root->mid, DI);
			printf("add r%d r%d 1\n", m_result, m_result);
			if(m_result == 1) xc = 1;
			if(m_result == 2) yc = 1;
			if(m_result == 3) zc = 1;
			return m_result;
		break;
		case PREDEC:
			// similar to PREINC
			m_result = codegen(root->mid, DI);
			printf("sub r%d r%d 1\n", m_result, m_result);
			if(m_result == 1) xc = 1;
			if(m_result == 2) yc = 1;
			if(m_result == 3) zc = 1;
			return m_result;
		break;
		case POSTINC:
			// increase after the expression
			m_result = codegen(root->mid, DI);
			printf("add r%d r%d 0\n", regUsedMax, m_result);
			regUsed[regUsedMax] = 1;
			regUsedMax++;
			printf("add r%d r%d 1\n", m_result, m_result);
			if(m_result == 1) xc = 1;
			if(m_result == 2) yc = 1;
			if(m_result == 3) zc = 1;
			return regUsedMax-1;
		break;
		case POSTDEC:
			// similar to POSTINC
			m_result = codegen(root->mid, DI);
			printf("add r%d r%d 0\n", regUsedMax, m_result);
			regUsed[regUsedMax] = 1;
			regUsedMax++;
			printf("sub r%d r%d 1\n", m_result, m_result);
			if(m_result == 1) xc = 1;
			if(m_result == 2) yc = 1;
			if(m_result == 3) zc = 1;
			return regUsedMax-1;
		break;
		case IDENTIFIER:
			if(DI == 1){ // left
				// nothing to do
				if((char)((root->val)) == 'x') return 1;
				else if((char)((root->val)) == 'y') return 2;
				else if((char)((root->val)) == 'z') return 3;
			}
			// load the identifier from memory to reg
			if((char)((root->val)) == 'x'){
				if((xc==1)||(regUsed[1]==1)) return 1;
				printf("load r1 [0]\n");
				regUsed[1] = 1;
				return 1;
			}else if((char)((root->val)) == 'y'){
				if((yc==1)||(regUsed[2]==1)) return 2;
				printf("load r2 [4]\n");
				regUsed[2] = 1;
				return 2;
			}else{
				if((zc==1)||(regUsed[3]==1)) return 3;
				printf("load r3 [8]\n");
				regUsed[3] = 1;
				return 3;
			}
		break;
		case CONSTANT:
			i = 4;
			while(regUsed[i] == 1) i++;
			printf("add r%d 0 %d\n", i, root->val);
			regUsed[i] = 1;
			return i;
		break;
		case LPAR:	// won't appear RPAR
			return codegen(root->mid, DI);
		break;
		case PLUS:
			return codegen(root->mid, DI);
		break;
		case MINUS:
			i = codegen(root->mid, DI);
			printf("sub r%d 0 r%d\n", regUsedMax, i);
			regUsed[regUsedMax] = 1;
			regUsedMax++;
			return regUsedMax-1;
		break;
		default:
			err("Undefined codegen.");
		break;
	}

}

void freeAST(AST *now) {
	if (now == NULL) return;
	freeAST(now->lhs);
	freeAST(now->mid);
	freeAST(now->rhs);
	free(now);
}

void token_print(Token *in, size_t len) {
	const static char KindName[][20] = {
		"Assign", "Add", "Sub", "Mul", "Div", "Rem", "Inc", "Dec", "Inc", "Dec", "Identifier", "Constant", "LPar", "RPar", "Plus", "Minus", "End"
	};
	const static char KindSymbol[][20] = {
		"'='", "'+'", "'-'", "'*'", "'/'", "'%'", "\"++\"", "\"--\"", "\"++\"", "\"--\"", "", "", "'('", "')'", "'+'", "'-'"
	};
	const static char format_str[] = "<Index = %3d>: %-10s, %-6s = %s\n";
	const static char format_int[] = "<Index = %3d>: %-10s, %-6s = %d\n";
	for(int i = 0; i < len; i++) {
		switch(in[i].kind) {
			case LPAR:
			case RPAR:
			case PREINC:
			case PREDEC:
			case ADD:
			case SUB:
			case MUL:
			case DIV:
			case REM:
			case ASSIGN:
			case PLUS:
			case MINUS:
				fprintf(stderr, format_str, i, KindName[in[i].kind], "symbol", KindSymbol[in[i].kind]);
				break;
			case CONSTANT:
				fprintf(stderr, format_int, i, KindName[in[i].kind], "value", in[i].val);
				break;
			case IDENTIFIER:
				fprintf(stderr, format_str, i, KindName[in[i].kind], "name", (char*)(&(in[i].val)));
				break;
			case END:
				fprintf(stderr, "<Index = %3d>: %-10s\n", i, KindName[in[i].kind]);
				break;
			default:
				fputs("=== unknown token ===", stderr);
		}
	}
}

void AST_print(AST *head) {
	static char indent_str[MAX_LENGTH] = "  ";
	static int indent = 2;
	const static char KindName[][20] = {
		"Assign", "Add", "Sub", "Mul", "Div", "Rem", "PreInc", "PreDec", "PostInc", "PostDec", "Identifier", "Constant", "Parentheses", "Parentheses", "Plus", "Minus"
	};
	const static char format[] = "%s\n";
	const static char format_str[] = "%s, <%s = %s>\n";
	const static char format_val[] = "%s, <%s = %d>\n";
	if (head == NULL) return;
	char *indent_now = indent_str + indent;
	indent_str[indent - 1] = '-';
	fprintf(stderr, "%s", indent_str);
	indent_str[indent - 1] = ' ';
	if (indent_str[indent - 2] == '`')
		indent_str[indent - 2] = ' ';
	switch (head->kind) {
		case ASSIGN:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case REM:
		case PREINC:
		case PREDEC:
		case POSTINC:
		case POSTDEC:
		case LPAR:
		case RPAR:
		case PLUS:
		case MINUS:
			fprintf(stderr, format, KindName[head->kind]);
			break;
		case IDENTIFIER:
			fprintf(stderr, format_str, KindName[head->kind], "name", (char*)&(head->val));
			break;
		case CONSTANT:
			fprintf(stderr, format_val, KindName[head->kind], "value", head->val);
			break;
		default:
			fputs("=== unknown AST type ===", stderr);
	}
	indent += 2;
	strcpy(indent_now, "| ");
	AST_print(head->lhs);
	strcpy(indent_now, "` ");
	AST_print(head->mid);
	AST_print(head->rhs);
	indent -= 2;
	(*indent_now) = '\0';
}
