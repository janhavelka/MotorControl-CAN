# ESP32-S2 TWAI target evidence

The ESP32-S2 adapter uses the same ESP-IDF 5.5.5 node API and fixed ownership
contract as the [initial ESP32-S3 review](twai_sdk.md). This extension was
reviewed on 2026-11-02 against `C:/pio/packages/framework-espidf`, at upstream
commit `b774170ff46c393eeb5e495ea37936038d3f4f4f`. All 24 previously recorded SDK
file hashes match. The four additional S2 source files in
[twai_sources.json](twai_sources.json) match both the installed bytes and the
original bytes fetched from that exact upstream commit.

This is source evidence for admitting this target. Firmware builds and physical
qualification are separate evidence. No bus timing, wiring, transceiver behavior
or motor behavior is inferred from S3 or another board.

## Target-specific proof

Paths and line numbers below refer to that exact SDK commit. `onchip` is
`components/esp_driver_twai/esp_twai_onchip.c`, `HAL` is
`components/hal/twai_hal_v1.c`, and `S2 LL` is
`components/hal/esp32s2/include/hal/twai_ll.h`.

| Claim | Source and implication |
| --- | --- |
| One CPU, one Classical CAN controller, one mask filter | `components/soc/esp32s2/include/soc/soc_caps.h:146,363-368`. The application owner runs on core 0. The driver still checks the same task and core on every cooperative call. No FD or hardware timestamp capability is declared. |
| No automatic retry of the submitted frame | `HAL:369-379` turns `fail_retry_cnt=0` into single-shot metadata; `HAL:391-405` selects the single-shot command. `S2 LL:287-303` writes `command_reg.val=0x03` and explicitly documents no automatic retry after errors. The original-ESP32 reset/retransmit workaround at `HAL:318-367` is excluded by its target guard. The adapter retains one outstanding frame, never enables or recovers the old node, and never submits an automatic replay. |
| Disable precedes destruction for a running node | `onchip:480-510` disables the interrupt before stopping HAL; `HAL:165-172` enters reset mode and halts TX. Disable retains SDK transaction storage, so it alone is insufficient retirement. |
| Delete retires the old local callback and transaction storage | `onchip:345-356` requires disabled/bus-off state, releases I/O, deinitializes HAL, disables/resets the controller, disables its clock source, then destroys resources. `HAL:68-74` masks peripheral interrupts; `S2 LL:432-435` writes that mask. `onchip:46-54` calls S2 clock/reset operations; `S2 LL:148-175` clears the TWAI clock bit and pulses the TWAI reset. |
| Clock-source teardown cannot return an error on this target | `components/esp_hw_support/port/esp32s2/esp_clk_tree.c:71-75` unconditionally returns `ESP_OK`. The post-HAL early error return in `onchip:354` is therefore unreachable for the reviewed S2 implementation. |
| ISR cannot outlive successful same-core deletion | `onchip:704` allocates the interrupt on the owner core; `components/esp_hw_support/intr_alloc.c:580-601` selects the current CPU. The S2 unicore path at `intr_alloc.c:767-793` directly calls the local free routine; `:796-858` disables the interrupt and removes its handler under a critical section before freeing its handle. `onchip:327-342` frees interrupts before queues and node storage. The same core cannot execute the owner task concurrently with its ISR. The context remains alive on any adapter shutdown failure. |
| GPIO validity differs from S3 | `components/soc/esp32s2/include/soc/soc_caps.h:171-173` excludes GPIO22-25 and excludes GPIO46 from output capability. `components/soc/esp32s2/include/soc/gpio_num.h:60-61` declares GPIO46 input-only and `GPIO_NUM_MAX=47`. The existing driver bounds the pin before evaluating the SDK mask, then requires output capability for TX and input capability for RX. These checks do not prove board pin availability. |

Direct deletion after confirmed bus-off remains a narrow, source-backed exception
for this SDK/target: pending TX is halted by bus-off (`HAL:203-211`), peripheral
interrupts are masked and the controller is reset before ISR and queue storage
are freed. It does not establish a generic cross-target lifecycle rule or retract
an already transmitted frame.

The same final submission critical section remains valid on S2: the owner cannot
run until its ISR has finished clearing `hw_busy` (`onchip:283-301`), so the next
single outstanding submission follows the direct path at `onchip:591-595`.
The existing reviewed Xtensa FreeRTOS critical-section/event-group sources apply
to this target. Cache-safe ISR, ISR-in-IRAM, dynamic power management and FreeRTOS
SMP remain rejected. Context and retained ISR data stay in internal RAM.

## Limits and verification

Only the target guard and explanatory comments change. Pin validation uses the
actual selected target's SDK definitions. Queue bounds, callbacks, explicit
retirement, one receive owner, frame validation and execution uncertainty retain
their existing contracts. The S2 owner must be pinned to core 0; the S3 example
continues to use core 1.

The shared HAL reports no physical receive timestamp (`HAL:382-389`). Hardware
FIFO loss and service latency still require measurement. This target review does
not establish Arduino-on-S2 support, electrical qualification,
bus-off recovery measurements or a live motor result. The IDF job in `.github/workflows/verify.yml` builds the S2 target. Record actual
bench outcomes in the active stage/bench report.

The later [receive-time extension](twai_receive_time.md) retains exact capture as
unknown and adds conservative completed-frame intervals using a pinned read-only
FIFO-count probe. It adds no reset or implicit device command. Its source proof,
ordering rules and host checks are recorded separately from this target-enablement
review.
