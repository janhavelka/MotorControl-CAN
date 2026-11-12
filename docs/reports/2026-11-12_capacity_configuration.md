# Capacity configuration: a user-set axis count and optional PSRAM storage (2026-11-12)

Read-only study on `main` at `e73cb7b`. `include/`, `src/`, `adapters/` and `examples/` are identical to `e693f16`,
the image measured in the [S2 memory report](2026-11-12_s2_memory.md) (`git diff --stat e693f16 HEAD -- include
adapters src examples` is empty). No code, configuration, hardware or other file was changed. Unrelated uncommitted
work in the checkout was not touched: bench evidence under `docs/bench/20261112-final/`, planning edits in
`docs/prompts/` and `docs/backlog.md`, and prompts 51 to 54.

Sibling state: MotorControl-RS `cd27b12`, clean, read for `BusStorage` only. FieldCore-node `5ad9292d`, clean.
Both are unchanged since the [multi-drive preparation](2026-11-12_multidrive_preparation.md), so FieldCore was not
reread.

The user's requests (2026-11-12): "I don't want the capacity to be fixed at 2 - make it as large as the underlying
system/MCU allows" and "the library should allow PSRAM allocations if feasible (future S3 projects/PCBs) at compile
time to allow control of a user-set number of drives".

## Short answer

- **The installed C++11 core has no axis, node or channel capacity.** Its fixed arrays have protocol shapes (PDO
  entries, read fields, setup histories) and do not grow with the number of drives. `include/` and `src/` need no
  change for this request.
- **The "two" is in the adapters and the examples.** `SdoClient::CHANNELS = 2` drives the urgent slots, the TWAI
  TX slots and the discovery targets. `SEQUENCES = 2` and `ProbeApp::axes_[2]` are set to the same value by hand.
  `REQUESTS = 6`, `PRODUCERS = 4` and `PDO_ROUTES = 8` are separate. The bench is fixed at nodes 1 and 2 by its
  fixture.
- **Recommended mechanism:** one adapter capacity header. Its compile-time defaults equal today's values, and the
  application's build can override them: CMake cache variable, IDF Kconfig or PlatformIO build flag. `static_assert`
  and a link-time tag check the values. Storage stays inside the existing caller-owned objects, so the application
  decides where it lives. RS's runtime `BusStorage` spans are the alternative (section 2).
- **PSRAM needs no library change.** An S3 application allocates the large per-axis objects at startup with
  `heap_caps_aligned_alloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`, or uses `EXT_RAM_BSS_ATTR` in IDF builds.
  `IdfTwaiDriver`, the task stack and the TWAI SDK objects stay in internal RAM. `IdfTwaiDriver::start` already
  refuses storage outside internal RAM.
- **Memory per axis:** 38436 bytes today for a headless axis, 23748 after B03 (one operation slot in
  `AxisSession`). S2: 4 axes today, 5 with B01, 9 with B01 and B03. S3 without PSRAM: about 7, and 13 after B03.
- **With PSRAM, memory is no longer the limit.** The limits are then:
  - host TX: one SDK frame per service turn, about 1000 frames/s;
  - owner CPU per turn, which grows with roughly N²;
  - bus time;
  - 127 CANopen node IDs, or 125 with the owner's 8-bit slot index.

## Method

- Sizes come from `sizeof`/`alignof` compiled by the pinned Xtensa GCC 14.2.0 (`toolchain-xtensa-esp-elf`) for
  esp32s2 and for esp32s3, to assembly only. Nothing was linked or run.
- The probe used `-Dprivate=public` and lives in the session scratchpad, not in the repository.
- S2 and S3 give identical sizes: both have 4-byte pointers and 8-byte `uint64_t` alignment. They match the DWARF
  numbers of the S2 memory report: `AxisSession` 35752, `CanBusOwner` 13464, `SdoClient` 11056. Native 64-bit host
  sizes differ, because pointers there are 8 bytes.
- S2 heap budgets are quoted from the S2 memory report (linked and board-measured).
- S3 budgets are computed from the p42 S3 compile fixture's map (`.build/p42-firmware/.../idf/motorcontrol_can_probe.map`:
  `_heap_start = 0x3FCA7E60`) and the IDF 5.5.5 S3 region table. No S3 board exists on this bench.
- IDF facts are from the local SDK at `C:\pio\packages\framework-espidf` (5.5.5). Paths below are relative to
  `components/`.

## 1. Inventory of fixed capacities

"Adapter API" means visible in an adapter or example header that applications include. These headers are built from
source by every application and are not in the installed package (`library.json` exports `include/` and `src/` only).
"Private" means storage behind such a header.

### Installed core

| Item | Where | Value | What it sizes | Kind |
| --- | --- | --- | --- | --- |
| `Endpoint::address`, `channel`, `subaxis` | `include/MotorControlCAN/Endpoint.h:29-31` | u32, u16, u16 | Route identity; "no common 1..127 restriction" | Contract; not a count |
| `PDO_ENTRIES` | `include/MotorControlCAN/canopen/Pdo.h:14` | 8 | Entries of one PDO map | Protocol shape |
| `SdoTransfer::data[32]` | `canopen/SdoTransfer.h:49`, `:77` | 32 | One transfer's payload | Protocol shape |
| Per-operation arrays (`ReadProvenance fields[n]`, `setupHistory[n]`, `commandHistory[3]`, `steps[3]`) | `profiles/lichuan_c/*.h` | 2 to 15 | One read or action | Protocol shape, per axis |
| CANopen node 1..127 | `canopen/Network.h:42-47`, `canopen/Sdo.h:24` | 1..127 | NMT and SDO targets | Protocol limit |

