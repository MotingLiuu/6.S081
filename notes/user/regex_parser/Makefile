CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g

OBJS = main.o lexer.o parser.o ast.o nfa.o

regex: $(OBJS)
	$(CC) $(CFLAGS) -o regex $(OBJS)

main.o: main.c lexer.h parser.h ast.h nfa.h
	$(CC) $(CFLAGS) -c main.c

lexer.o: lexer.c lexer.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c parser.h lexer.h ast.h
	$(CC) $(CFLAGS) -c parser.c

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

nfa.o: nfa.c nfa.h ast.h
	$(CC) $(CFLAGS) -c nfa.c

clean:
	rm -f regex $(OBJS)
