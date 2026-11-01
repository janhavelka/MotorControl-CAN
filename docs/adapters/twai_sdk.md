# TWAI adapter SDK evidence

The selected interface is the ESP-IDF **v5.5.5 node API** in `esp_twai.h` and
`esp_twai_onchip.h`, reviewed at commit
`b774170ff46c393eeb5e495ea37936038d3f4f4f`. The initial SDK implementation targets
ESP32-S3. The installed portable library has no ESP-IDF, Arduino or board
dependency. See [the adapter guide](../../adapters/twai/README.md) for application
ownership and configuration.

This review establishes SDK behavior from source. Host tests and firmware
compilation are separate checks; neither establishes physical CAN or motor
qualification. No board pins, transceiver, drive, motor, firmware, node or bitrate
has been inferred from the RS bench.

## Version identity

pioarduino platform **55.03.311** selects Arduino **3.3.11**, whose
[dependency template](https://github.com/espressif/arduino-esp32/blob/3.3.11/package/package_esp32_index.template.json)
identifies `esp32-arduino-libs` as `idf-release_v5.5-b774170f-v1`.
The locally inspected `C:/p55s` packages report Arduino 3.3.11 and IDF v5.5.5
`b774170ff46`. SHA-256 hashes of all three installed public node API headers
match the original files at the full SDK commit above.

The default `C:/Users/Honza/.platformio` packages inspected during this audit
contain Arduino 3.2.0 and older IDF libraries. An installed platform manifest
alone does not prove that its requested framework has been resolved. Builds must
inspect the actual selected packages. Native ESP-IDF uses the same v5.5.5 pin.

[twai_sources.json](twai_sources.json) records original source URLs, the exact
commit and SHA-256 hashes. It contains provenance, without copied SDK source.
The [official v5.5.5 guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/peripherals/twai.html)
provides API context; the pinned implementation resolves details below.

## Reviewed SDK behavior

All source paths in this table are relative to the pinned ESP-IDF repository.
`onchip` means `components/esp_driver_twai/esp_twai_onchip.c`; `HAL` means
`components/hal/twai_hal_v1.c`; `LL` means
`components/hal/esp32s3/include/hal/twai_ll.h`.

| Evidence | Original source | Adapter contract |
| --- | --- | --- |
| `twai_node_transmit` accepts a frame pointer and can mount it for later transmission. | `onchip:575–631`; `include/esp_twai_types.h`, `twai_frame_t` | SDK acceptance means submission only. The adapter owns immutable frame metadata and bytes until checked completion or successful deletion. |
| TX completion supplies the exact `done_tx_frame` pointer and `is_tx_success`. | `onchip:283–289` | Match the active owned slot and its recorded request/generation. Callback evidence is copied; the borrowed SDK event is never retained. Wire TX success is distinct from endpoint acceptance and motion completion. |
| HAL v1 selects single shot whenever retry count differs from minus one. | `HAL:370–378`; `LL:285–299` | Configure `fail_retry_cnt=0`. No SDK second transaction is submitted while one is active. No automatic application replay follows errors or recovery. |
| Error callbacks and state callbacks are separate from TX completion. | `onchip:221–255` | Preserve controller errors, bus-off and TX outcomes independently. An error event cannot manufacture an endpoint result. |
| Disable stops the controller but retains `hw_busy`, current frame and queue. Enable can restart the retained current frame. | `onchip:452–510`; `HAL:165–172` | Disable alone is insufficient retirement. Delete the old node before creating a replacement. Never enable an old node to recover owner work. |
| Bus recovery can restart transactions remaining in the old SDK queue. | `onchip:246–253`, `:545–554` | Do not call old-node `twai_node_recover()` for owner recovery. Recovery destroys the old node and creates a fresh one after retirement. |
| Delete permits the SDK bus-off/disabled state, deinitializes hardware, resets/disables clocks and frees interrupt/queue storage. | `onchip:309–356` | Running shutdown requires successful disable then delete. Confirmed bus-off permits direct delete. Successful teardown retires local pending TX; it cannot retract an already transmitted frame or settle its external effect. |
| ESP32-S3 clock-source enable/disable returns `ESP_OK` unconditionally at this pin. | `components/esp_hw_support/port/esp32s3/esp_clk_tree.c:71–75` | The post-HAL clock-source error return in deletion is unreachable for this selected target. Other targets need a fresh lifecycle review. |
| RX parsing is permitted only during `on_rx_done`. Data copy is capped by the supplied buffer length and eight-byte hardware limit; original DLC is retained. | `components/esp_driver_twai/esp_twai.c:151–157`; `onchip:651–663`; `LL:724–770` | Receive into an owned eight-byte buffer, then validate DLC and flags before publishing. Reject DLC above eight. RTR preserves requested DLC and publishes no payload bytes. |
| S3 HAL has no hardware timestamp. With a configured timebase, the receive parser falls back to `esp_timer_get_time()`. | `HAL:382–389`; `onchip:440–449`, `:651–663` | Keep physical arrival time unknown. ISR observation time may be retained as a diagnostic. The application supplies explicit monotonic owner time; 64-bit ESP timer access requires no adapter wrap extension. |
| Full-open filter `id=0, mask=0` disables the HAL software distinction between standard and extended IDs. | `HAL:111–123`, `:443–449` | Accept both formats and route in the cooperative owner. Required heartbeat, PDO and EMCY traffic must remain visible. |
| SDK TX queue depth must be at least one; storage holds frame pointers. | `onchip:679`, `:698`, `:597–606` | Configure depth one and keep it unused by submitting one active SDK frame at a time. The adapter's bounded software FIFO is separate. |
| Hardware FIFO overrun can discard RX frames without a public overflow callback or counter. | `onchip:258–278`; `HAL:426–439` | Adapter ring overflow is observable; hardware RX completeness is not promised. A larger application ring cannot prove that hardware lost no frames. |
| SDK ISR drains hardware RX while the FIFO is nonempty. | `onchip:258–278` | Adapter callbacks perform bounded copying only. The underlying SDK loop has no fixed per-interrupt frame budget; service latency needs physical measurement. |
| Node object is allocated from internal RAM. Queue/event storage capabilities depend on cache-safe configuration. | `onchip:684–700`; `components/esp_driver_twai/twai_private.h:50–60` | Keep the callback context and adapter ISR storage in internal RAM. Initial support requires cache-safe ISR execution and power management disabled. |
| Cache-safe registration checks callback placement; reachable code and data must also be available while cache is disabled. | `onchip:362–370`; `components/esp_driver_twai/Kconfig`, `linker.lf` | Do not enable cache-safe execution merely by decorating an entry point. That configuration needs a separate complete call/data placement review. |
| S3 has one Classical CAN controller and one mask filter. | `components/soc/esp32s3/include/soc/soc_caps.h:412–418`; `LL` | Preserve 11/29-bit IDs and data/RTR. Reject FD, BRS and unsupported metadata before submission. An external CAN transceiver is required. |

Direct deletion after confirmed bus-off is a source-backed exception for this
pin and target: `_node_delete` accepts the SDK bus-off value; HAL deinitialization
masks peripheral interrupts; S3 clock teardown resets the controller before
freeing the interrupt and queue. The same-core owner prevents a concurrently
executing ISR during this task sequence. Power management and cache-safe
execution remain disabled. This does not establish a generic cross-target API
promise for deleting any live controller.

Interrupt creation and deletion must run on the same pinned owner task/core.
Espressif's [interrupt allocation requirements](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/intr_alloc.html)
require freeing external interrupts on their allocation core. This is especially
relevant because `_node_destroy` ignores the return value of `esp_intr_free`.
Do not treat a cross-core teardown attempt as evidence of retirement.

### Final handoff and GPIO validation

The final copied-RX/fault check and `twai_node_transmit(..., 0)` share one driver
critical section. This prevents a captured boot-up callback from entering between
the guard and SDK admission. Physical arrivals while interrupts are masked are
still unobserved; this lock provides no new remote freshness guarantee.

This relies on the reviewed single-outstanding-frame, same-pinned-core contract.
`onchip:283-301` calls TX completion before clearing `hw_busy`, but the owner task
cannot resume until that ISR finishes. The next submission therefore takes the
idle direct path at `onchip:591-595`. Its `xEventGroupClearBits` only nests a
critical section (`components/freertos/FreeRTOS-Kernel/event_groups.c:488-513`),
then the SDK mounts the frame and calls HAL without a queue wait or synchronous
callback. Xtensa `portable/xtensa/port.c` restores interrupts only at the outermost
critical exit. A different SDK, scheduler or submission policy needs a fresh review.

`components/esp_driver_gpio/include/driver/gpio.h:25-29` shifts `1ULL` by the pin
after checking only the lower bound. The driver checks `< GPIO_NUM_MAX` before
using those macros. The pinned S3 mask excludes GPIO22-25 and permits GPIO46;
board/transceiver suitability remains an independent application requirement.
The manifest now includes the GPIO and FreeRTOS sources supporting these facts.

## Selected bounds and failure behavior

The adapter has two TX slots in total, including waiting work, the single SDK
active frame and a retained transport report. A slot is not reusable while its
report awaits owner consumption. The driver has a sixteen-entry RX ring, one TX
completion mailbox and sticky fault/state/counter evidence. Admission reserves
owned storage before publication. These are configured bounds, without a
physical throughput or service-gap guarantee.

RX and TX callbacks copy bounded values into application-owned internal RAM;
state/error callbacks retain sticky bounded evidence.
They do not route protocol traffic, print, sleep or call the owner recursively.
Ring or mailbox overflow faults the adapter and prevents silent success. Pending
work remains unavailable for fresh execution until explicit retirement succeeds.
Raw diagnostic tracing must not consume frames from normal dispatch.

Shutdown and recovery preserve the owned SDK frame and context when teardown
fails. The callback context remains alive for the entire node lifetime, including
after a TX callback. New work remains blocked. A successful delete retires local pending work;
a new node starts with new ownership evidence. No recovery step sends NMT,
enables a drive, clears a drive fault or replays a previous motor write.

## Rejected interface and remaining limits

The legacy `driver/twai.h` receive wrapper is not selected. At this pin,
`components/driver/twai/twai.c:819` zero-fills using eight minus the returned DLC,
while HAL/LL preserve the original four-bit DLC. A received DLC above eight can
therefore reach an invalid `memset` length before an adapter can reject it. The
node API performs the bounded copy directly and permits validation afterward.
This is a source finding; no malformed live traffic was generated.

Original ESP32 errata handling can retransmit a saved frame after a peripheral
reset, including an explicitly documented duplicate-send possibility in
`HAL:344–352`. Initial S3 support does not establish support for that target or
for another SDK release. Firmware compilation, electrical behavior, bus-off
reproduction, sustained load, memory use and maximum service gaps remain separate
verification and qualification evidence.