Static RAM of `libMotorControlCAN.a`: zero (S2 memory report).

### SDO adapter (`adapters/canopennode/SdoClient.h`)

| Item | Where | Value | What it sizes | Bytes per unit | Kind |
| --- | --- | ---: | --- | ---: | --- |
| `CHANNELS` | `:50` | 2 | `channels_[]` (`:720`). Each channel holds one CANopenNode SDO engine, staging port, route, node snapshot (heartbeat, boot-up, EMCY) and drain history. Each channel anchors one node, and channels are never rebound (Q18) | 1520 | Adapter API constant; private storage |
| `PDO_ROUTES` | `:52` | 8 | `pdos_[]` (`:761`), permanently bound PDO wire routes | 856 | Adapter API constant |
| `NETWORK_HISTORY` | `:51` | 4 | Passive/unknown frame ring (`:723`) | 200 | Independent of N |
| `Channel::RETAINED_PAIRS` | `:686` | 4 | Drain history per channel | 4 | Per channel, not a count |
| `SdoRequestId::channel` | `:19` | `uint8_t` | At most 255 channels | n/a | Adapter type layout |
| Whole object | | | 11056 in total, of which 1168 is base | | Caller-owned. `FirmwareProbe` borrows it; the firmware places it `static DRAM_ATTR` (`examples/common/EspProbeMain.cpp:38`) |

### Owner (`adapters/owner/CanBusOwner.h`, `CanBusTypes.h`)

| Item | Where | Value | What it sizes | Bytes per unit | Kind |
| --- | --- | ---: | --- | ---: | --- |
| `REQUESTS` | `CanBusOwner.h:21` | 6 | Shared ordinary request/result slots, `slots_[0..5]` (`:349-358`) | 432 | Adapter API constant |
| `URGENT_REQUESTS` | `:22` | `= CHANNELS` | One reserved urgent slot per channel: stops and best-effort `000B` | 432 | Derived |
| `SEQUENCES` | `:24` | 2 | `sequences_[]` (`:344-348`), logical-axis reservations | 80 | Adapter API constant, equal to `CHANNELS` by hand |
| `PRODUCERS` | `:25` | 4 | Fairness producers 1..4; no storage, only the `select()` loop | 0 | Adapter API constant |
| `external_[CHANNELS]` plus the NMT handoff | `:359-363` | 2 + 1 | TX handoff retirement | 36 | Private |
| `active_[CHANNELS] = {-1, -1}` | `:375` | 2 | In-flight slot per channel | 4 | Private. **The initializer is written for exactly two entries.** With more channels, the extra entries start at 0, which means "slot 0 active". With one channel it does not compile |
| `health_[CHANNELS]` | `:377-382` | 2 | Per-binding health | 112 | Private |
| `TRACE_CAPACITY` | `:30` | 24 | Diagnostic ring `trace_[]` (`:384`), sized to one turn's records | 384 | Independent of N |
| `RX_LIMIT`, `TX_LIMIT`, `EVENT_LIMIT`, `START_LIMIT` | `:26-29` | 8, 2, 8, 2 | Work per service turn | n/a | Throughput limits, independent of N |
| `RequestId::slot`, `SequenceId::slot` | `CanBusTypes.h:94`, `:100` | `uint8_t` | At most 255 slots of each kind | n/a | Adapter type layout |
| `OwnerResources` counts, `ServiceBudget`, `traceHead_`, `nextChannel_` | `CanBusTypes.h:199-210`, `:263-271`; `CanBusOwner.h:374`, `:385` | `uint8_t` | At most 255 | n/a | Adapter types |
| Whole object | | | 13464: trace 9216, slots 3456 | | Member of `FirmwareProbe` |

### TWAI adapters (`adapters/twai`)

| Item | Where | Value | What it sizes | Bytes per unit | Kind |
| --- | --- | ---: | --- | ---: | --- |
| `TwaiAdapter::TX_CAPACITY` | `TwaiAdapter.h:87-91` | `CHANNELS + 1` = 3 | Software TX slots `slots_[]` (`:136`) | 64 | Derived |
| `IdfTwaiDriver::RX_CAPACITY` | `IdfTwaiDriver.h:24-26` | 16 | RX ring filled by the ISR callback. Kconfig `MOTORCONTROLCAN_CAN_RX_DEPTH` allows 1..16 (`examples/probe_idf/main/Kconfig.projbuild:19-22`) | One `ReceivedFrame` | Internal RAM only |
| One SDK frame in flight | `adapters/twai/README.md:33-34`; `TwaiAdapter::promote` (`TwaiAdapter.cpp:148-187`); `FirmwareProbe::service` (`FirmwareProbe.cpp:203-208`) | 1 per service turn | Host TX rate | n/a | Design (exact TX retirement) |

