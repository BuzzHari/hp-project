#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static option_t *MakeOpt(
    const char option, char *const argument, const int index);

static size_t MatchOpt(
    const char argument, char *const options);

option_t *GetOptList(const int argc, char *const argv[], char *const options)
{
    int nextArg;
    option_t *head, *tail;
    size_t optIndex;
    size_t argIndex;

    /* start with first argument and nothing found */
    nextArg = 1;
    head = NULL;
    tail = NULL;

    /* loop through all of the command line arguments */
    while (nextArg < argc)
    {
        argIndex = 1;

        while ((strlen(argv[nextArg]) > argIndex) && ('-' == argv[nextArg][0]))
        {
            /* attempt to find a matching option */
            optIndex = MatchOpt(argv[nextArg][argIndex], options);

            if (options[optIndex] == argv[nextArg][argIndex])
            {
                /* we found the matching option */
                if (NULL == head)
                {
                    head = MakeOpt(options[optIndex], NULL, OL_NOINDEX);
                    tail = head;
                }
                else
                {
                    tail->next = MakeOpt(options[optIndex], NULL, OL_NOINDEX);
                    tail = tail->next;
                }

                if (':' == options[optIndex + 1])
                {
                    /* the option found should have a text arguement */
                    argIndex++;

                    if (strlen(argv[nextArg]) > argIndex)
                    {
                        /* no space between argument and option */
                        tail->argument = &(argv[nextArg][argIndex]);
                        tail->argIndex = nextArg;
                    }
                    else if (nextArg < argc)
                    {
                        /* there must be space between the argument option */
                        nextArg++;
                        tail->argument = argv[nextArg];
                        tail->argIndex = nextArg;
                    }

                    break; /* done with argv[nextArg] */
                }
            }

            argIndex++;
        }

        nextArg++;
    }

    return head;
}

static option_t *MakeOpt(
    const char option, char *const argument, const int index)
{
    option_t *opt;

    opt = malloc(sizeof(option_t));

    if (opt != NULL)
    {
        opt->option = option;
        opt->argument = argument;
        opt->argIndex = index;
        opt->next = NULL;
    }
    else
    {
        perror("Failed to Allocate option_t");
    }

    return opt;
}

void FreeOptList(option_t *list)
{
    option_t *head, *next;

    head = list;
    list = NULL;

    while (head != NULL)
    {
        next = head->next;
        free(head);
        head = next;
    }

    return;
}

static size_t MatchOpt(
    const char argument, char *const options)
{
    size_t optIndex = 0;

    /* attempt to find a matching option */
    while ((options[optIndex] != '\0') &&
        (options[optIndex] != argument))
    {
        do
        {
            optIndex++;
        }
        while ((options[optIndex] != '\0') &&
            (':' == options[optIndex]));
    }

    return optIndex;
}

char *FindFileName(const char *const fullPath)
{
    int i;
    const char *start;                          /* start of file name */
    const char *tmp;
    const char delim[3] = {'\\', '/', ':'};     /* path deliminators */

    start = fullPath;

    /* find the first character after all file path delimiters */
    for (i = 0; i < 3; i++)
    {
        tmp = strrchr(start, delim[i]);

        if (tmp != NULL)
        {
            start = tmp + 1;
        }
    }

    return (char *)start;
}

