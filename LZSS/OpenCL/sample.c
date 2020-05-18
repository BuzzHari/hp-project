#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "lzss.h"
#include "optlist.h"

typedef enum
{
    ENCODE,
    DECODE
} modes_t;

int main(int argc, char *argv[])
{
    struct timeval t1_start,t1_end;

    option_t *optList;
    option_t *thisOpt;
    FILE *fpIn;             /* pointer to open input file */
    FILE *fpOut;            /* pointer to open output file */
    modes_t mode;

    /* initialize data */
    fpIn = NULL;
    fpOut = NULL;
    mode = ENCODE;

    /* parse command line */
    optList = GetOptList(argc, argv, "cdi:o:h?");
    thisOpt = optList;

    while (thisOpt != NULL)
    {
        switch(thisOpt->option)
        {
            case 'c':       /* compression mode */
                mode = ENCODE;
                break;

            case 'd':       /* decompression mode */
                mode = DECODE;
                break;

            case 'i':       /* input file name */
                if (fpIn != NULL)
                {
                    fprintf(stderr, "Multiple input files not allowed.\n");
                    fclose(fpIn);

                    if (fpOut != NULL)
                    {
                        fclose(fpOut);
                    }

                    FreeOptList(optList);
                    return -1;
                }

                /* open input file as binary */
                fpIn = fopen(thisOpt->argument, "rb");
                if (fpIn == NULL)
                {
                    perror("Opening input file");

                    if (fpOut != NULL)
                    {
                        fclose(fpOut);
                    }

                    FreeOptList(optList);
                    return -1;
                }
                break;

            case 'o':       /* output file name */
                if (fpOut != NULL)
                {
                    fprintf(stderr, "Multiple output files not allowed.\n");
                    fclose(fpOut);

                    if (fpIn != NULL)
                    {
                        fclose(fpIn);
                    }

                    FreeOptList(optList);
                    return -1;
                }

                /* open output file as binary */
                fpOut = fopen(thisOpt->argument, "wb");
                if (fpOut == NULL)
                {
                    perror("Opening output file");

                    if (fpIn != NULL)
                    {
                        fclose(fpIn);
                    }

                    FreeOptList(optList);
                    return -1;
                }
                break;

            case 'h':
            case '?':
                printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
                printf("options:\n");
                printf("  -c : Encode input file to output file.\n");
                printf("  -d : Decode input file to output file.\n");
                printf("  -i <filename> : Name of input file.\n");
                printf("  -o <filename> : Name of output file.\n");
                printf("  -h | ?  : Print out command line options.\n\n");
                printf("Default: %s -c -i stdin -o stdout\n",
                    FindFileName(argv[0]));

                FreeOptList(optList);
                return 0;
        }

        optList = thisOpt->next;
        free(thisOpt);
        thisOpt = optList;
    }

    /* use stdin/out if no files are provided */
    if (fpIn == NULL)
    {
        fpIn = stdin;
    }

    if (fpOut == NULL)
    {
        fpOut = stdout;
    }

    /* we have valid parameters encode or decode */
    gettimeofday(&t1_start,0);
    if (mode == ENCODE)
    {
        EncodeLZSS(fpIn, fpOut);
    }
    else
    {
        DecodeLZSS(fpIn, fpOut);
    }
    gettimeofday(&t1_end,0);
    double alltime = (t1_end.tv_sec-t1_start.tv_sec) + (t1_end.tv_usec - t1_start.tv_usec)/1000000.0;
    printf("\tAll the time took:\t%f \n", alltime);
    /* remember to close files */
    fclose(fpIn);
    fclose(fpOut);
    return 0;
}