### Shared application (`examples/common`)

| Item | Where | Value | What it sizes | Bytes per unit | Kind |
| --- | --- | ---: | --- | ---: | --- |
| `ProbeApp::axes_[2]` | `ProbeApp.h:359` | 2 | Open session pointers. A third session is refused (`ProbeApp.cpp:156`) | 4 | Private |
| Producer 1 for every session and read | `ProbeApp.cpp:159`, `:239` | 1 | Fairness identity (M06) | n/a | Implementation |
| `DiscoveryPlan::CAPACITY = CHANNELS` | `DiscoveryScan.h:63-67`, `:182-184` | 2 | Targets and retained findings. Stack copies in `DiscoveryScan.cpp:41-42` and `ProbeConsole.cpp:2269` | 2248 per target: target 68, finding 2176, session counter 4 | Adapter API constant (console `discover` grammar) |
| `ProbeDevice` slot checks | `ProbeDevice.cpp:39`, `:59-78` | Below `CHANNELS` | Slot selection | n/a | Follows the constant |
| `ProbeConsole` `OPERATION_CAPACITY` 2, `TOTAL_OPERATION_CAPACITY` 5, `stopReplies_[2]`, one `motion_` | `ProbeConsole.h:85-94`, `:194`, `:198`, `:210-212`; `ProbeConsole.cpp:561-566`, `:1118` | One attached axis with one HALT and one quick-stop slot | Console correlations | Operation 592, record 524; one per-axis stop pair is 2232 | Example API (M03) |
| `ProbeMotion::stops_[2]` | `ProbeMotion.h:297` | 2 | Normal and fast stop of one axis | 1168 | Per axis, not a count |
| `AxisSession` `stops_[2]`, `moveReads_[2]`, `retained_[4]` | `AxisSession.h:697`, `:734`, `:742` | n/a | One axis's operation contexts | 35752 per session | Per axis (B03) |
| `FirmwareSmoke::nodes[2]` | `FirmwareSmoke.h:12`; Kconfig `SMOKE_NODE_A`/`_B` | 2 | Smoke fixture | n/a | Fixture |
| `hostWiring_[10]`, console line/record/platform buffers | `ProbeApp.h:357`, `ProbeConsole.h`, `ProbePlatform.h` | Various | Terminals and text | n/a | Not axis-related |

### Bench (`examples/s2_bench`, the two-drive fixture)

| Item | Where | Value | Kind |
| --- | --- | --- | --- |
| `BenchRoute::sessions_[2]`, `attached(node)` accepts only `node >= 1 && node <= 2`, `axis()` indexes `node - 1` | `BenchRoute.h:801-805`, `:920`, `:987-988`, `:1035` | Nodes 1 and 2 | Fixture |
| Per-node `[2]` arrays: limits, I/O, rx counters, move axes and references, trigger/halt, commanded, `motionCases_`, `settingsCases_`, `modeAdmitted_`, `emcyBaseline_` | `BenchRoute.h:676-704`, `:931-982` | 2 | Fixture |
| Route record `ROUTE_TERMINALS` 128, `ROUTE_TRAFFIC` 448, `ROUTE_TAIL` 16; caps `ROUTE_MOTION_TOTAL` 12, `ROUTE_MOTION_PER_CASE` 3 | `BenchRoute.h:470-494`, `:606-608` | One record shared by every node of a route | Deliberate evidence bound and safety budget |
| `BenchConsole::axes_[2]`, `budgets_[2]`, `emcyBaseline_[2]` | `BenchConsole.h:220-230` | 2 | Fixture |
| `RESERVE_SESSION1`/`2`, `placedSessions[2]`, `target(node)`: logical axis = node, slot = node − 1, COB-IDs node + 0x600, 0x580, 0x700 and 0x80 | `main/main.cpp:96-126`, `:656`, `:764-772` | 2 | Fixture |
| `CONSOLE open slot0=node1 slot1=node2` | `main/main.cpp:1705` | 2 | Fixture text |

## 2. Mechanism: compare with MotorControl-RS

### What RS does

- **`BusStorage`** (`examples/common/RtuBusOwner.h:90-114`): caller arrays `PendingSlot* pending`,
  `ResultSlot* results` and `ProducerSlot* producers`, each with a count. Also `urgentPendingCapacity` and
  `urgentResultCapacity`, the reserved last slots. The fields are private while owned.
- **Constructor** (`RtuBusOwner.cpp:18-41`) rejects any of these, and the owner then stays `valid() == false` and
  refuses all work:
  - capacities above 65535, or zero producers;
  - urgent quotas above the totals;
  - null arrays;
  - any overlap between the arrays, the owner and the runner.
- **Application** (`examples/probe_cli/ProbeApp.cpp`): declares the arrays inside its `App` struct with `constexpr`
  capacities (`REQUEST_CAPACITY = 8`, `ORDINARY_PENDING = 4`, `STOP_RECORDS = 2`, `:51`, `:87-89`) and builds the
  view in `busStorage()` (`:73-77`).
