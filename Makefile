EXEC := gigachad_compiler
RUN_FILE := vzor_01.php
CODE_FILE := temp.ifjcode
INTERPRETER := ic22int

#Zde pridavejte zdrojaky chlapi
SRCS := main.c lex.c parser.c support.c expression.c tstack.c symtable.c token.c generator.c expStack.c
OBJS := main.o lex.o parser.o support.o expression.o tstack.o symtable.o token.o generator.o expStack.o

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -c

.PHONY: all run clean

all: $(EXEC)

# MOONY: prozatim jsem smazal presmerovani ze stdin, pozdeji bude potreba 
$(EXEC): $(OBJS)
	$(CC) $^ -o $@
	
run: $(EXEC)
	./$(EXEC) < $(RUN_FILE) > $(CODE_FILE)
	./$(INTERPRETER) $(CODE_FILE)
	
clean:
	rm -vf *.o
	rm -f gigachad_compiler
