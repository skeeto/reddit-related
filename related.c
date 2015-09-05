#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include "trie.h"

#define MAX_LENGTH 32

#define countof(a) (sizeof(a)/sizeof(0[a]))

static char *
decode_string(char **s)
{
    char *p = *s;
    char *key;
    if (*p == '"') {
        key = ++p;
        for (; *p != '"'; p++)
            if (*p == '\\')
                p++;
        *p = 0;
        *s = p + 2;
    } else {
        key = p;
        while (*p != ',' && *p != '}')
            p++;
        *p = 0;
        *s = p + 1;
        if (strcmp(key, "null") == 0)
            return NULL;
    }
    return key;
}

const char *ignore[] = {
    "[deleted]",
    "AutoModerator"
};

static void *
increment(const char *key, void *current, void *arg)
{
    (void) key;
    return (void *)((uintptr_t)current + (uintptr_t)arg);
}

typedef struct {
    int worst;
    size_t size;
    size_t count;
    struct {
        int count;
        char name[MAX_LENGTH];
    } entry[];
} table;

static table *
table_create(size_t size)
{
    table *t = malloc(sizeof(*t) + sizeof(t->entry[0]) * size);
    t->worst = -1;
    t->size = size;
    t->count = 0;
    return t;
}

static int
table_cmp(const void *e0, const void *e1)
{
    return *(int *)e1 - *(int *)e0;
}

static void
table_sort(table *t)
{
    qsort(t->entry, t->count, sizeof(t->entry[0]), table_cmp);
}

static void
table_print(const table *t)
{
    for (size_t i = 0; i < t->count; i++)
        printf("%-24s %d\n", t->entry[i].name, t->entry[i].count);
}

static void
table_insert(table *t, const char *name, int count)
{
    if (t->count < t->size) {
        size_t i = t->count++;
        t->entry[i].count = count;
        strcpy(t->entry[i].name, name);
        if (count > t->worst)
            t->worst = count;
    } else if (count > t->worst) {
        table_sort(t);
        size_t last = t->size - 1;
        t->entry[last].count = count;
        strcpy(t->entry[last].name, name);
        table_sort(t);
        t->worst = t->entry[last].count;
    }
}

static int
transfer_slash(const char *key, void *data, void *arg)
{
    int count = (uintptr_t)data;
    table *table = arg;
    const char *author = key;
    while (*author != '/')
        author++;
    table_insert(table, author + 1, count);
    return 0;
}


static int
transfer_simple(const char *key, void *data, void *arg)
{
    int count = (uintptr_t)data;
    table *table = arg;
    table_insert(table, key, count);
    return 0;
}

static table *
prefix_top(trie_t *t, const char *prefix, int limit)
{
    char actual_prefix[MAX_LENGTH];
    sprintf(actual_prefix, "%s/", prefix);
    table *top = table_create(limit);
    trie_visit(t, actual_prefix, transfer_slash, top);
    table_sort(top);
    return top;
}

static int
transfer_trie(const char *key, void *data, void *arg)
{
    (void) data;
    trie_t *tmp = arg;
    const char *subreddit = key;
    while (*subreddit != '/')
        subreddit++;
    trie_replace(tmp, subreddit, increment, data);
    return 0;
}

static table *
related_table(trie_t *t, const table *names, int limit)
{
    trie_t *tmp = trie_create();
    for (size_t i = 0; i < names->count; i++) {
        const char *name = names->entry[i].name;
        char prefix[MAX_LENGTH];
        sprintf(prefix, "%s/", name);
        trie_visit(t, prefix, transfer_trie, tmp);
    }
    table *top = prefix_top(tmp, "", limit);
    trie_free(tmp);
    return top;
}

enum format {
    FORMAT_COLUMNS, FORMAT_CSV
};

