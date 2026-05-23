/* Thien Nguyen
   Login ID: nthien12@cs.unm.edu 
*/

#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

typedef struct {
    int valid; // Valid bit
    unsigned long long tag; // Tag bits
    int last_used; // LRU counter for eviction policy
} Line;

typedef struct {
    Line *lines; // Array of lines in the set
} Set;

typedef struct {
    Set  *sets; // Array of sets
    int s; // Number of set index bits
    int E; // Number of lines per set
    int b; // Number of block offset bits
    int S; // Number of sets (S = 2^s)
    char* trace_file; // Name of the valgrind trace to replay
} Cache;

/* This function creates the cache in memory and initializes every line as empty */
Cache createCache(int s, int E, int b) {
    Cache cache;
    // Save the cache settings
    cache.s = s;
    cache.E = E;
    cache.b = b;

    // Number of sets = 2^s
    cache.S = 1 << s; 

    //Make memory for all sets
    cache.sets = (Set *)malloc(cache.S * sizeof(Set)); // create An array of sets
    if (cache.sets == NULL) {
        fprintf(stderr, "malloc failed");
        exit(1);
    }
    // For each set, make memory for its lines
    for (int i = 0; i < cache.S; i++) {
        cache.sets[i].lines = (Line *)malloc(E * sizeof(Line)); // create an array of lines
        if (cache.sets[i].lines == NULL) {
            fprintf(stderr, "malloc failed");
            exit(1);
        }
        // At the beginning, every line is empty
        for (int j = 0; j < E; j++) {
            cache.sets[i].lines[j].valid = 0; // Initialize valid bits to 0
            cache.sets[i].lines[j].tag = 0; // Initialize tags to 0
            cache.sets[i].lines[j].last_used = 0; // Initialize last used counters to 0
        }
    }
    
    return cache;
}

/* This function frees the memory allocated for the cache */
void freeCache(Cache cache) {
    for (int i = 0; i < cache.S; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

/* Find the set index from the address */
unsigned long getSetIndex(unsigned long address, int s, int b) {
    return (address >> b) & ((1UL << s) - 1); // 1UL is an unsigned long 64bits
}

/* Find the tag from the address */
unsigned long getTag(unsigned long address, int s, int b) {
    return address >> (s + b);
}

/* This function simulates one memory access and updates 
   the number of hits, misses, and evictions */
void accessCache(Cache cache, unsigned long address,
                 int *hits, int *misses, int *evictions, int *clock) {

    // find which set this address belongs to                
    unsigned long set_index = getSetIndex(address, cache.s, cache.b);
    // find the tag for this address
    unsigned long tag = getTag(address, cache.s, cache.b);

    // get the set we need to access
    Set current_set = cache.sets[set_index];

    int hit_line = -1;
    int empty_line = -1;
    int last_used = 0;

    /* Find hit, empty line, and LRU line */
    for (int i = 0; i < cache.E; i++) {
        if (current_set.lines[i].valid == 1 &&
            current_set.lines[i].tag == tag) {
            hit_line = i;
            break;
        }

        if (current_set.lines[i].valid == 0 && empty_line == -1) {
            empty_line = i;
        }

        if (current_set.lines[i].last_used <
            current_set.lines[last_used].last_used) {
            last_used = i;
        }
    }

    /* Case 1: if we found the block already in the cache, it's a hit */
    if (hit_line != -1) {
        (*hits)++;
        (*clock)++;
        current_set.lines[hit_line].last_used = *clock;
        return;
    }

    /* Case 2: if not found, it's a miss */
    (*misses)++;

    /* If there is an empty line, put the block there */
    if (empty_line != -1) {
        (*clock)++;
        current_set.lines[empty_line].valid = 1;
        current_set.lines[empty_line].tag = tag;
        current_set.lines[empty_line].last_used = *clock;
        return;
    }

    /* Case 3: if no empty line, replace the least recently used line */
    (*evictions)++;
    (*clock)++;
    current_set.lines[last_used].valid = 1;
    current_set.lines[last_used].tag = tag;
    current_set.lines[last_used].last_used = *clock;
}

int main(int argc, char *argv[])
{
    int s = 0;
    int E = 0;
    int b = 0;
    char *tracefile = NULL;

    int option;

    /* Read command-line arguments */
    while ((option = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (option) {
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                tracefile = optarg;
                break;
            default:
                printf("Usage: ./csim -s <s> -E <E> -b <b> -t <tracefile>\n");
                return 1;
        }
    }

    Cache cache = createCache(s, E, b);

    int hits = 0;
    int misses = 0;
    int evictions = 0;
    int clock = 0;

    FILE *fp = fopen(tracefile, "r");
    if (fp == NULL) {
        printf("Error: could not open trace file.\n");
        freeCache(cache);
        return 1;
    }

    char operation;
    unsigned long address;
    int size;

    /* Read each trace line */
    while (fscanf(fp, " %c %lx,%d", &operation, &address, &size) == 3) {
        if (operation == 'I') {
            continue;   // ignore instruction loads
        }

        if (operation == 'L' || operation == 'S') {
            accessCache(cache, address, &hits, &misses, &evictions, &clock);
        } else if (operation == 'M') {
            /* M = load + store, so two accesses */
            accessCache(cache, address, &hits, &misses, &evictions, &clock);
            accessCache(cache, address, &hits, &misses, &evictions, &clock);
        }
    }

    fclose(fp);
    freeCache(cache);

    printSummary(hits, misses, evictions);
    return 0;
}
