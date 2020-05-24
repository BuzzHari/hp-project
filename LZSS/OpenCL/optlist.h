#ifndef OPTLIST_H
#define OPTLIST_H

#define    OL_NOINDEX    -1   

typedef struct option_t
{
    char option;                /* the current character option character */
    char *argument;             /* pointer to arguments for this option */
    int argIndex;               /* index into argv[] containing the argument */
    struct option_t *next;      /* the next option in the linked list */
} option_t;


/* returns a linked list of options and arguments similar to getopt() */
option_t *GetOptList(int argc, char *const argv[], char *const options);

/* frees the linked list of option_t returned by GetOptList */
void FreeOptList(option_t *list);

/* return a pointer to file name in a full path.  useful for argv[0] */
char *FindFileName(const char *const fullPath);

#endif  /* ndef OPTLIST_H */
