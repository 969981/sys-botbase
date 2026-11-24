#define _POSIX_C_SOURCE 200809L
#include "read_wire.h"
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

static int64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int read_wire_line(int fd, char *line, size_t capacity, int timeout_ms)
{
    if (!line || capacity < 2 || timeout_ms <= 0) return -1;
    int64_t start = monotonic_ms();
    if (start < 0) return -1;
    size_t used = 0;
    while (used < capacity)
    {
        int64_t now = monotonic_ms();
        if (now < start || now - start >= timeout_ms) return -1;
        struct pollfd p = { .fd = fd, .events = POLLIN };
        int ready = poll(&p, 1, timeout_ms - (int)(now - start));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0 || !(p.revents & POLLIN)) return -1;

        /* Peeking prevents consuming a second request in the same TCP packet.
         * There is exactly one socket reader in the dispatcher. */
        size_t limit = capacity - used;
        if (limit > 4096) limit = 4096;
        ssize_t n = recv(fd, line + used, limit, MSG_PEEK);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        char *lf = memchr(line + used, '\n', (size_t)n);
        size_t consume = lf ? (size_t)(lf - (line + used)) + 1 : (size_t)n;
        ssize_t got = recv(fd, line + used, consume, 0);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return -1;
        size_t previous = used;
        used += (size_t)got;
        if (memchr(line + previous, '\0', (size_t)got)) return -1;
        if (line[used - 1] == '\n')
        {
            line[used - 1] = '\0';
            return (int)(used - 1);
        }
    }
    return -1;
}

int read_wire_hex(FILE *out, const uint8_t *data, size_t length)
{
    static const char digits[] = "0123456789ABCDEF";
    char buffer[8192];
    while (length)
    {
        size_t count = length > sizeof(buffer) / 2 ? sizeof(buffer) / 2 : length;
        for (size_t i = 0; i < count; ++i)
        {
            buffer[2 * i] = digits[data[i] >> 4];
            buffer[2 * i + 1] = digits[data[i] & 15];
        }
        if (fwrite(buffer, 1, count * 2, out) != count * 2)
            return -1;
        data += count;
        length -= count;
    }
    return 0;
}
