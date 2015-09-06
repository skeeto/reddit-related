CFLAGS = -std=c99 -Wall -Wextra -g3 -O3

related : trie.o related.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

trie.o : trie.c
related.o : related.c

clean :
	$(RM) related trie.o related.o
