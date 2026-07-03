#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEVEL 2
#define MAXLEN 100

char *whitespace = " \t\n";
char *level3ops = "+-";
char *level2ops = "*/%";
char *ops = "+-*/%()";

typedef struct token {
    char tokentype;
    char *ps;
    char *pe;
} token;

token tstack[MAXLEN];
int top = 0;
void push(char tokentype, char *ps, char *pe) {
    if (top >= MAXLEN) {
        printf("error: stack overflow\n");
        exit(1);
    }
    tstack[top].tokentype = tokentype;
    tstack[top].ps = ps;
    tstack[top].pe = pe;
    top++;
}
token pop() {
    if (top <= 0) {
        printf("error: stack underflow\n");
        exit(1);
    }
    top--;
    return tstack[top];
}


int parse(char **s, int level);
int gettoken(char **s, char **ps, char **pe);

int test_gettoken();
int test_parse();

int main(int argc, char *argv[]) {
    test_gettoken(); 
    test_parse();
    return 0;
}

int test_gettoken() {
    char *stack0 = "qwe";
    char tokentype0 = 'v';
    push(tokentype0, stack0, stack0 + strlen(stack0));
    char tokentype1 = '*';
    push(tokentype1, 0, 0);
    char tokentype2 = '+';
    push(tokentype2, 0, 0);
    char *test_str1 = "abc + def + (ghi * jkl) + mno";
    printf("test_str1: %s\n", test_str1);
    char **psc = &test_str1;
    char *ps, *pe;
    int tokentype;
    while ((tokentype = gettoken(psc, &ps, &pe)) != 0) {
        printf("tokentype: %c ", tokentype);
        if (tokentype == 'v') {
            if (!ps || !pe) {
                printf("error: ps or pe is null\n");
                return -1;
            }
            printf("token: ");
            while (ps < pe) {
                putchar(*ps);
                (ps)++;
            }
        }
        printf("\n");
    }
    return 0;
}

int test_parse() {
    char *test_str1 = "a * ((b - c) / (d + e))";
    printf("test_str1: %s\n", test_str1);
    parse(&test_str1, 3);
    return 0;
}

int gettoken(char **psc, char **ps, char **pe) {
    if (top != 0) {
        token t = pop();
        *ps = t.ps;
        *pe = t.pe;
        return t.tokentype;
    }
    while (**psc && strchr(whitespace, **psc))
        (*psc)++;
    if (**psc == '\0')
        return 0;
    switch (**psc) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '(':
        case ')':
            (*psc)++;
            return *(*psc - 1);
        default:
            *ps = *psc;
            while (**psc && !strchr(whitespace, **psc) && !strchr(ops, **psc))
                (*psc)++;
            *pe = *psc;
            return 'v';
    }
    return -1;
}

int parse(char **s, int level) {
    char *ps, *pe;
    int tokentype;
    
    switch (level) {
        case 1:
            tokentype = gettoken(s, &ps, &pe);
            if (tokentype == 'v') {
                if (!ps || !pe) {
                    printf("error: ps or pe is null\n");
                    return -1;
                }
                while (ps < pe) {
                    putchar(*ps);
                    (ps)++;
                }
                printf(" ");
            } else if (tokentype == '(') {
                if (parse(s, 3) < 0) return 1;
                tokentype = gettoken(s, &ps, &pe);
                if (tokentype != ')') {
                    printf("error: tokentype is not )\n");
                    return -1;
                }
            } else {
                printf("Format error: tokentype is not v or (\n");
                return -1;
            } 
            break;
        case 2:
            if (parse(s, 1) < 0) return -1;
            while (1) {
                tokentype = gettoken(s, &ps, &pe);
                if (tokentype != 0 && strchr(level2ops, tokentype)) {
                    int tmpop = tokentype;
                    if (parse(s, 1) < 0) return -1;
                    printf("%c ", tmpop);
                } else {
                    push(tokentype, ps, pe);
                    break;
                }
            }
            break;
        case 3:
            if (parse(s, 2) < 0) return -1;
            while (1) {
                tokentype = gettoken(s, &ps, &pe);
                if (tokentype != 0 && strchr(level3ops, tokentype)) {
                    int tmpop = tokentype;
                    if (parse(s, 2) < 0) return -1;
                    printf("%c ", tmpop);
                } else {
                    push(tokentype, ps, pe);
                    break;
                }
            }
            break;
        }
    return 0;
}


