- **Placement**: the whole `App` is allocated once at startup with
  `heap_caps_malloc(sizeof(App), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` plus placement new (`:3087-3099`).
  There is no internal-RAM fallback (`ProbeApp.h:24-27`).
- **RS sdkconfig**: `CONFIG_SPIRAM=y`, octal, 80 MHz, `SPIRAM_USE_MALLOC`,
  `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=n` (`examples/probe_idf/sdkconfig.defaults:8-14`).
- **Adopted** from RS: the application chooses the capacity, a reserved urgent quota that ordinary work cannot
  consume, explicit refusal at exhaustion, placement by the application at startup, and a boot failure line when
  placement fails.
- **Rejected** from RS: a hard PSRAM requirement with no internal option. The CAN audit already records this
  (`docs/audit/MOTORCONTROL_RS_AUDIT.md:540`).

### Options for this library

| Option | For | Against | Verdict |
| --- | --- | --- | --- |
| Template parameters on `SdoClient` and `CanBusOwner` | Compile-time sizes | About 3200 lines of adapter `.cpp` would move into headers or need explicit instantiation. The template type spreads to every holder of `SdoClient&` or `CanBusOwner&`: `ProbeApp`, `AxisSession`, `DiscoveryScan`, `ProbeDevice`, `FirmwareProbe`, `TwaiAdapter` and the bench. MAINTAINABILITY prefers typed values over template machinery | Reject |
| **Configuration header: macros mapped once into `constexpr` constants** | Defaults reproduce today's layout byte for byte. About 260 `SdoClient` and 64 `CanBusOwner` constructions in tests and examples stay unchanged. `static_assert` checks ranges and relations at compile time. Same plumbing as the existing `MOTORCONTROLCAN_CAN_*` board facts (`examples/probe_idf/main/CMakeLists.txt:27-34`, `platformio.ini`). The core is untouched | One capacity set per program. An ODR hazard if two translation units see different values: native CMake builds the adapters as separate static libraries. Each N needs its own test build | **Recommended**, with PUBLIC definitions and a link tag |
| Caller-provided spans (RS `BusStorage`) | Consistent with RS. Capacity per interface. Hot and cold storage can be placed separately. One test binary covers every N | Every construction changes, or a holder template is needed. Every public call gains a runtime "invalid owner" state. The owner and `SdoClient` spans must agree at runtime. Per-channel CANopenNode engines must still be stationary | Upgrade path if per-interface capacity or split placement becomes a requirement. At N = 32, owner and `SdoClient` together are about 103 KB, which fits S3 internal RAM |
| Storage the application instantiates | Natural for per-axis objects | n/a | Already in place: `AxisSession`, `ProbeMotion` and `BenchAxis` are caller-owned, and `ProbeApp::openAxis` borrows them. Keep it |

Backlog M04 says "after RS `BusStorage`". This recommendation adopts `BusStorage`'s semantics but not its
runtime-span form, so the lead should record that as a decision, or choose spans.

### Proposed configuration (names are proposals)

| Macro | Default | Range and checks | Drives |
| --- | --- | --- | --- |
| `MOTORCONTROLCAN_SDO_CHANNELS` | 2 | 1..125, checked by `static_assert` | `SdoClient::CHANNELS`, `CanBusOwner::SEQUENCES` and `URGENT_REQUESTS`, `TwaiAdapter::TX_CAPACITY` (+1), `ProbeApp` sessions |
| `MOTORCONTROLCAN_OWNER_REQUESTS` | max(6, channels + 4) | channels + requests ≤ 255 (`RequestId::slot`) | `CanBusOwner::REQUESTS` |
| `MOTORCONTROLCAN_OWNER_PRODUCERS` | max(4, channels + 2) | 1..255 | `CanBusOwner::PRODUCERS`; one producer per session (M06) |
| `MOTORCONTROLCAN_PDO_ROUTES` | 8 | 1..255 | `SdoClient::PDO_ROUTES`; independent of channels |
| `MOTORCONTROLCAN_DISCOVERY_TARGETS` | min(channels, 2) | 1..channels | `DiscoveryPlan::CAPACITY`. Larger scans run in batches over existing slots; this avoids 2248 bytes per channel |

- **Derived values.** `SEQUENCES` is set equal to `CHANNELS`. A binding has one usable reservation, and its channel
  is fixed, so extra reservations cannot be used. The values are equal today too.
- **What stays fixed.** `TRACE_CAPACITY`, the per-turn limits and `NETWORK_HISTORY` stay fixed, because they are
  per-turn or diagnostic bounds, not per-axis ones.
- **Link tag.** `SdoClient.cpp` defines one `extern "C"` symbol whose name encodes the values, for example
  `motorcontrolcan_capacity_c2_r6_p4_o8_d2`. The header references it from an internal pointer marked
  `__attribute__((used))`; MSVC uses `#pragma detect_mismatch`. A translation unit compiled with other values then
  fails to link instead of running with a different layout.
- **CMake.** Cache variables, set with `target_compile_definitions(canopennode_sdo PUBLIC ...)` in
  `adapters/canopennode/CMakeLists.txt`. The owner, `examples/common` and the tests inherit them.
