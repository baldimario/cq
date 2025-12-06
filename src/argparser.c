#include "argparser.h"

#include <stdio.h>

void printUsage(char* program_name) {
    fprintf(stderr, "Usage: %s <file_name> <options>\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -p    Enable printing of the CSV file\n");
}

unsigned char checkArgFlags(int argc, char** argv, char character) {
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if (arg[0] == '-') {
            for (int j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == character) {
                    return 1;
                }
            }
        }
    }

    return 0;
}