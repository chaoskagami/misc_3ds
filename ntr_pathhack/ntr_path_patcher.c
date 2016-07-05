#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RELOC_COUNT 9

#include <stdint.h>
#include <stdlib.h>

#define ALPHABET_LEN 256
#define NOT_FOUND patlen
#define max(a, b) ((a < b) ? b : a)

// Quick Search algorithm, adapted from
// http://igm.univ-mlv.fr/~lecroq/string/node19.html#SECTION00190
uint32_t
memfind(uint8_t *startPos, uint32_t size, const void *pattern, uint32_t patternSize)
{
    const uint8_t *patternc = (const uint8_t *)pattern;

    // Preprocessing
    uint32_t table[ALPHABET_LEN];

    for (uint32_t i = 0; i < ALPHABET_LEN; ++i)
        table[i] = patternSize + 1;
    for (uint32_t i = 0; i < patternSize; ++i)
        table[patternc[i]] = patternSize - i;

    // Searching
    uint32_t j = 0;

    while (j <= size - patternSize) {
        if (memcmp(patternc, startPos + j, patternSize) == 0)
            return j;
        j += table[startPos[j + patternSize]];
    }

    return 0;
}

int main(int argc, char** argv) {
    uint32_t size = 0;
    uint32_t base = 0x100100;
    uint8_t* mem = NULL;
    FILE* ntr = NULL;
    int expand = 0;
    int vanilla = 0;

    static uint8_t* find_strings[RELOC_COUNT] = {
        "/ntr.bin",
        "/plugin/%s",
        "/debug.flag",
        "/axiwram.dmp",
        "/pid0.dmp",
        "/pid2.dmp",
        "/pid3.dmp",
        "/pidf.dmp",
        "/arm11.bin",
    }; // Nine buffers

    static uint8_t* fix_strings[RELOC_COUNT] = {
        "/3ds/ntr/ntr.bin",
        "/3ds/ntr/plugin/%s",
        "/3ds/ntr/debug.flag",
        "/3ds/ntr/axiwram.dmp",
        "/3ds/ntr/pid0.dmp",
        "/3ds/ntr/pid2.dmp",
        "/3ds/ntr/pid3.dmp",
        "/3ds/ntr/pidf.dmp",
        "/3ds/ntr/arm11.bin",
    }; // Nine buffers

    if (argc < 2) {
        fprintf(stderr, "No NTR.bin provided to patch, see --help for usage or drop a file on the binary\n");
        exit(0);
    }

    if (!strcmp(argv[1], "--help")) {
        printf("Automated NTR path patcher r1\n");
        printf("Usage: %s [-v] NTR.bin\n", argv[0]);
        printf(" -v       Keep vanilla paths, do not redirect to `/3ds/ntr`\n");
        printf("An output file will be generated named `NTR_patched.bin`\n");
        exit(0);
    } else if (!strcmp(argv[1], "-v")) {
        printf("Keeping vanilla paths.\n");
        vanilla = 1;
    }

    // Get size, allocate memory, clear and read to memory
    ntr = fopen(argv[1], "rb");
    fseek(ntr, 0, SEEK_END);
    size = ftell(ntr);
    rewind(ntr);
    mem = malloc(size + (0x100 * 9));
    memset(mem, 0, size + (0x100 * 9));
    fread(mem, 1, size, ntr);
    fclose(ntr);

    fprintf(stderr, "size: %u\n", size);

    // Find location of strings to relocate.
    for(int i=0; i < RELOC_COUNT; i++) {
        char* str = find_strings[i];
        uint32_t strl = strlen(str);
        uint32_t off = memfind(mem, size, str, strl);

        if (off == 0) {
            fprintf(stderr, "String not found for \"%s\" (%d). Skipping.\n", str, strl);
            continue;
        }

        fprintf(stderr, "Found string \"%s\" in ntr: %x\n", str, off);

        // Clear string data
        memset(&mem[off], 0, strl);

        off += base; // Rebase pointer relative to NTRs base.

        uint32_t *patchme = (uint32_t*)memfind(mem, size, (uint8_t*)&off, 4); // Find xref
        if (patchme == 0) {
            fprintf(stderr, "Pointer for string \"%s\" is missing! Aborting.\n", str);
            exit(1);
        }

        fprintf(stderr, "Found pointer @ %08x\n", patchme);

        // New offset is end + patch count * buffer
        uint32_t patch_off = size + (0x100 * expand);
        if (vanilla)
            strcpy(&mem[patch_off], str);
        else
            strcpy(&mem[patch_off], fix_strings[i]);

        expand += 1;

        uint32_t* patch_rel = (uint32_t*)(&mem[(uint32_t)patchme]);

        // Rebase new pointer
        *patch_rel = patch_off + base;
    }

    if (expand) {
        FILE* out_fix = fopen("ntr_patched.bin", "wb");
        fwrite(mem, 1, size + (0x100 * expand), out_fix);
        fclose(out_fix);
    }

    free(mem);
    return 0;
}