- **IDF.** Integers in the application's `Kconfig.projbuild`, set on `${COMPONENT_LIB}` beside the board
  definitions. The adapters are compiled in the same component (`examples/common/firmware_sources.json`).
- **PlatformIO.** `build_flags`, which apply to every source of the environment.

## 3. PSRAM placement

### What must stay in internal RAM

| Object | Placement | Reason and evidence |
| --- | --- | --- |
| `IdfTwaiDriver`: RX ring, TX frame, `portMUX` and the SDK callback context | Internal, `static DRAM_ATTR` as today | TWAI ISR callbacks write it (`rxDone`/`txDone`, registered with `this` at `IdfTwaiDriver.cpp:105`). `start` refuses an object that is not entirely internal (`IdfTwaiDriver.cpp:58-59`, `esp_ptr_internal`). Existing rule: "Keep the callback context and ISR storage in internal RAM" (`adapters/twai/README.md:26-29`, `docs/adapters/twai_sdk.md:77`, `:126`, `docs/adapters/twai_esp32s2.md:43`, `IdfTwaiDriver.h:13-14`). The exact phrase "SDK callback storage never resides in optional PSRAM" is not in the repository; these lines carry the rule. The SDK itself puts the node in internal RAM "because it contains atomic variable" (`esp_driver_twai/esp_twai_onchip.c:684-685`) |
| TWAI SDK TX queue and event group | Chosen by the SDK | They use `TWAI_MALLOC_CAPS = MALLOC_CAP_DEFAULT` unless `TWAI_OBJ_CACHE_SAFE` is set (`esp_driver_twai/twai_private.h:50-54`). Only `TWAI_ISR_IN_IRAM` or `TWAI_IO_FUNC_IN_IRAM` select that, and the adapter forbids `ISR_IN_IRAM` (`IdfTwaiDriver.cpp:17`). On S3, `DEFAULT` is served from internal DRAM first, and from PSRAM only when internal is exhausted (`heap/port/esp32s3/memory_layout.c:63-65`). Start the node before internal RAM fills up, and check the internal free size afterwards |
| Owner task stack and TCB | Internal, `static DRAM_ATTR` | PSRAM stacks need `FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`, and the stack must never be used while the cache is disabled (`freertos/Kconfig:585-596`). Today: `EspProbeMain.cpp:23-24`, bench `main.cpp:38-39` |
| `CanBusOwner`, `SdoClient`, `TwaiAdapter`, `EspProbePlatform` | Internal preferred, PSRAM permitted | No ISR touches them; they run in the cooperative context only. The owner and the SDO adapter are scanned every turn, and at N = 2 to 32 they are 25 to 105 KB, comparable to or larger than the 32 KB data cache |
| `AxisSession`, `ProbeMotion`, `BenchAxis`, `ProbeConsole`, `ProbeDevice`, `DeviceEvidence`, route records, the arena, soak and capture | PSRAM candidates | Large, mostly cold and used only by the owner task |

### How an S3 application places storage (application code, never the core)

