#ifndef CACHESIM_H
#define CACHESIM_H

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

struct cache_stats_t {
    uint64_t accesses;
    uint64_t reads;
    uint64_t read_misses;
    uint64_t read_misses_combined;
    uint64_t writes;
    uint64_t write_misses;
    uint64_t write_misses_combined;
    uint64_t misses;
    uint64_t write_backs;
    uint64_t vc_misses;
    uint64_t prefetched_blocks;
    uint64_t useful_prefetches;
    uint64_t bytes_transferred;

    double hit_time;
    uint64_t miss_penalty;
    double   miss_rate;
    double   avg_access_time;
};

typedef struct cache_block{
    uint64_t tag;
    uint64_t index;
    uint64_t offset;
    uint64_t timestamp;

    bool valid;
    bool dirty;
    bool prefetched;
}block;

void cache_access(char rw, uint64_t address, struct cache_stats_t* p_stats);
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k);
void complete_cache(struct cache_stats_t *p_stats);
void calculateMasks(int offsetBits, int indexBits, int tagBits);
block* getLRUBlock(block* set);
bool matchTag(block* set, uint64_t tag, block** foundBlock);
void writeBack(block* block);
void printSet(block* set, int index);
bool checkVC(uint64_t tag, uint64_t index, block** foundBlock);
void swap(block* first, block* second);
void putInVictimCache(block* toInsert);
void printBlock(block* toPrint);
int prefetch(uint64_t blockAddr, int pendingStride, int prefetchSize);
int minTimestamp(block* set);

static const uint64_t DEFAULT_C = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_V = 4;    /* 4 victim blocks */
static const uint64_t DEFAULT_K = 2;    /* 2 prefetch distance */

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';

block** cache;
block* victimCache;

uint64_t offsetMask;
uint64_t indexMask;
uint64_t tagMask;

uint64_t lastMissAddress;
uint64_t d;
int pendingStride;

int totalBytes;
int bytesPerBlock;
int blocksPerSet;
int totalRows;
int victimCacheSize;
int prefetchSize;

int offsetBits;
int indexBits;
int tagBits;

bool fullyAssociative;
double hit_time;
uint64_t writeBacks;

uint64_t counter;
uint64_t vcCounter;

#endif /* CACHESIM_H */
