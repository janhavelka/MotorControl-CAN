# ESP32-S2 console ownership

The example platform uses ESP-IDF 5.5.5 at
`b774170ff46c393eeb5e495ea37936038d3f4f4f`. Reviewed original SDK paths and
SHA-256 values are in [esp_platform_sources.json](esp_platform_sources.json).
This proof concerns console calls and task ownership. It does not establish
CAN retirement, electrical correctness, drive identity or harmless read effects.

`EspProbePlatform` selects UART0 on S2 and retains USB Serial/JTAG on S3.
S2 UART0 uses GPIO43 TX and GPIO44 RX, 115200 baud, eight data bits, no parity,
one stop bit and no flow control. The SDK's `soc/uart_pins.h` defines the same
default GPIOs. These match the inspected OPTAK programming connector; the
[fixture intake](../reports/2026-11-02_s2_bench_intake.md) records the separate
design and ROM evidence. They are example configuration, not installed-core
defaults or a generic board detection mechanism.

## Exclusive owner and finite calls

The platform records the current pinned task/core during `begin`. Every clock,
read, write, idle and teardown call checks that identity. S2 uses its sole core0;
the shared S3 entry retains core1. Calls from another task/core cannot access the
SDK driver. Construction, cached diagnostics and callback construction do no I/O.
The platform attempts initialization once and rejects an already installed
UART0 driver. `CONFIG_ESP_CONSOLE_NONE` is required at compile time on S2.
Applications must not install GDB, VFS, logging or another UART0 user, or call
UART SDK operations outside this owner. The check cannot authenticate external
code's ownership promises.

`uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0)` installs a 256-byte RX
ring, no TX ring and no event queue. The UART SDK allocates driver storage and
attaches its interrupt to the calling core. Preparation sets the format and
pins before installing this driver. A setup failure admits no console service
and is not automatically retried.

`uart_read_bytes(..., 1, 0)` requests one byte with zero ticks for its RX mutex
and ring receive. Empty/error leaves the public output unchanged. Its internal
full-buffer reconciliation cannot be replenished by another consumer under
the same-core, single-reader ownership used here.

`uart_tx_chars(..., 1)` writes only available FIFO space through
`uart_enable_tx_write_fifo`; it does not wait for FIFO space or wire completion.
The SDK internally takes its TX mutex with `portMAX_DELAY`. That acquisition is
uncontended because this task is the exclusive UART0 writer and this call
releases the mutex before returning. This ownership assumption is necessary to
the bounded-call argument; using this API from another task invalidates it.
`uart_write_bytes` and `uart_wait_tx_done` are never used by the platform.
Zero accepted bytes returns false and increments a saturating refusal counter;
the application queue retains its unaccepted suffix for a later ordinary turn.

`uart_driver_delete` frees the interrupt and driver resources without waiting
for console delivery. The object relinquishes ownership only on successful
deletion; failure keeps its storage/callback lifetime obligation. No destructor
uninstalls a driver. A successful write means FIFO acceptance, not complete
transmission, ESP-Prog2 receipt or PC receipt. Teardown can truncate queued output.

## Evidence limits and checks

No UART event queue is consumed. FIFO/ring overflow or framing errors therefore
cannot be fully counted by this console, even though the application has its
own bounded input/output pressure counters. Serial text is not a lossless raw
CAN evidence recorder. The bench operator must retain explicit acquisition
overflow and completion records and account for host capture loss separately.

The source-compiled S2 fake test verifies the exact pins/format/buffer arguments,
one-byte zero-tick reads, FIFO refusal, unchanged failed outputs, task/core
rejection, one-shot initialization, partial setup failures, explicit teardown
failure, equal/negative/backward clock handling and idle ownership. Existing S3
platform tests continue to exercise the original USB path. Fake tests establish
host contracts only. Real target build/link, reset/console observation and CAN
acquisition remain separately reported checks.