1. **Startup heap (recommended for IDF and Arduino).**
   - Before the controller starts, reserve blocks with
     `heap_caps_aligned_alloc(alignof(T), sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
   - Construct each with placement new and never free it.
   - The existing `BenchAllocator`/`reserveLargestFirst` interface (`examples/s2_bench/BenchHeap.h:17-50`) already
     takes such an allocator. Only the capability word differs from S2's `internalAllocate` (`main.cpp:662-664`).
   - A null block is reported and its axis is refused.
2. **Static PSRAM (IDF only).**
   - Declare `EXT_RAM_BSS_ATTR alignas(8) static uint8_t memory[N][sizeof(AxisSession)];` and use placement new.
   - It needs `CONFIG_SPIRAM=y` and `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`
     (`esp_common/include/esp_attr.h:140-145`, `esp_psram/Kconfig.spiram.common:137-148`). IDF 5.5.5 uses this name;
     the older `CONFIG_SPIRAM_ALLOW_BSS_EXT_MEM` is not in its rename tables.
   - Without that option the attribute expands to nothing, and the storage silently stays internal.
   - The pinned Arduino S3 libraries (`framework-arduinoespressif32-esp32s3-libs` 3.3.12) leave it unset. They have
     `SPIRAM_USE_MALLOC`, `MALLOC_ALWAYSINTERNAL=4096`, `MALLOC_RESERVE_INTERNAL=0`, quad PSRAM at 80 MHz, and
     initialize PSRAM at run time.
   - The section is NOLOAD and is zeroed by `esp_psram_bss_init` (`esp_psram/system_layer/esp_psram.c:767-772`,
     called at `esp_system/port/cpu_start.c:771`), before the global constructors (`esp_system/startup.c:207`).
     Placement new removes any dependence on that order and on constant initialization.
3. **Not recommended: plain `new`/`malloc` with `SPIRAM_USE_MALLOC`.** Placement would then depend on
   `SPIRAM_MALLOC_ALWAYSINTERNAL` (16384 in IDF, 4096 in Arduino) and on the fallback order. Explicit capabilities
   keep the placement reviewable.
4. **Boot evidence.** Extend the existing `HEAP` line with `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`, and mark
   each reserved block internal or external (`esp_ptr_external_ram`), so that a record shows where storage went.

### Latency and cache

- **Cache.** S3 reads PSRAM through the shared data cache: 32 KB with 32-byte lines, both in the fixture's sdkconfig
  and in the Arduino libraries. A miss fetches a line over the SPI/OPI bus. A per-turn scan larger than the cache
  misses on most lines.
- **Placement rule.** Keep the state scanned every turn in internal RAM: owner, `SdoClient`, `TwaiAdapter`, driver and
  stack. Put cold evidence in PSRAM. Before stating any bound, measure `maxServiceGapUs` and the route `gap=` figure
  with both placements.
- **Cache-disabled windows.** Flash writes disable the cache; none is linked today. Non-IRAM tasks wait either way.
  No IRAM ISR touches application storage, because `TWAI_ISR_IN_IRAM` is off in `COMMON_SDKCONFIG`
  (`scripts/check_firmware.py:197-198`).
- **Pins.** PSRAM uses the MSPI pins (GPIO26..32, and 33..37 for octal PSRAM). `firmwareSystemPin` already reserves
  GPIO26..32 and the MSPI bus (`docs/firmware_examples.md:55-58`).

### Identical semantics on S2 and S3

- **Same build inputs.** Both targets use the same capacity values, types and code paths.
- **Only the allocator differs.** It is one firmware-layer choice: `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` on S2 and
  on S3 without PSRAM, and `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` on S3 with PSRAM.
- **Same refusal.** A refused block is reported and its axis is refused the same way on both targets, as
  `TRIAL_REFUSED reason=memory` does today. There is no silent fallback and no behavior that exists only on S3.
- **S2 constraint.** The S2 must reserve sessions from the heap at boot, largest first. Its statics end at
  `0x3FFDE000`, with 46776 bytes left in the bench image, so a static array of two or more sessions does not fit.

## 4. Axes that fit, per target

Growth of the adapters per added channel above two:

| Part | Bytes |
| --- | ---: |
| SDO channel | 1520 |
| Urgent slot | 432 |
| One more ordinary slot | 432 |
| Health record | 112 |
| Reservation | 80 |
| TWAI TX slot | 64 |
| Handoff record | 36 |
| Active index | 4 |
| `ProbeApp` pointer | 4 |
| **Total per channel** | **2684** |

Per-axis profiles:

| Profile | Contents | Today | After B03 |
| --- | --- | ---: | ---: |
| H, headless | Session plus adapter growth | 38436 | 23748 |
| C, console motion | H plus `ProbeMotion` 3528 and one stop pair 2232 | 44196 | 29508 |
| C+P | C plus four TPDO routes, 3424 | 47620 | 32932 |

Both bounds below are applied:

- **Count bound:** the application budget divided by the per-axis profile.
- **Contiguity bound:** each session must be one block. On S2 it must fit in heap region r2; on S3 in the main
  region.

The table gives the smaller of the two.

| Target and configuration | H | C | C+P | Basis |
| --- | ---: | ---: | ---: | --- |
| S2, today's SDK configuration | 4 | 4 | 4 | Budget 244651 minus the lean fixed part 47080 (S2 memory report). r2 is 169648, so it holds 4 sessions of 35752 |
| S2 with B01 (`-Os`, b2) | 5 | 5 | 4 | Budget 279515. r2 is 204512, so 5 sessions |
| S2 with B01 and B03 | 9 | 7 | 7 | Same budget. r2 holds 9 sessions of 21064 |
| S2 bench image | 2 | 2 | 2 | Fixture. With B01, B02 and B03 there is memory for about four more axes (S2 memory report), but the routes address nodes 1 and 2 only |
| S3 without PSRAM, today | 7 | 7 | 6 | Computed heap about 312000 (see the list below). The main region holds 7 sessions of 35752 |
| S3 without PSRAM, with B03 | 13 | 10 | 9 | Same heap. Contiguity allows 13 sessions of 21064 |
| S3 with 2 MB PSRAM | 45 or more | About 45 | About 40 | Per-axis objects go to PSRAM; internal RAM keeps 2684 per axis. The other limits below apply first |
| S3 with 8 MB PSRAM | About 125 | About 125 | About 125 | Bound by the slot index, not by memory |

How the S3 heap of about 312000 bytes is computed:

| Region | Range | Bytes |
| --- | --- | ---: |
| Main heap | `0x3FCA7E60`..`0x3FCE9710` | 268464 |
| Startup-reclaimed area | `0x3FCE9710`..`0x3FCF0000` | 26864 |
| Spare data-cache bank (only with DCache 32 KB) | `0x3FCF0000`..`0x3FCF8000` | 32768 |
| SDK run-time use, estimated (stacks of 2 idle tasks, 2 IPC tasks, `esp_timer` and the timer task, plus TCBs and drivers) | | about −16000 |

The p42 S3 fixture's statics already include the lean fixed part (`FirmwareProbe`, `SdoClient`, console, device,
stack), so it is not subtracted again.

Limits that do not depend on memory, on every target:

| Limit | Value | Source |
| --- | --- | --- |
| Host TX | `promote` submits at most one SDK frame per service turn, about 1000 frames/s at the 1 ms turn. SDO polling with the bench's seven-field cycle every 50 ms saturates this at about **7 axes per interface**. A stop of N axes needs at least N turns to hand off, plus each channel's in-flight exchange (about 3 ms typical, at most the 250 ms response budget) | `FirmwareProbe.cpp:203-208`, `TwaiAdapter.cpp:148-187`; the multi-drive report's own table assumed `START_LIMIT` 2 per turn |
| Bus | About 13 SDO axes at 50 ms, or 40 with one TPDO every 10 ms, at 50% bus load. TPDO sample provenance is still unqualified (D11, D15) | Multi-drive report section 2 |
| CANopen | Node IDs 1..127. The default SDO COB-IDs are node + 0x600 and node + 0x580 | Core |
| Owner slot index | `uint8_t`: with `REQUESTS = channels + 4`, channels ≤ 125 | `CanBusTypes.h:94` |

## 5. Implementation plan, in small reviewable steps

Each step is its own commit with its own checks, and has no hardware dependency unless stated.

1. **Capacity header with today's defaults; no behavior change.**
   - Files:
     - add `adapters/canopennode/Capacity.h`;
     - `SdoClient.h:50-53`;
     - `CanBusOwner.h:20-31`, with `SEQUENCES = CHANNELS`;
     - `CanBusOwner.h:375`: initialize `active_` in the constructor instead of `{-1, -1}`;
     - `TwaiAdapter.h:90`, `DiscoveryScan.h:63-65`;
     - `ProbeApp.h:359`: `axes_[CanBusOwner::SEQUENCES] = {}`;
     - the link tag in `SdoClient.cpp`;
     - the cache variables in `adapters/canopennode/CMakeLists.txt`.
   - Tests: the existing suite must pass unchanged at the defaults. `firmware_probe_test` must still print
     `SdoClient` 11056, `CanBusOwner` 13464 and `AxisSession` 35752 on target.
   - Gates:
     - `python scripts/verify.py --mode quick`;
     - `python scripts/check_firmware.py --target all`;
     - `python scripts/check_stack.py --strict`, which should show unchanged frames.
2. **Host tests for N = 1, 2, 8 and 32.**
   - Files: `test/capacity_test.cpp`, and a `test/CMakeLists.txt` function that compiles the adapter,
     CANopenNode and `examples/common` sources into one object library per N with the same PUBLIC definitions. Tests
     `capacity_n1` to `capacity_n32`.
   - Contracts:
     - all N routes configure, and slot N is refused;
     - one reservation per binding, up to N;
     - N urgent stops are accepted while every ordinary slot is full (`RESULTS_FULL` only for ordinary work), and
       `admitBestEffortStop` works per channel;
     - producers rotate fairly across `PRODUCERS`, and producer `PRODUCERS + 1` is refused;
     - `TwaiAdapter` has N + 1 TX slots;
     - `ProbeApp` opens N sessions and refuses one more;
     - discovery runs in batches;
     - N concurrent relative moves, with a HALT on one axis that leaves the others untouched (pattern of
       `bothNodesUrgent` in `move_relative_test.cpp`).
   - Plus a mismatched translation unit that must fail to link (WILL_FAIL).
   - Gate: `verify.py --mode full`, both compilers and the sanitizers. Cost: three more adapter builds.
3. **Owner turn cost.**
   - Measure host time per `service()` at N = 32 with every channel queued. The count is roughly
     (urgent + ordinary slots) × (4 channel scans + all slots) in `select`/`eligible`
     (`CanBusOwner.cpp:443-496`).
   - Then estimate the Xtensa time. If it is material, cache the channel index per slot at admission (a binding's
     channel is fixed), and rerun the step-2 tests unchanged.
4. **Placement and the S3 PSRAM fixture.**
   - Code: the allocator choice and the boot placement report in `examples/common/EspProbeMain.cpp`, with
     `BenchHeap.h` reused as it is.
   - Fixture: `examples/probe_idf/sdkconfig.defaults.esp32s3_psram`, compile only, with no board claim:
     - `CONFIG_SPIRAM=y`;
     - `SPIRAM_USE_CAPS_ALLOC`, so that `malloc` stays internal;
     - `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`, for one `EXT_RAM_BSS_ATTR` check;
     - `SPIRAM_IGNORE_NOTFOUND` not set.
   - Gate wiring: a new `idf-s3-psram` entry in `SHARED_IDF_BUILDS` (`scripts/check_firmware.py:225-237`) with
     reviewed keys and N = 8, its CI step in `.github/workflows/verify.yml`, and its `compile_commands.json` added to
     the stack gate.
   - Docs: the internal-RAM rule table above goes into `adapters/twai/README.md`, `docs/firmware_examples.md` and
     `docs/can_owner.md`.
   - This step is a major milestone, so verify CI on its final commit.
5. **S2 savings, one commit each.**
   - B01: the sdkconfig fragment for `examples/s2_bench` and `sdkconfig.optak_s2`, plus the `BENCH_SDKCONFIG` keys.
     It needs one boot, `M` and a short regression in prompt 54.
   - B03: the single operation slot in `AxisSession`, under the existing host tests.
   - B02: the bench arena.
6. **Per-axis application parts (prompt 53 items 3 and 5).**
   - One producer per session in `ProbeApp::openAxis` (M06).
   - Console motion attachments and stop pairs per axis behind `MOTORCONTROLCAN_CONSOLE_MOTION_AXES`, default 1
     (M03).
   - The group stop then uses the per-channel urgent slots, which grow with N automatically.
7. **Bench.**
   - Replace the literal `[2]`s and `node <= 2` with one `BENCH_NODES = 2`, and add
     `static_assert(BENCH_NODES <= SdoClient::CHANNELS)`.
   - A build with N > 2 then still compiles the bench unchanged.
   - Keep the fixture, the caps and the runner rules until a fixture with more drives is recorded.

Gates not named above stay as they are: `test/package_test.py` and the installed consumer (the core is unchanged),
`python scripts/check_promptset.py` and `git diff --check` for the documentation, and strict Doxygen for the
installed headers.

## 6. Defaults that keep today's builds unchanged

| Constant | Today | Default |
| --- | ---: | --- |
| `SdoClient::CHANNELS` | 2 | 2 |
| `CanBusOwner::SEQUENCES` | 2 | `= CHANNELS`, which is 2 |
| `URGENT_REQUESTS` | 2 | `= CHANNELS` |
| `REQUESTS` | 6 | max(6, channels + 4), which is 6 |
| `PRODUCERS` | 4 | max(4, channels + 2), which is 4 |
| `PDO_ROUTES` | 8 | 8 |
| `DiscoveryPlan::CAPACITY` | 2 | min(channels, 2), which is 2 |
| `TwaiAdapter::TX_CAPACITY` | 3 | channels + 1, which is 3 |
| `ProbeApp` sessions | 2 | `SEQUENCES` |
| `NETWORK_HISTORY`, `TRACE_CAPACITY`, per-turn limits, RX ring | 4, 24, 8/2/8/2, 16 | Unchanged, not configurable |
| Console motion attachments | 1 | 1 |

## 7. Risks

| Risk | Detail and mitigation |
| --- | --- |
| ABI | The installed core is unchanged. Adapter and example layouts change only when an application sets a value, and the whole application is then rebuilt from source. The link tag catches mixed builds. Default sizes stay identical |
| ODR in native builds | Adapters are separate static libraries. Use PUBLIC definitions on `canopennode_sdo` and the link tag; never set the values on single targets |
| Index widths | `RequestId::slot`, `SequenceId::slot`, `SdoRequestId::channel`, `OwnerResources` and `ServiceBudget` are `uint8_t`. Check with `static_assert`; widening them is an adapter type change |
| Owner CPU per turn | The scans grow with about N². An estimate gives a small cost at N = 8, but N = 32 could approach the 1 ms turn when every channel is queued. Measure (step 3) before claiming large N |
| Host TX rate | One SDK frame per turn bounds SDO monitoring (about 7 axes at 50 ms) and group-stop handoff (at least N turns). Going further needs TPDO monitoring (M08) or a new TX design with its own retirement proof |
| RX ring | 16 ISR entries and `RX_LIMIT` 8 per turn. At N = 32 with a 10 ms TPDO per axis, a 4 ms service gap brings about 13 frames. An overflow inhibits TX, which is conservative but stops service. Measure, and raise the Kconfig maximum only with internal-RAM evidence |
| Bus time and evidence | See section 4. Each N is a capacity count, not timing, bus-load or service-gap qualification (prompt 54) |
| Test cost | Three extra adapter builds in the full mode. The existing tests hard-code two nodes (for example `pdo_observations_test.cpp:281`, the bench and console tests) and stay at the default configuration |
| Stack | The stack copies in `DiscoveryScan.cpp:41-42` and `ProbeConsole.cpp:2269` grow by 5 bytes per discovery target. Run the stack gate on the larger fixture too |
| PSRAM | Wrong placement when `EXT_RAM_BSS_ATTR` silently stays internal. TWAI SDK objects can fall back to PSRAM when internal RAM runs out. Cache-miss latency. Boot memory-test time. No S3 board exists yet, so step 4 is compile-only |
| CANopen | 127 node IDs per interface. A multi-axis drive may need several channels on one node; the owner already keeps such peers together (`peersResolved`, `CanBusOwner.cpp:81-88`) |

## Limits of this report

- Sizes are compiler `sizeof` values. Heap blocks add a few bytes of header and alignment each.
- S3 budgets are computed from a map and the SDK region table, with an estimated SDK use of about 16 KB. They are not
  measured on a board.
- The owner turn cost at large N is an estimate from loop structure. It needs the step-3 measurement.
- The host-TX bound assumes one service turn per 1 ms tick, as reported in the multi-drive preparation. It was not
  re-measured here.

**Correction (2026-11-12, later the same day).** The host-TX figure of about 1000 frames/s is tick arithmetic, not a measurement. Retained night traces show that during SDO work a service pass ran only about every 3 ms, so the host sent at most about 300 frames/s; see the [host throughput report](2026-11-12_host_throughput.md) and [prompt 56](../prompts/56_host_throughput.md). The text above is kept as written.
