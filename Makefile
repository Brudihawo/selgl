CFLAGS := -Wall -Werror -Wextra -pedantic --std=c11

selgl:
	$(CC) main.c -o selgl $(CFLAGS)
