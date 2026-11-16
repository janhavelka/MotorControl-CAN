# Startup cold-storage compile fixture

This application never starts CAN, a task, a receive loop or motor work. It builds
the SDK allocator boundary with actual driver, RX/TX, stop and task storage types
kept in static internal RAM. Only an independent 32 KiB archival byte buffer uses
[StartupColdStorage](../common/StartupColdStorage.h).

`StartupColdStorage` tries an aligned external allocation once when the application
reports initialized PSRAM, then explicitly internal RAM once if needed. It owns
the successful block for its lifetime. Invalid arguments or two refusals leave it
empty. No allocation occurs in a control loop. Installed core code is unchanged
and allocates nothing.

Run the same checked build used by CI with the reviewed ESP-IDF 5.5.5 environment:

```text
python scripts/check_firmware.py --target storage
```

This builds `storage-s2`, `storage-s2-psram`, `storage-s3` and
`storage-s3-psram`. Each may also be selected individually. The optional PSRAM
configurations use capability-only allocation, allow boot without detected
PSRAM, and disable external BSS, noinit and task stacks. Quad/40 MHz is a compile
configuration, not a claim about any board's wiring or fitted memory. The current
S2 bench image continues to disable PSRAM.

On an independently reviewed board run, the fixture checks its actual allocation
branch and internal hot-storage addresses, then emits one `STORAGE` line. Linking
does not execute those checks or establish allocation availability, driver/ISR
latency or stop timing. Runtime qualification remains in Prompt54. See the
[storage report](../../docs/reports/prompt_53_storage.md) for exact build evidence
and limitations.
