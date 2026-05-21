#include "util.h"
void dump_hex(const void *data, size_t size) {
    const unsigned char *p = data;
    for (size_t i = 0; i < size; i += 16) {
        printf("%08zx: ", i);
        
        // Print hex
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                printf("%02x ", p[i + j]);
            } else {
                printf("   ");
            }
        }
        
        printf(" | ");
        
        // Print ASCII
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                unsigned char c = p[i + j];
                printf("%c", isprint(c) ? c : '.');
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
}
