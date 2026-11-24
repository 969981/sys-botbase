#define _POSIX_C_SOURCE 200809L
#include "read_wire.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void pair(int fd[2]) { assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0); }
static void end(int fd[2]) { close(fd[0]); close(fd[1]); }

static void *fragment(void *arg)
{
    int fd = *(int *)arg;
    struct timespec delay = { .tv_nsec = 20000000 };
    assert(write(fd, "peek", 4) == 4);
    nanosleep(&delay, NULL);
    assert(write(fd, "Absolute 0x1234 4\r\n", 19) == 19);
    return NULL;
}

static uint64_t fake_ticks(void)
{
    static uint64_t now = 0;
    return ++now;
}

int main(void)
{
    int fd[2];
    char line[64];
    pair(fd);
    const char *pipeline = "getVersion\r\ngetReadPerf\n";
    assert(write(fd[1], pipeline, strlen(pipeline)) == (ssize_t)strlen(pipeline));
    assert(read_wire_line(fd[0], line, sizeof(line), 1000) == 11);
    assert(strcmp(line, "getVersion\r") == 0);
    assert(read_wire_line(fd[0], line, sizeof(line), 1000) == 11);
    assert(strcmp(line, "getReadPerf") == 0);
    end(fd);

    pair(fd);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, fragment, &fd[1]) == 0);
    assert(read_wire_line(fd[0], line, sizeof(line), 1000) == 22);
    assert(strcmp(line, "peekAbsolute 0x1234 4\r") == 0);
    pthread_join(thread, NULL);
    end(fd);

    pair(fd);
    assert(write(fd[1], "1234567\n", 8) == 8);
    assert(read_wire_line(fd[0], line, 8, 1000) == 7);
    assert(write(fd[1], "12345678\n", 9) == 9);
    assert(read_wire_line(fd[0], line, 8, 1000) == -1);
    end(fd);

    pair(fd);
    const char malformed[] = {'a', '\0', '\n'};
    assert(write(fd[1], malformed, sizeof(malformed)) == (ssize_t)sizeof(malformed));
    assert(read_wire_line(fd[0], line, sizeof(line), 1000) == -1);
    end(fd);

    pair(fd);
    assert(write(fd[1], "partial", 7) == 7);
    assert(read_wire_line(fd[0], line, sizeof(line), 40) == -1);
    end(fd);

    pair(fd);
    assert(write(fd[1], "partial", 7) == 7);
    close(fd[1]);
    assert(read_wire_line(fd[0], line, sizeof(line), 1000) == -1);
    close(fd[0]);

    uint8_t bytes[24480];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    FILE *fp = tmpfile();
    assert(fp);
    assert(read_wire_hex(fp, bytes, sizeof(bytes)) == 0);
    assert(fputc('\n', fp) != EOF);
    assert(fflush(fp) == 0);
    assert(ftell(fp) == 48961);
    rewind(fp);
    for (size_t i = 0; i < sizeof(bytes); ++i)
    {
        char expected[3];
        snprintf(expected, sizeof(expected), "%02X", (unsigned)bytes[i]);
        assert(fgetc(fp) == expected[0]);
        assert(fgetc(fp) == expected[1]);
    }
    assert(fgetc(fp) == '\n');
    assert(fgetc(fp) == EOF);
    fclose(fp);

    fp = tmpfile();
    assert(fp);
    uint64_t encode_ticks = 0, write_ticks = 0;
    assert(read_wire_hex_timed(fp, bytes, sizeof(bytes), fake_ticks, &encode_ticks, &write_ticks) == 0);
    assert(encode_ticks == 6 && write_ticks == 6);
    assert(ftell(fp) == 48960);
    fclose(fp);
    puts("read_wire host tests passed");
    return 0;
}
