//
//  mini!.c
//
//
//  Created by 戴維恩 on 2020/4/28.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define TBLSIZE 64
#define PRINTERR 1
#define MAXLEN 256
#define error(errorNum) { \
    printf("EXIT 1\n"); \
    err_self(errorNum); \
}
typedef enum {
    UNDEFINED, MISPAREN, NOTNUMID, NOTFOUND, RUNOUT, NOTLVAL, DIVZERO, SYNTAXERR
} ErrorType;
typedef struct {
    int val;
    char name[MAXLEN];
} Symbol;
typedef enum {
    UNKNOWN, END, ENDFILE,
    INT, ID,
    ADDSUB, MULDIV, LOGICAL,
    INCDEC, ASSIGN,
    LPAREN, RPAREN
} TokenSet;
typedef struct _Node {
    TokenSet data;
    int val;
    char lexeme[MAXLEN];
    struct _Node *left;
    struct _Node *right;
} BTNode;
Symbol table[TBLSIZE];
void initTable(void);
int getval(char *str);
int setval(char *str, int val);
BTNode *makeNode(TokenSet tok, const char *lexe);
void freeTree(BTNode *root);
BTNode *factor(void);
BTNode *term(void);
BTNode *term_tail(BTNode *left);
BTNode *expr(void);
BTNode *expr_tail(BTNode *left);
void statement(void);
void err_self(ErrorType errorNum);
int match(TokenSet token);
void advance(void);
char *getLexeme(void);
TokenSet getToken(void);
TokenSet curToken = UNKNOWN;
int evaluateTree(BTNode *root);
void printPrefix(BTNode *root);
char lexeme[MAXLEN];
int rflag;
int sbcount = 0;


int main() {
    initTable();
    while (1) {
        statement();
    }
    return 0;
}

void initTable(void) {
    strcpy(table[0].name, "x");
    table[0].val = 0;
    strcpy(table[1].name, "y");
    table[1].val = 0;
    strcpy(table[2].name, "z");
    table[2].val = 0;
    sbcount = 3;
}
int getval(char *str) {
    int i = 0;

    for (i = 0; i < sbcount; i++)
        if (strcmp(str, table[i].name) == 0){
            printf("MOV r%d [%d]\n", rflag, i*4); rflag++;
            return table[i].val;
        }
    error(NOTFOUND);
    return 0;
}

int setval(char *str, int val) {
    int i = 0;

    for (i = 0; i < sbcount; i++) {
        if (strcmp(str, table[i].name) == 0) {
            table[i].val = val;
            printf("MOV [%d] r%d\n", i*4, rflag-1);
            return val;
        }
    }

    if (sbcount >= TBLSIZE)
        error(RUNOUT);
    
    strcpy(table[sbcount].name, str);
    table[sbcount].val = val;
    printf("MOV [%d] r%d\n", sbcount*4, rflag-1);
    sbcount++;
    return val;
}

BTNode *makeNode(TokenSet tok, const char *lexe) {
    BTNode *node = (BTNode*)malloc(sizeof(BTNode));
    strcpy(node->lexeme, lexe);
    node->data = tok;
    node->val = 0;
    node->left = NULL;
    node->right = NULL;
    return node;
}

void freeTree(BTNode *root) {
    if (root != NULL) {
        freeTree(root->left);
        freeTree(root->right);
        free(root);
    }
}

// factor := INT | ADDSUB INT |
//                ID  | ADDSUB ID  |
//                ID ASSIGN expr |
//                LPAREN expr RPAREN |
//                ADDSUB LPAREN expr RPAREN | INCDEC ID
BTNode *factor(void) {
    BTNode *retp = NULL, *left = NULL;

    if (match(INT)) {
        retp = makeNode(INT, getLexeme());
        advance();
    } else if (match(ID)) {
        left = makeNode(ID, getLexeme());
        advance();
        if (!match(ASSIGN)) {
            retp = left;
        } else {
            retp = makeNode(ASSIGN, getLexeme());
            advance();
            retp->left = left;
            retp->right = expr();
        }
    } else if (match(ADDSUB)) {
        retp = makeNode(ADDSUB, getLexeme());
        retp->left = makeNode(INT, "0");
        advance();
        if (match(INT)) {
            retp->right = makeNode(INT, getLexeme());
            advance();
        } else if (match(ID)) {
            retp->right = makeNode(ID, getLexeme());
            advance();
        } else if (match(LPAREN)) {
            advance();
            retp->right = expr();
            if (match(RPAREN))
                advance();
            else
                error(MISPAREN);
        } else {
            error(NOTNUMID);
        }
    } else if (match(LPAREN)) {
        advance();
        retp = expr();
        if (match(RPAREN))
            advance();
        else
            error(MISPAREN);
    } else if (match(INCDEC)) {
        retp = makeNode(INCDEC, getLexeme());
        advance();
        if (match(ID)) {
            retp->left = makeNode(ID, getLexeme());
            retp->right = makeNode(INT, "1");
            advance();
        } else error(SYNTAXERR);
    } else {
        error(NOTNUMID);
    }
    return retp;
}

// term := factor term_tail
BTNode *term(void) {
    BTNode *node = factor();
    return term_tail(node);
}

// term_tail := MULDIV factor term_tail | NiL
BTNode *term_tail(BTNode *left) {
    BTNode *node = NULL;

    if (match(MULDIV)) {
        node = makeNode(MULDIV, getLexeme());
        advance();
        node->left = left;
        node->right = factor();
        return term_tail(node);
    } else {
        return left;
    }
}

// expr := term expr_tail
BTNode *expr(void) {
    BTNode *node = term();
    return expr_tail(node);
}

