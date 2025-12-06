#ifndef ARGPARSER_H
#define ARGPARSER_H

void printUsage(char* program_name);
unsigned char checkArgFlags(int argc, char** argv, char character);
char* getOptionValue(int argc, char** argv, char option);

#endif
