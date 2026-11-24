---
type: research
status: implementation-host-tested
updated: 2026-11-24
baseline: 5b03e199b6aec3f00cd1a8360d4fe650458993c6
tags: [sys-botbase, performance, opt009]
---
# OPT-009 · Wi-Fi read experiment D016

Independent branch `perf/opt009-wifi-read`, based on sys-botbase 2.5 source. No automatic release or installation. Master/USB/agent unchanged.

## Changes

- `read_wire.c/h`: poll with a 5s monotonic receive budget, 4096-byte MSG_PEEK then consume only through the first LF. Accept CRLF and LF; reject embedded NUL, over-capacity and incomplete lines. Capacity is the existing 22016 bytes including LF. A second command remains in the socket; only one reader may own the socket. Host-tested on POSIX stream sockets, not libnx yet.
- `main.c`: use bounded helper; release freezeMutex during receive, reacquire before parse and dispatch. Still serial command dispatch and original default 50ms loop sleep. Socket output remains stdout redirected by dup2, now unbuffered to avoid carrying pending output into another client; close/shutdown failed connections. Icon/pixel output also uses the bulk encoder to avoid regressions caused by unbuffering. Check allocation and output size for those two paths. No guarantee of send deadline yet; slow output and incomplete receive can still stall the main command dispatcher. No persistent cross-command debug handles, no parallel process memory reads.
- `args.c`: allocation checks and free empty argument string; parser still uses two tokenization passes.
- `commands.c`: uppercase Hex via 16-character lookup, 8192-byte encoding buffer, at most 4096 input bytes per fwrite. Preserve response order and terminating LF. Full HOME box (24480 bytes) via one-range Multi needs six fwrite calls plus LF on success; continuous path keeps original MAX_LINE_LENGTH server read chunks and has seven data fwrite calls for this geometry (six for 22016 bytes, one for 2464 bytes). This is not a packet count or timing result. Multi reads still call readMem for each range, with one attach/detach; no automatic coalescing. Multi allocation bounded to 512 ranges and 1 MiB total, reject zero-length ranges, arithmetic overflow and allocation failure; pointerPeek now uses one-range Multi with same cap, so oversized historical requests are deliberately rejected in the experiment. Existing pointerPeekMulti 40-range limit enforced before storing/resolving group; minimum group size checked. Normal accepted read commands retain uppercase Hex+LF; no blanket claim of compatibility for invalid/oversized requests. Existing debug result/echo options can add text, disable during parity testing.
- `configure readPerf 1` enables last-read profiling, `configure readPerf 0` disables and clears it. `getReadPerf` returns one explicit diagnostic line prefixed readperf-v1, or unavailable. No timing text appended to existing binary/hex replies; getVersion stays 2.5. Samples global to dispatcher, not per client; run with one client. Raw counter ticks from armGetSystemTick; convert using verified target counter frequency, not assumed wall clock. Fields: ok, ranges, bytes, calls (attempted readMem), input_ticks, parse_dispatch_ticks (also includes metadata/pointer resolution where applicable), attach_ticks, read_ticks, output_ticks (combined encode/write/LF/flush), encode_ticks, write_ticks (fwrite call time, not wire delivery), detach_ticks, total_ticks (read function entry through cleanup). Overlap output subfields; do not sum twice. Input excludes mutex reacquisition; total excludes input/parse; read failures recorded when function reached. Profiling overhead exists. No real device timing claimed.

## Host regression evidence

Compiled and executed exact Git blob content in Linux using cc with -Wall -Wextra -Werror. Hashes:
- read_wire.h: 65fb02c57ac0e92f78d3c488d795ef822edb0a35
- read_wire.c: 1db1fb4b8d1729c2a52555ccce4a8186e37b34eb
- tests/test_read_wire.c: 92951094bb3ffad6ab94b562ddd56c6f14dcc9ec

```sh
cc -std=c11 -Wall -Wextra -Werror -O2 -pthread \
  -Isys-botbase/source sys-botbase/source/read_wire.c tests/test_read_wire.c \
  -o /tmp/test_read_wire
/tmp/test_read_wire
cc -std=c11 -Wall -Wextra -Werror -g -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
  -Isys-botbase/source sys-botbase/source/read_wire.c tests/test_read_wire.c \
  -o /tmp/test_read_wire_san
/tmp/test_read_wire_san
```

Both returned 0, output `read_wire host tests passed`. Covers pipelined LF/CRLF, fragmented input, exact capacity/overlength, embedded NUL, deadline, EOF, full 24480-byte fixture and six timed encoder chunks. Synthetic tick assertions do not measure performance. Host tests do not compile main.c/commands.c/libnx or validate socket behavior on Switch.

## Device acceptance

1. With matching devkitPro/libnx, build `make -C sys-botbase` locally; archive baseline and experimental artifact SHA256, exact toolchain version, source SHA. No automatic installation.
2. Compare original master vs experiment on same HOME 4.1.0 / console / network; disable debugResultCodes/echoCommands; keep 50ms loop sleep and client pacing identical. Verify getVersion, title/build, pointer reads, peekAbsolute, peekAbsoluteMulti and ordinary controls/screenshot/icon. Test malformed/timeout/reconnect in isolated test environment.
3. Use HOME-Live D015 opt-in differential tool for 20+ stable groups per configuration; compare identical full-byte coverage. Separately use configure readPerf and getReadPerf with one client to collect stage counters; clear profiling for latency comparison. Include empty/partial/full and sparse addresses. Archive failures and unstable references separately; no zero-byte/partial success.
4. Compare p50/p95, CPU and memory impact where measurable; inspect freeze worker and UI command latency. Only after passing test controlled experiments on mainLoopSleepTime, contiguous range merge, parser pass reduction and socket write deadlines. Revert to original known-good firmware on failure.

Full Switch build and device tests NOT_RUN (DEVKITPRO unset, no Switch in this runtime). Do not tag or publish stable release. Rollback: use existing master firmware; no persistent-data migration.
