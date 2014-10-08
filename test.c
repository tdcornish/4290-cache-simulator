#include "cachesim.h"

int main(int argc, char* argv[]) {
    block first;
    first.tag = 0x5000;
    first.index = 2;
    first.timestamp = 5;

    block second;
    second.tag = 0x6000;
    second.index = 3;
    second.timestamp = 2;

    printf("first tag %llx\n", first.tag);
    swap(&first, &second);
    printf("first tag %llu\n", first.timestamp);
}

void swap(block* first, block* second){
    block* temp = (block*)malloc(sizeof(block));
    memcpy(temp, first, sizeof(block));
    memcpy(first, second, sizeof(block));
    memcpy(second, temp, sizeof(block));
}
