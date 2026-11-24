#include "read_merge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CAP 24480
typedef struct { uint64_t address, length; } Range;

static size_t simulate(const Range *ranges, size_t count, unsigned char *out, bool merge)
{
    size_t pos = 0, calls = 0;
    for (size_t i = 0; i < count;)
    {
        uint64_t start = ranges[i].address, length = ranges[i].length;
        ++i;
        while (merge && i < count && read_merge_can_extend(start, length,
            ranges[i].address, ranges[i].length, CAP))
            length += ranges[i++].length;
        for (uint64_t j = 0; j < length; ++j)
            out[pos++] = (unsigned char)((start + j) ^ ((start + j) >> 8));
        ++calls;
    }
    return calls;
}

int main(void)
{
    Range slots[30];
    unsigned char a[CAP], b[CAP];
    for (size_t i = 0; i < 30; ++i) slots[i] = (Range){ 0x10000 + i * 816, 816 };
    assert(simulate(slots, 30, a, false) == 30);
    assert(simulate(slots, 30, b, true) == 1);
    assert(memcmp(a, b, CAP) == 0);
    for (size_t i = 0; i < 30; ++i) slots[i].length = 16;
    assert(simulate(slots, 30, b, true) == 30);

    Range mixed[] = {{100, 4}, {104, 3}, {200, 2}, {100, 2}, {101, 3}, {104, 1}};
    assert(simulate(mixed, 6, a, false) == 6);
    assert(simulate(mixed, 6, b, true) == 4);
    assert(memcmp(a, b, 15) == 0);
    assert(!read_merge_can_extend(100, CAP, 100 + CAP, 1, CAP));
    assert(!read_merge_can_extend(100, 0, 100, 1, CAP));
    assert(!read_merge_can_extend(100, 1, 101, 0, CAP));
    assert(!read_merge_can_extend(UINT64_MAX, 1, 0, 1, CAP));
    assert(!read_merge_can_extend(UINT64_MAX - 2, 1, UINT64_MAX - 1, 3, CAP));
    assert(read_merge_can_extend(UINT64_MAX - 2, 1, UINT64_MAX - 1, 2, CAP));
    puts("read_merge host tests passed");
    return 0;
}
