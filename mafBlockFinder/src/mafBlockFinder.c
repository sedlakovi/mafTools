/* 
 * Copyright (C) 2011-2012 by 
 * Dent Earl (dearl@soe.ucsc.edu, dentearl@gmail.com)
 * ... and other members of the Reconstruction Team of David Haussler's 
 * lab (BME Dept. UCSC).
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE. 
 */
#include <assert.h>
#include <errno.h> // file existence via ENOENT
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "sharedMaf.h"

int g_verbose_flag = 0;
int g_debug_flag = 0;

void usage(void);
void parseOptions(int argc, char **argv, char *filename, char *seqName, uint32_t *position);
void checkRegion(unsigned lineno, char *fullname, uint32_t pos, uint32_t start, 
                 uint32_t length, uint32_t sourceLength, char strand);
void searchInput(mafFileApi_t *mfa, char *fullname, unsigned long pos);

void usage(void) {
    fprintf(stderr, "Usage: mafBlockFinder --maf [path to maf] "
            "--seq [sequence name (and possibly chr)] "
            "--pos [position to search for] [options]\n\n"
            "mafBlockFinder is a program that will look through a maf file for a\n"
            "particular sequence name and location. If a match is found the line\n"
            "number and first few fields are returned. If no match is found\n"
            "nothing is returned.\n\n");
    fprintf(stderr, "Options: \n"
            "  -h, --help     show this help message and exit.\n"
            "  -m, --maf      path to maf file.\n"
            "  -s, --seq      sequence name.chr e.g. `hg18.chr2.'\n"
            "  -p, --pos      position along the chromosome you are searching for.\n"
            "                 Must be a positive number.\n"
            "  -v, --verbose  turns on verbose output.\n");
    exit(EXIT_FAILURE);
}
void parseOptions(int argc, char **argv, char *filename, char *seqName, uint32_t *position) {
    extern int g_debug_flag;
    extern int g_verbose_flag;
    int c;
    int setMName = 0, setSName = 0, setPos = 0;
    int32_t tempPos = 0;
    while (1) {
        static struct option long_options[] = {
            {"debug", no_argument, &g_debug_flag, 1},
            {"verbose", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {"maf",  required_argument, 0, 'm'},
            {"seq",  required_argument, 0, 's'},
            {"sequence",  required_argument, 0, 's'},
            {"pos",  required_argument, 0, 'p'},
            {"position",  required_argument, 0, 'p'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long(argc, argv, "m:s:p:v:h",
                        long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 0:
            break;
        case 'm':
            setMName = 1;
            sscanf(optarg, "%s", filename);
            break;
        case 's':
            setSName = 1;
            sscanf(optarg, "%s", seqName);
            break;
        case 'p':
            setPos = 1;
            tempPos = strtoll(optarg, NULL, 10);
            if (tempPos < 0) {
                fprintf(stderr, "Error, --pos %d must be nonnegative.\n", tempPos);
                usage();
            }
            *position = tempPos;
            break;
        case 'v':
            g_verbose_flag++;
            break;
        case 'h':
        case '?':
            usage();
            break;
        default:
            abort();
        }
    }
    if (!(setMName && setSName && setPos)) {
        fprintf(stderr, "specify --maf --seq --position\n");
        usage();
    }
    // Check there's nothing left over on the command line 
    if (optind < argc) {
        char errorString[30] = "Unexpected arguments:";
        while (optind < argc) {
            strcat(errorString, " ");
            strcat(errorString, argv[optind++]);
        }
        fprintf(stderr, "%s\n", errorString);
        usage();
    }
}
bool insideLine(mafLine_t *ml, uint32_t pos) {
    // check to see if pos is inside of the maf line
    uint32_t absStart, absEnd;
    if (ml->strand == '-') {
        absStart =  ml->sourceLength - (ml->start + ml->length);
        absEnd = ml->sourceLength - ml->start - 1;
    } else {
        absStart = ml->start;
        absEnd = ml->start + ml->length - 1;
    }
    if ((absStart <= pos) && (absEnd >= pos))
        return true;
    return false;
}
void checkBlock(mafBlock_t *mb, char *fullname, uint32_t pos) {
    mafLine_t *ml = mb->headLine;
    while (ml != NULL) {
        if (ml->type != 's') {
            ml = ml->next;
            continue;
        }
        if (!(strcmp(ml->species, fullname) == 0)) {
            ml = ml->next;
            continue;
        }
        if (insideLine(ml, pos))
            printf("%u: s %s %u %u %c %u ...\n", ml->lineNumber, fullname, ml->start, 
                   ml->length, ml->strand, ml->sourceLength);
        ml = ml->next;
    }
}
void searchInput(mafFileApi_t *mfa, char *fullname, unsigned long pos) {
    mafBlock_t *thisBlock = NULL;
    while ((thisBlock = maf_readBlock(mfa)) != NULL) {
        checkBlock(thisBlock, fullname, pos);
        maf_destroyMafBlockList(thisBlock);
    }
}

int main(int argc, char **argv) {
    extern const int kMaxStringLength;
    char filename[kMaxStringLength];
    char targetName[kMaxStringLength];
    uint32_t targetPos;
    parseOptions(argc, argv,  filename, targetName, &targetPos);
    mafFileApi_t *mfa = maf_newMfa(filename, "r");

    searchInput(mfa, targetName, targetPos);
    maf_destroyMfa(mfa);
   
    return EXIT_SUCCESS;
}