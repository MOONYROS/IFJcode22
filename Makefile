EXEC := gigachad_compiler

#Zde pridavejte zdrojaky chlapi
SRCS := main.c lex.c parser.c support.c expression.c tstack.c symtable.c token.c
OBJS := main.o lex.o parser.o support.o expression.o tstack.o symtable.o token.o

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -c

.PHONY: all run clean

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $^ -o $@

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $^

run: $(EXEC)
	./$(EXEC)
	
clean:
	rm -vf *.o
	rm -f gigachad_compiler
