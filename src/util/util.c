#include "util.h"

void print_bytes(uint8_t* bytes, size_t num_bytes){
    for(size_t b; b < num_bytes; ++b){ 
        __print(bytes[b]);
    }
}

void print_byte(uint8_t byte){
    __print(byte);
}

static void __print(uint8_t byte){
    #ifdef PRINT_BIN
        for(int i = 7; i >= 0; --i){
            printf("%d", (byte >> i) & 1);
        }
    #elif defined(PRINT_HEX)
        printf("0x%02X",byte);
    #elif defined(PRINT_DEC)
        printf("%u",byte);
    #endif
    printf(" ");
}