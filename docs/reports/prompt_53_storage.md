# Prompt 53 storage placement and S2 configuration

Scope: application storage and SDK configuration only. This work performed no
flashing, controller startup or physical motion. The installed core still owns no
heap allocation. Prompt54 retains runtime heap, allocation placement, service-gap,
ISR/driver and stop-timing qualification.

## SDK basis and placement boundary

The builds use ESP-IDF v5.5.5, pinned commit
`b774170ff46c393eeb5e495ea37936038d3f4f4f`, and its Xtensa GCC 14.2.0 toolchain.
The verifier checks the existing 43 reviewed SDK-source hashes, actual compilation
inputs, compiler target, resolved configuration, ELF/BIN and linked type sizes.

The example-only [StartupColdStorage](../../examples/common/StartupColdStorage.h)
allocates once after SDK startup, with `heap_caps_aligned_alloc` and explicit
`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`; absent initialized PSRAM or a refused external
allocation falls back once to `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`. Storage does
not move or grow. Fake-allocator tests cover successful external/internal branches,
both refusals, alignment, overflow, release and stable storage without later calls.

Only disconnected archival bytes use this helper. Owners, active contexts, receive
and transmit queues, reserved stop contexts, driver/ISR state and task storage stay
internal. The fixture selects capability-only PSRAM allocation and disables external
BSS, noinit and task stacks; `CONFIG_SPIRAM_IGNORE_NOTFOUND=y` permits internal
fallback at startup. Quad/40 MHz describes these compile configurations only.

This follows the SDK's cache-disabled accessibility and internal DMA/stack
restrictions, rather than treating PSRAM as interchangeable with internal RAM.
See [Espressif external RAM guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-guides/external-ram.html),
and pinned SDK `components/heap/include/esp_heap_caps.h`,
`components/esp_psram/Kconfig.spiram.common` and target PSRAM Kconfigs.

## Four real compile fixtures

`python scripts/check_firmware.py --target storage --temporary-root C:/esp/build
--build-dir .build/p53-storage` passed with `IDF_PY_BUILD_JOBS=8`.
Evidence directory: `.build/p53-storage/20261116T034644552052Z-45316-jhcumrlz/`.
Its `summary.json`, per-target SDK logs, compilation databases, resolved
`sdkconfig`, ELF/BIN/MAP and `storage-size.json` contain the actual results.

| Fixture | PSRAM configured | Static data + BSS bytes | IRAM code + vectors bytes | App BIN bytes |
| --- | --- | ---: | ---: | ---: |
| storage-s2 | No | 22968 | 40624 | 122912 |
| storage-s2-psram | Optional | 23040 | 41676 | 126368 |
| storage-s3 | No | 26608 | 49535 | 147712 |
| storage-s3-psram | Optional | 26680 | 50967 | 150832 |

Sizes are from the actual linked maps with the pinned `esp_idf_size --format json`;
the IRAM column includes the S3's separate IRAM and shared DIRAM instruction regions.
They are static image use, not free-heap or allocation measurements. All four images
contain a 32772-byte `ColdHistory` type (32768 payload bytes plus its count), a
24-byte allocation owner, a 1160-byte `IdfTwaiDriver` and 5152-byte `ActionContext`.
`HotStorage` is 13928 bytes on S2 and 13936 bytes on S3. Cold history is allocated
at runtime and is therefore outside those static data/BSS totals.

An initial unrestricted Ninja run exited `0xFFFFFFFF` without a compiler diagnostic
under parallel host load; its full evidence remains in
`.build/p53-storage/20261116T034423962544Z-37468-e0mus_9d/`. The bounded-jobs run above
compiled all four variants. Neither run executed allocation or address checks on
a board; both runtime-evidence flags remain false in the verifier summary.

## B01 S2 settings

The S2 bench and shared S2 defaults adopt the reviewed settings from the
[S2 memory study](2026-11-12_s2_memory.md): DCache 0 KiB without PSRAM, `-Os`,
non-ISR FreeRTOS and heap functions in flash, ring-buffer functions including its
ISR functions in flash, libc locks outside IRAM, and a 2048-byte ESP timer task
stack. TWAI cache-safe/IRAM ISR and UART IRAM ISR remain disabled. The owner task
stack is unchanged.

The SDK Kconfigs require that cache-disabled ISRs not call the moved code; this is
an application restriction, not a general driver guarantee. Pinned sources:
`components/esp_system/port/soc/esp32s2/Kconfig.cache`, `heap/Kconfig`,
`freertos/Kconfig`, `esp_ringbuf/Kconfig`, `newlib/Kconfig` and `esp_timer/Kconfig`.
See also [Espressif RAM guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s2/api-guides/performance/ram-usage.html).
The resolved SDK writes zero cache size as `CONFIG_ESP32S2_DATA_CACHE_SIZE=0`.
With ring-buffer ISR functions in flash, the UART IRAM option's dependency is
false and Kconfig omits `CONFIG_UART_ISR_IN_IRAM`; the strict gate requires that
absence rather than accepting any disabled-looking spelling.

