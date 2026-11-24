#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int parseArgs(char* origargstr, int (*callback)(int, char**))
{
    if (!origargstr || !callback) return -1;
    size_t length = strlen(origargstr);
    if (length == SIZE_MAX) return -1;
    char* argstr = malloc(length + 1);
    if (!argstr) return -1;
    memcpy(argstr, origargstr, length + 1);

    /* Preserve the original command for echoCommands. Tokenize the copy
       exactly once; grow the pointer array only when needed. */
    size_t capacity = 0;
    int argc = 0;
    char** argv = NULL;
    char* p = argstr;
    while (*p)
    {
        p += strspn(p, " \r\n");
        if (!*p) break;
        if (argc == INT_MAX) goto fail;
        if ((size_t)argc == capacity)
        {
            size_t next = capacity ? capacity * 2 : 16;
            if (next < capacity || next > SIZE_MAX / sizeof(*argv)) goto fail;
            char** grown = realloc(argv, next * sizeof(*argv));
            if (!grown) goto fail;
            argv = grown;
            capacity = next;
        }
        argv[argc++] = p;
        p += strcspn(p, " \r\n");
        if (*p) *p++ = '\0';
    }
    int ret = callback(argc, argv);
    free(argv);
    free(argstr);
    return ret;

fail:
    free(argv);
    free(argstr);
    return -1;
}
