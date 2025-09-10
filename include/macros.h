#include <stdio.h>
#include <stdlib.h>

#define ASSERT(x)                                                                   \
    if (!(x))                                                                       \
    {                                                                               \
        printf("Assertion failed: %s, file %s, line %d\n", #x, __FILE__, __LINE__); \
        exit(1);                                                                    \
    }

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))