// expr_tail := ADDSUB term expr_tail | LOGICAL term expr_tail |NiL
BTNode *expr_tail(BTNode *left) {
    BTNode *node = NULL;

    if (match(ADDSUB)) {
        node = makeNode(ADDSUB, getLexeme());
        advance();
        node->left = left;
        node->right = term();
        return expr_tail(node);
    }
    else if(match(LOGICAL)){
        node = makeNode(LOGICAL, getLexeme());
        advance();
        node->left = left;
        node->right = term();
        return expr_tail(node);
    }
    else {
        return left;
    }
}

// statement := ENDFILE | END | expr END
void statement(void) {
    BTNode *retp = NULL;

    if (match(ENDFILE)) {
        printf("MOV r0 [0]\nMOV r1 [4]\nMOV r2 [8]\n");
        printf("EXIT 0\n");
        exit(0);
    } else if (match(END)) {
        advance();
    } else {
        retp = expr();
        if (match(END)) {
            evaluateTree(retp);
            rflag = 0;
            printPrefix(retp);
            freeTree(retp);
            advance();
        } else {
            error(SYNTAXERR);
        }
    }
}

void err_self(ErrorType errorNum) {
    exit(0);
}
TokenSet getToken(void) {
    int i = 0;
    char c = '\0';

    while ((c = fgetc(stdin)) == ' ' || c == '\t');

    if (isdigit(c)) {
        lexeme[0] = c;
        c = fgetc(stdin);
        i = 1;
        while (isdigit(c) && i < MAXLEN) {
            lexeme[i] = c;
            ++i;
            c = fgetc(stdin);
        }
        ungetc(c, stdin);
        lexeme[i] = '\0';
        return INT;
    } else if (c == '+' || c == '-') {
        lexeme[0] = c;
        c = fgetc(stdin);
        if (c == lexeme[0]) {
            lexeme[1] = c;
            lexeme[2] = '\0';
            char oc = fgetc(stdin);
            if(oc == lexeme[0]) {
                ungetc(oc, stdin);
                ungetc(c, stdin);
                lexeme[1] = '\0';
                return ADDSUB;
            }
            else{
                ungetc(oc, stdin);
            }
            return INCDEC;
        } else {
            ungetc(c, stdin);
            lexeme[1] = '\0';
            return ADDSUB;
        }
    } else if (c == '&' || c == '|' || c == '^') {
        lexeme[0] = c;
        lexeme[1] = '\0';
        return LOGICAL;
    } else if (c == '*' || c == '/') {
        lexeme[0] = c;
        lexeme[1] = '\0';
        return MULDIV;
    } else if (c == '\n') {
        lexeme[0] = '\0';
        return END;
    } else if (c == '=') {
        strcpy(lexeme, "=");
        return ASSIGN;
    } else if (c == '(') {
        strcpy(lexeme, "(");
        return LPAREN;
    } else if (c == ')') {
        strcpy(lexeme, ")");
        return RPAREN;
    } else if (isalpha(c) || c == '_') {
        lexeme[0] = c;
        c = fgetc(stdin);
        i = 1;
        while (isalpha(c) || isdigit(c) || c == '_') {
            lexeme[i] = c;
            ++i;
            c = fgetc(stdin);
        }
        ungetc(c, stdin);
        lexeme[i] = '\0';
        return ID;
    } else if (c == EOF) {
        return ENDFILE;
    } else {
        return UNKNOWN;
    }
}

void advance(void) {
    curToken = getToken();
}

int match(TokenSet token) {
    if (curToken == UNKNOWN)
        advance();
    return token == curToken;
}

char *getLexeme(void) {
    return lexeme;
}
int evaluateTree(BTNode *root) {
    int retval = 0, lv = 0, rv = 0;
    if (root != NULL) {
        switch (root->data) {
            case ID:
                retval = getval(root->lexeme);
                break;
            case INT:
                retval = atoi(root->lexeme);
                printf("MOV r%d %d\n", rflag, retval);
                rflag++;
                break;
            case ASSIGN:
                rv = evaluateTree(root->right);
                retval = setval(root->left->lexeme, rv);
                break;
            case ADDSUB:
            case INCDEC:
            case MULDIV:
            case LOGICAL:
                lv = evaluateTree(root->left);
                rv = evaluateTree(root->right);
                
                if(strcmp(root->lexeme, "++") == 0){
                    retval = lv + rv;
                    printf("ADD r%d r%d\n", rflag-2, rflag-1); rflag--;
                    setval(root->left->lexeme, retval);
                } else if(strcmp(root->lexeme, "--") == 0){
                    retval = lv - rv;
                    printf("SUB r%d r%d\n", rflag-2, rflag-1); rflag--;
                    setval(root->left->lexeme, retval);
                } else if (strcmp(root->lexeme, "+") == 0) {
                    printf("ADD r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv + rv;
                } else if (strcmp(root->lexeme, "-") == 0) {
                    printf("SUB r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv - rv;
                } else if (strcmp(root->lexeme, "*") == 0) {
                    printf("MUL r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv * rv;
                } else if (strcmp(root->lexeme, "/") == 0) {
                    if (rv == 0){
                        error(DIVZERO);
                    }
                    printf("DIV r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv / rv;
                } else if (strcmp(root->lexeme, "&") == 0) {
                    printf("AND r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv & rv;
                } else if (strcmp(root->lexeme, "|") == 0) {
                    printf("OR r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv | rv;
                } else if (strcmp(root->lexeme, "^") == 0) {
                    printf("XOR r%d r%d\n", rflag-2, rflag-1); rflag--;
                    retval = lv ^ rv;
                }
                break;
            default:
                retval = 0;
        }
    }
    return retval;
}

void printPrefix(BTNode *root) {
}
