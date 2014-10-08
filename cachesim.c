#include "cachesim.h"

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @c The total number of bytes for data storage is 2^C
 * @b The size of a single cache line in bytes is 2^B
 * @s The number of blocks in each set is 2^S
 * @v The number of blocks in the victim cache is 2^V
 * @k The prefetch distance is k
 */

void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k) {
    pendingStride = 0;
    lastMissAddress = 0;
    writeBacks = 0;
    counter = 0;
    vcCounter = 0;
    totalBytes = pow(2, c);
    bytesPerBlock = pow(2, b);
    blocksPerSet = pow(2, s);

    hit_time = 2 + 0.2 * s;
    prefetchSize = k;

    if(c - b == s){
        fullyAssociative = true;
    }

    totalRows = totalBytes/bytesPerBlock;

    //printf("totalRows: %d, blocksPerSet: %d\n\n", totalRows, blocksPerSet);

    offsetBits = b;
    indexBits = c-b-s;
    tagBits = 64 - indexBits - offsetBits;

    calculateMasks(offsetBits, indexBits, tagBits);

    if(!fullyAssociative){
        cache = (block**) malloc(totalRows * sizeof(block*));
    }
    else{
        cache = (block**) malloc(sizeof(block*));
        totalRows = 1;
    }

    for(int i = 0; i < totalRows; i++){
        cache[i] = (block*) malloc(blocksPerSet * sizeof(block));
        for(int j = 0; j < blocksPerSet; j++){
            cache[i][j].valid = false;
            cache[i][j].dirty = false;
            cache[i][j].prefetched = false;
        }
    }

    victimCacheSize = v;
    victimCache = (block*)malloc(v * sizeof(block));
    for(int i = 0; i < victimCacheSize; i++){
        victimCache[i].valid = false;
        victimCache[i].dirty = false;
        victimCache[i].prefetched = false;
    }
}
/**
 * Subroutine that simulates the cache one trace event at a time.
 * XXX: You're responsible for completing this routine
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, struct cache_stats_t* p_stats) {
    p_stats->accesses++;
    (rw == READ) ? p_stats->reads++ : p_stats->writes++;
    //printf("access\n");

    uint64_t index = (address & indexMask) >> offsetBits;
    uint64_t tag = (address & tagMask) >> (offsetBits + indexBits);
    uint64_t offset = (address & offsetMask);

    //printf("tagMask %llx\n", tagMask);
    //printf("indexMask %llx\n", indexMask);
    //printf("%c address: %llx, index: %llu, tag: %llx\n", rw, address, index, tag);

    block* set;
    if(fullyAssociative){
        set = cache[0];
    }
    else{
        set = cache[index];
    }

    block* foundBlock = (block*)malloc(sizeof(block));
    bool mainCacheHit = matchTag(set, tag, &foundBlock);

    if(!mainCacheHit){
        //L1 miss
        (rw == READ) ? p_stats->read_misses++ : p_stats->write_misses++;

        block* vcBlock = (block*)malloc(sizeof(block));
        bool victimCacheHit;
        victimCacheHit = checkVC(tag, index, &vcBlock);

        block* blockToReplace = getLRUBlock(set);

        if(victimCacheHit){
            swap(blockToReplace, vcBlock);
            vcBlock->timestamp = blockToReplace->timestamp;
            blockToReplace->timestamp = counter++;
            if(rw == WRITE){
                blockToReplace->dirty = true;
            }
        }
        else{
            //L1 and VC miss
            (rw==READ) ? p_stats->read_misses_combined++ : p_stats->write_misses_combined++;
            p_stats->vc_misses++;

            if(blockToReplace->valid){
                putInVictimCache(blockToReplace);
            }

            if(victimCacheSize == 0 && blockToReplace->dirty){
                writeBack(blockToReplace);
            }

            blockToReplace->tag = tag;
            blockToReplace->index = index;
            blockToReplace->offset = offset;
            blockToReplace->timestamp = counter++;
            blockToReplace->valid = true;
            blockToReplace->prefetched = false;

            if(rw == WRITE){
                blockToReplace->dirty = true;
            }
            else{
                blockToReplace->dirty = false;
            }
        }

        if(prefetchSize > 0){
            uint64_t blockAddrX = address & ~offsetMask;
            d = blockAddrX - lastMissAddress;
            lastMissAddress = blockAddrX;

            if(d == pendingStride){
                p_stats->prefetched_blocks += prefetch(blockAddrX, pendingStride,  prefetchSize);
            }

            pendingStride = d;
        }
    }
    else{
        foundBlock->timestamp = counter++;

        if(rw == WRITE){
            foundBlock->dirty = true;
        }
    }

    //printSet(set, index);
    //printf("\n\n");
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(struct cache_stats_t *p_stats) {
    p_stats->misses = p_stats->write_misses + p_stats->read_misses;
    p_stats->write_backs = writeBacks;
    p_stats->bytes_transferred = (p_stats->prefetched_blocks + p_stats->vc_misses + p_stats->write_backs) * 32;
    p_stats->hit_time = hit_time;
    p_stats->miss_penalty = 200;
    p_stats->miss_rate = (double)(p_stats->misses)/(p_stats->accesses);

    if(victimCacheSize == 0){
        p_stats->avg_access_time = p_stats->hit_time + p_stats->miss_rate * p_stats->miss_penalty;
    }
    else{
        double victimCacheMissRate = (double)p_stats->vc_misses/p_stats->misses;
        p_stats->avg_access_time = p_stats->hit_time + p_stats->miss_rate * victimCacheMissRate *  p_stats->miss_penalty;
    }

    for(int i = 0; i < totalRows; i++){
        free(cache[i]);
    }
    free(cache);
}

int prefetch(uint64_t blockAddr, int pendingStride, int prefetchSize){
    int numberOfBlocksPrefetched = 0;
    for(int i = 1; i <= prefetchSize; i++){
        numberOfBlocksPrefetched++;
        uint64_t address = blockAddr + i * pendingStride;
        uint64_t index = (address & indexMask) >> offsetBits;
        uint64_t tag = (address & tagMask) >> (offsetBits + indexBits);

        block* set;
        if(fullyAssociative){
            set = cache[0];
        }
        else{
            set = cache[index];
        }

        block* foundBlock = (block*)malloc(sizeof(block));
        bool mainCacheHit = matchTag(set, tag, &foundBlock);

        if(!mainCacheHit){
            block* vcBlock = (block*)malloc(sizeof(block));
            bool victimCacheHit;
            victimCacheHit = checkVC(tag, index, &vcBlock);

            block* blockToReplace = getLRUBlock(set);

            if(victimCacheHit){
                swap(blockToReplace, vcBlock);
                vcBlock->timestamp = blockToReplace->timestamp;
                blockToReplace->timestamp = minTimestamp(set) - 1;
                blockToReplace->prefetched = true;
            }
            else{
                if(blockToReplace->valid){
                    putInVictimCache(blockToReplace);
                }

                if(victimCacheSize == 0 && blockToReplace->dirty){
                    writeBack(blockToReplace);
                }

                blockToReplace->tag = tag;
                blockToReplace->index = index;
                blockToReplace->timestamp = minTimestamp(set) - 1;
                blockToReplace->valid = true;
                blockToReplace->prefetched = true;
                blockToReplace->dirty = false;
            }
        }
    }

    return numberOfBlocksPrefetched;
}

void calculateMasks(int offsetBits, int indexBits, int tagBits){
    int currentBit = 1;
    offsetMask = 0;
    for(int i = 0; i < offsetBits; i++){
        offsetMask = offsetMask | currentBit;
        currentBit = currentBit << 1;
    }

    if(!fullyAssociative){
        currentBit = 1;
        indexMask = 0;
        for(int i = 0; i < indexBits; i++){
            indexMask = indexMask | currentBit;
            currentBit = currentBit << 1;
        }
        indexMask = indexMask << offsetBits;

        tagMask = ~0 << (offsetBits + indexBits);
    }
    else{
        tagMask = ~0 << offsetBits;
    }

}

block* getLRUBlock(block* set){
    for(int i = 0; i < blocksPerSet; i++){
        if(!set[i].valid){
            //printf("block %d filled\n", i);
            return &set[i];
        }
    }

    block* lruEntry = &set[0];
    uint64_t lruTimestamp = lruEntry->timestamp;
    //int location;
    for(int i = 1; i< blocksPerSet; i++){
        uint64_t currentBlockTimestamp = set[i].timestamp;
        if(currentBlockTimestamp < lruTimestamp){
            lruEntry = &set[i];
            lruTimestamp = currentBlockTimestamp;
            //        location = i;
        }
    }

    //printf("block %d replaced %llx\n", location, lruEntry->tag);

    return lruEntry;
}

bool matchTag(block* set, uint64_t tag, block** foundBlock){
    bool found = false;
    int i=0;
    while(!found && i < blocksPerSet){
        block entry = set[i];
        if(entry.valid && entry.tag == tag){
            found = true;
            //printf("matched tag %llx block %d\n", entry.tag, i);
            *foundBlock = &set[i];
        }

        i++;
    }

    return found;
}

bool checkVC(uint64_t tag, uint64_t index, block** foundBlock){
    if(victimCacheSize == 0){
        return false;
    }

    for(int i = 0; i< victimCacheSize; i++){
        block entry = victimCache[i];
        if(entry.valid && entry.index == index && entry.tag == tag){
            *foundBlock = &victimCache[i];
            //printf("victim matched tag %llx block %d\n", victimCache[i].tag, i);
            return true;
        }
    }
    return false;
}

void putInVictimCache(block* toInsert){
    if(victimCacheSize > 0){
        bool emptyBlockFound = false;
        int i=0;
        while(!emptyBlockFound && i < victimCacheSize){
            block* newEntry = &victimCache[i];
            if(!newEntry->valid){
                memcpy(newEntry, toInsert, sizeof(block));
                newEntry->timestamp = vcCounter++;
                emptyBlockFound = true;
            }

            i++;
        }

        if(!emptyBlockFound){
            block* firstIn = &victimCache[0];
            for(int i=1; i < victimCacheSize; i++){
                block* entry = &victimCache[i];
                if(entry->timestamp < firstIn->timestamp){
                    firstIn = entry;
                }
            }

            if(firstIn->dirty){
                writeBack(firstIn);
            }

            memcpy(firstIn, toInsert, sizeof(block));
            firstIn->timestamp=vcCounter++;
        }
    }
}

void writeBack(block* block){
    //printf("write back: %llx\n", block->tag);
    block->dirty = false;
    writeBacks++;
}

void printSet(block* set, int index){
    printf("Set %d: ", index);
    for(int i=0; i < blocksPerSet; i++){
        printf("%llx ", set[i].tag);
    }
    printf("\n");
    for(int i = 0; i<victimCacheSize; i++){
        printf("%llx, %llu\n", victimCache[i].tag, victimCache[i].index);
    }
}

void printBlock(block* toPrint){
    printf("tag: %llx\nindex: %llx\noffset: %lld\ntimestamp: %llu\n", toPrint->tag, toPrint->index, toPrint->offset, toPrint->timestamp);
}

void swap(block* first, block* second){
    block* temp = (block*)malloc(sizeof(block));
    memcpy(temp, first, sizeof(block));
    memcpy(first, second, sizeof(block));
    memcpy(second, temp, sizeof(block));
}

int minTimestamp(block* set){
    int lowestTimestamp = set[0].timestamp;
    for(int i = 1; i < blocksPerSet; i++){
        if(set[i].timestamp < lowestTimestamp){
            lowestTimestamp = set[i].timestamp;
        }
    }

    return lowestTimestamp;
}
