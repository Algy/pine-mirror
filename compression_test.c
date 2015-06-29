#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lz4/lib/lz4.h"
#include "lz4/lib/lz4hc.h"

static void pfree(char **p) {
    free(*p);
    *p = NULL;
}

#define AUTOF __attribute__((cleanup(pfree)))




int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }
    int compression_level = 8;
    if (argc >= 3) {
        compression_level = atoi(argv[2]);
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Cannot open the file\n");
        return 1;
    }
    fseek(fp, 0L, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    char *buffer AUTOF = calloc(filesize + 1, 1);
    fread(buffer, 1, filesize + 1, fp);
    buffer[filesize] = 0;


    printf("The size of original file: %.2lfKB\n", (double)filesize / 1000.);
    printf("Comrpession Level: %d\n", compression_level);

    int compress_bound = LZ4_compressBound(filesize);
    printf("Estimated Worst compress bound: %.2lfKB\n", (double)compress_bound/1000.0);

    char* compressed_buffer AUTOF = calloc(compress_bound, 1);
    clock_t stc = clock();
    int compressed_filesize = LZ4_compress_HC(buffer, compressed_buffer, filesize, compress_bound, compression_level);
    clock_t edc = clock();
    printf("The size of Compressed file: %.2lfKB\n", (double)compressed_filesize/1000.0);
    printf("Compression time: %.2lfus\n", (double)(edc - stc) /CLOCKS_PER_SEC * 1000.0 * 1000.0);

    char *dec_buffer AUTOF = calloc(filesize, 1);
    stc = clock();
    if (LZ4_decompress_fast(compressed_buffer, dec_buffer, filesize) < 0) {
        printf("Malformed binary compressed\n");
    }
    edc = clock();
    printf("Decompression time: %.2lfus\n", (double)(edc - stc) /CLOCKS_PER_SEC * 1000.0 * 1000.0);
    if (memcmp(buffer, dec_buffer, filesize)) {
        printf("original file != decompressed one!!!\n");
    }
    fclose(fp);
    return 0;
}

