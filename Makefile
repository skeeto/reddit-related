CFLAGS = -std=c99 -Wall -Wextra -g3 -O3

reddit : trie.o related.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

trie.o : trie.c
related.o : related.c

clean :
	$(RM) reddit trie.o related.o
