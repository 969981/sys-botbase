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

## D017 · CI prerelease and follow-up implementation (2026-11-24)

- CI: [opt009-experiment-release.yml](../.github/workflows/opt009-experiment-release.yml), workflow commit `6812932c13ddacb9d0d373f8011b596b53b42b87`. Push on experimental source/workflow paths triggers host regression, devkitPro build (same pinned image as upstream workflow), packaging and artifact verification, then SHA-specific prerelease; never sets latest or changes stable tag. Docs-only pushes do not retrigger. At record time no push-run result was available through connector; do not mark build/publish successful. [Actions](https://github.com/969981/sys-botbase/actions/workflows/opt009-experiment-release.yml).
- Parser: args.c commit `36f94644838662b69d6c26b5d0e36b8ffc55bda3`, exact blob `7eec81d68df1befd1b5f9313b0dca97f16748392`. Single copy, single tokenization pass, checked geometric argv growth; retains original echo string and original delimiter semantics. Local C compiler -Wall -Wextra -Werror, normal and ASan/UBSan parser harness passed (empty, three arguments, 601 arguments, original string, null argument); portable wire fixtures also passed again. The local parser harness was not part of CI at this revision.
- Socket: main.c commit `520f7db484340192e042964d816121f24e3f54b2` sets SO_SNDTIMEO=5s per blocking send on accepted sockets; fail closed if setting fails, no claim of overall response deadline. Existing output-error shutdown path retained; connection table allocations checked. libnx support/timeout propagation must be verified on Switch; no firmware build result yet.
- HOME-Live develop: opt-in `HOMELIVE_MULTI_READ_SERVER_PERF=1` plus existing audit switch. Experimental firmware only, one client, known readperf-v1 support; captures server counters and client read time for each of the five reads, separate query cost, auditId, parity verdict. Counters remain ticks, no assumed frequency. Profiles perturb pacing; collect unprofiled comparison separately. M0/M1 remains NOT_RUN.

## D018 · Adjacent absolute ranges (2026-11-24)

Implementation [7737a9f](https://github.com/969981/sys-botbase/commit/7737a9f58ef887d2d5e18c0bf936c7838420b243). Only the absolute Multi dispatcher can opt into merging; `peekMulti`, `peekMainMulti`, `pointerPeekMulti`, single reads and write paths retain the previous read geometry. Global switch defaults off at process startup.

- `configure readMerge 1` / `configure readMerge 0`; `getReadMerge` replies `readmerge-v1 enabled=0 max_bytes=24480` or enabled=1. Single-client experimental testing only.
- Merge only immediately adjacent entries in wire order with checked address arithmetic and a 24480-byte merged-span budget. No sort, gap reads, overlap collapse or duplicate removal. A larger original range is left alone, not enlarged. Existing count/total-allocation limits remain. Failed merged SVC is not automatically retried; no successful partial response. The readperf ranges field remains original request count, calls counts actual attempted SVCs.
- Host synthetic fixture: 30 contiguous 816-byte ranges -> one simulated read with byte-exact 24480-byte output; 30 spaced 16-byte headers remain 30; mixed ordered/repeated/overlapping intervals preserve all requested bytes, six ranges -> four simulated calls. Boundary/zero/overflow tests. C11 -Wall -Wextra -Werror normal and ASan/UBSan binaries both returned 0, output read_merge host tests passed. Exact blob hashes matched local git hash-object: read_merge.h 05980f7b962a8d072fb2ced88b326512bea11fe8; test_read_merge.c 906b784bd45b5ae5d046cc5858971cfc107b7daf. This tests the helper and simulated loop, not actual libnx SVC behavior.
- The experimental prerelease workflow now runs this fixture before building. No verified push-run/build/release result at record time; no Switch execution, no latency claims. Validate mapped-region boundary behavior and real bytes with D015/D017 joint tool, merge off/on, 20+ stable groups per mode. Keep pacing unchanged and compare calls, read_ticks, latency and failures; never enable by default until acceptance. 