The final comparison built both configurations from the same frozen source:
`.build/p53-b01/20261116T041410945780Z-20456-8gmodugo/`. Its `summary.json` contains
per-file source hashes, actual resolved configuration, map measurements and
`same_source_verified: true`. The final B01 image also passed the production
input/compiler/target/configuration checks against its actual compilation database.

| Same-source bench image | DCache bytes | IRAM code + vectors bytes | Data + BSS bytes | Map DIRAM remainder bytes | App BIN bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Previous SDK settings, `-Og` | 8192 | 54972 | 56604 | 60456 | 758480 |
| B01 settings, `-Os` | 0 | 29512 | 55392 | 95320 | 680304 |

The map remainder grows by **34864 bytes**: 25460 fewer instruction bytes, 1212
fewer data/BSS bytes and 8192 cache bytes returned to the region. This reproduces
the earlier study's configuration gain on the current code; it is not a measured
free-heap, largest-region or service-time result. The ESP timer task's configured
1536-byte stack reduction is separate from that static-image comparison. The
linked bench has no `esp_timer_create` symbol, but actual task high-water evidence
and cache-disabled execution/service gaps still belong to Prompt54.

Only this final pair supplies the comparison claim. Earlier disposable audits
remain in `.build/p53-b01/20261116T040431144817Z-29096-06nrq8q1/` and
`20261116T041039457419Z-51692-7zx5jys1/`: the first stopped on an incorrect BIN
basename in the audit script, and a later raw-source guard rejected a changed
console-header comment. SDK image builds linked; those incomplete audits are not
counted as same-source verification.

## B02 record sharing

The fresh source audit found raw, PDO, discovery, communication and heartbeat
collectors already sharing one arena. The old report's full Raw/PDO static-storage
saving had already been implemented and cannot be counted again.

The remaining change shares caller-owned `AcquisitionRecord` and `RouteRecord`
objects in that same internal arena. `DeviceEvidence` and `BenchRoute` borrow an
explicit live record pointer, while their admission IDs, time, grants, unresolved
write history, controlword history, session attachments and arming read storage
stay stationary. Null storage refuses admission. Binding another pointer or
detaching while an active or terminal record is held fails unchanged; only explicit
release permits the arena to destroy that record and construct a different kind.
The shared firmware's ordinary example retains a separate explicit acquisition
record because it does not use the bench's exclusive-record lifecycle.

The main loop detaches both idle borrowers before any kind-changing arena
destruction. Every raw/PDO/D/K/H/acquisition/route acquisition uses that boundary.
Neither an active record nor an unreleased terminal permits replacement. Record
bindings also reject overlap with the collector, application/owner or the route's
attached firmware, service store, sessions and scratch work.

Actual S2 linked DWARF from
`.build/p53-bench/20261116T040316351524Z-44956-e8o2ahr1/s2/`:

| Type | Bytes |
| --- | ---: |
| AcquisitionRecord | 14768 |
| RouteRecord | 38016 |
| DeviceEvidence, with borrowed record pointer | 40 |
| BenchRoute, with borrowed record pointer | 5408 |
| RawEvidence | 23592 |
| PdoExperiment | 24720 |
| FirmwareProbe | 17448 |
| AxisSession, including B03 sharing | 36960 |
| RouteWork | 8688 |

The arena is therefore 38016 bytes, enlarged from the existing 24720-byte maximum,
while the two 14768/38016-byte records no longer live in separate collector
objects. These layouts establish bounded storage costs. Padding, allocator
overhead, fragmentation and available heap regions remain separate from a payload
sum; the bench's actual free/largest/minimum heap still requires Prompt54.
The [session report](prompt_53_sessions.md) explains B03's narrower home/velocity
sharing and the native contexts that must remain separate.

Native regressions exercise real collectors, refusal while active and retained,
null detach/rebind, same-pointer binding, overlap rejection, monotonic IDs after
record replacement, and route control/fault/uncertainty history across changes.
The soak, silent-capture and watch allocations remain separate: their state and
tap/gate lifetimes require independent analysis before sharing them.

## Local verification

The four storage variants passed the real pinned-SDK build gate. The full bench
S2 and shared S2 images passed their actual input, compiler, target and resolved
configuration gates:

- `.build/p53-bench/20261116T040316351524Z-44956-e8o2ahr1/` (`--target s2`).
- `.build/p53-shared-s2/20261116T040541025869Z-27192-6dwky9nw/` (`--target idf-s2`).
- `.build/p53-b01/20261116T041410945780Z-20456-8gmodugo/b01/` (final frozen-source
  bench image, additionally checked after the comparison).

`storage_placement`, `bench_heap`, `device_evidence` and `bench_route` focused
native tests passed, including real collector lifetime/admission behavior. The
other affected firmware/route/soak/capture suites compiled and passed in focused
runs. `python test/firmware_build_test.py` passed 52 tests,
`python scripts/s2_flash_test.py` 77 and `python scripts/s2_smoke_test.py` 35, using
only fakes for flash/serial behavior. Strict Doxygen passed. Combined final
verification and exact-commit CI remain the lead's responsibility.
