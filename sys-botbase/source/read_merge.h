#ifndef BOTBASE_READ_MERGE_H
#define BOTBASE_READ_MERGE_H
#include <stdbool.h>
#include <stdint.h>

/* Only merge consecutive ranges in wire order. No gaps, overlaps, sorting,
 * padding, or speculative reads. A large original range remains unchanged. */
static inline bool read_merge_can_extend(uint64_t address, uint64_t length,
    uint64_t next_address, uint64_t next_length, uint64_t limit)
{
    return length != 0 && next_length != 0 &&
        length <= limit && next_length <= limit - length &&
        address <= UINT64_MAX - length &&
        address + length == next_address &&
        next_address <= UINT64_MAX - (next_length - 1);
}
#endif
