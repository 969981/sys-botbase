#ifndef BOTBASE_READ_WIRE_H
#define BOTBASE_READ_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Preserve the original LF-delimited request and uppercase Hex response.
 * Receive only through the first LF, leaving pipelined bytes in the socket.
 * Return -1 for EOF, malformed/oversize line, timeout or socket error. */
int read_wire_line(int fd, char *line, size_t capacity, int timeout_ms);
int read_wire_hex(FILE *out, const uint8_t *data, size_t length);
int read_wire_hex_timed(FILE *out, const uint8_t *data, size_t length,
    uint64_t (*ticks)(void), uint64_t *encode_ticks, uint64_t *write_ticks);

#endif
