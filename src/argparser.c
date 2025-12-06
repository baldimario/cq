#include "argparser.h"

#include <stdio.h>

void printUsage(char* program_name) {
    fprintf(stderr, "Usage: %s <file_name> <options>\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-h\tShow this help message\n");
    fprintf(stderr, "\t-p\tEnable printing of the CSV file\n");
    fprintf(stderr, "\t-e\tExpression to evaluate\n");
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

char* getOptionValue(int argc, char** argv, char option) {
    for (int i = 1; i < argc - 1; i++) {
        char* arg = argv[i];

        if (arg[0] == '-' && arg[1] == option && arg[2] == '\0') {
            return argv[i + 1];
        }
    }
    return NULL;
}