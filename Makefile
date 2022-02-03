# CFLAGS := -Wall -Werror -Wextra --std=c11
CFLAGS := -Wall -Wextra --std=c11
LIBS := -lGL -ldl -lglfw -lm
CC = gcc

.PHONY: selgl

.DEFAULT_GOAL = selgl

log:
	$(CC) log.c -c

selgl: log
	$(CC) src/glad.c main.c log.o -I include -o selgl $(CFLAGS) $(LIBS)

compiledb:
	compiledb make -Bnwk

clean:
	rm -vf *.o
	rm -vf **/*.o
	rm selgl