static void
print_stats(trie_t *subreddit_author,
            trie_t *author_subreddit,
            const char *subreddit,
            enum format format,
            int divisor,
            int limit)
{
    char with_slash[MAX_LENGTH];
    sprintf(with_slash, "%s/", subreddit);
    size_t nauthors = trie_count(subreddit_author, with_slash);
    nauthors = (nauthors + divisor - 1) / divisor;
    table *top_authors = prefix_top(subreddit_author, subreddit, nauthors);
    table *related = related_table(author_subreddit, top_authors, limit);
    switch (format) {
        case FORMAT_COLUMNS:
            printf("%s (%zu)\n", subreddit, related->count);
            table_print(related);
            printf("\n");
            break;
        case FORMAT_CSV:
            printf("%s", subreddit);
            for (size_t i = 0; i < related->size; i++)
                printf(",%s",
                       i < related->count ? related->entry[i].name : "");
            printf("\n");
            break;
    }
    free(related);
    free(top_authors);
}

static void
load_data(trie_t *subreddit_author,
          trie_t *author_subreddit,
          trie_t *subreddits,
          FILE *in)
{
    size_t line_size = 32 * 1096 * 1096;
    char *line = malloc(line_size);
    long linenum = 0;
    while (!feof(in)) {
        if (!fgets(line, line_size, in))
            break;
        linenum++;
        char *p = line + 1;
        char *author = NULL;
        char *subreddit = NULL;
        do {
            assert(*p && *p != '\n');
            char *key = decode_string(&p);
            char *value = decode_string(&p);
            if (strcmp(key, "author") == 0)
                author = value;
            else if (strcmp(key, "subreddit") == 0)
                subreddit = value;
        } while (!author || !subreddit);
        bool keep = true;
        for (size_t i = 0; i < countof(ignore); i++)
            if (strcmp(author, ignore[i]) == 0) {
                keep = false;
                break;
            }
        if (keep) {
            trie_replace(subreddits, subreddit, increment, (void *)1);
            char sa_key[MAX_LENGTH * 2];
            char as_key[MAX_LENGTH * 2];
            sprintf(sa_key, "%s/%s", subreddit, author);
            sprintf(as_key, "%s/%s", author, subreddit);
            trie_replace(subreddit_author, sa_key, increment, (void *)1);
            trie_replace(author_subreddit, as_key, increment, (void *)1);
        }
    }
    free(line);
}

static table *
subreddits_by_count(trie_t *subreddits)
{
    size_t count = trie_count(subreddits, "");
    table *subreddit_table = table_create(count);
    trie_visit(subreddits, "", transfer_simple, subreddit_table);
    table_sort(subreddit_table);
    return subreddit_table;
}

int
main(int argc, char *argv[])
{
    enum format format = FORMAT_COLUMNS;
    bool verbose = false;
    bool process_all = false;
    int divisor = 100;
    int limit = 20;

    int option;
    while ((option = getopt(argc, argv, "ad:f:n:v")) != -1) {
        switch (option) {
            case 'a':
                process_all = true;
                break;
            case 'd':
                divisor = atoi(optarg);
                break;
            case 'f':
                if (strcmp(optarg, "csv") == 0)
                    format = FORMAT_CSV;
                else if (strcmp(optarg, "columns") == 0)
                    format = FORMAT_COLUMNS;
                else {
                    fprintf(stderr, "%s: unknown format, %s\n",
                            argv[0], optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'n':
                limit = atoi(optarg);
                break;
            case 'v':
                verbose = true;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    trie_t *subreddit_author = trie_create();
    trie_t *author_subreddit = trie_create();
    trie_t *subreddits = trie_create();

    load_data(subreddit_author, author_subreddit, subreddits, stdin);
    if (verbose) fprintf(stderr, "Finished loading data.\n");

    if (process_all) {
        table *subreddit_table = subreddits_by_count(subreddits);
        if (verbose)
            fprintf(stderr, "Processing %zu subreddits.\n",
                    subreddit_table->count);
        for (size_t i = 0; i < subreddit_table->count; i++) {
            char *sr = subreddit_table->entry[i].name;
            if (verbose)
                fprintf(stderr, "Processing %s ...\n", sr);
            print_stats(subreddit_author, author_subreddit,
                        sr, format, divisor, limit);
        }
        free(subreddit_table);
    } else {
        for (int i = optind; i < argc; i++) {
            char *sr = argv[i];
            print_stats(subreddit_author, author_subreddit,
                        sr, format, divisor, limit);
        }
    }

    trie_free(subreddits);
    trie_free(author_subreddit);
    trie_free(subreddit_author);
    return 0;
}
