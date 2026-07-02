#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEVEL 2
#define MAXLEN 100

char *whitespace = " \t\n";
char *level1ops = "+-";
char *level2ops = "*/%";
char *ops = "+-*/%()";

int postpolish(char *s, int level);
int gettoken(char **s, char **ps, char **pe);

int test_gettoken();

int main(int argc, char *argv[]) {
    test_gettoken();
    return 0;
}

int test_gettoken() {
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

int gettoken(char **psc, char **ps, char **pe) {
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
}

int postpolish(char *s, int level) {
    char **pc = &s;
    int tokentype;
    char **ps, **pe;
    while ((tokentype = gettoken(&s, ps, pe)) != 0) {
        switch (tokentype) {
            ;
        }
    }
    return 0;
}

