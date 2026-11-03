# Native position parameter preview

This standalone native example calls the installed
`LICHUAN_C::preparePositionParameters` API. It prepares an explicit absolute
250-pulse target with 120 rpm profile speed and 100/200 ms ramp times.
The route identity and requested parameters are illustrative host inputs.

The result owns the original request, exact target/quantization evidence, axis
configuration and requested native settings. No SDK, CLI, CAN owner, service
engine or hardware connection is needed. Successful execution returns zero and
prints the parameter candidate. It sends no traffic and establishes no drive
acceptance, firmware applicability, physical position or completed move.

The helper supports unwrapped integer targets, including explicitly bounded
approximate radians conversion. It checks the documented target/rpm/time ranges
without clamping. Relative inputs additionally need the caller's matching host
position witness; their arithmetic does not establish a Lichuan wire reference
basis or target queue.

It then calls `prepareMoveAbsolute` with default `MoveEvidence` and prints the
`UNSUPPORTED` refusal: no specimen has qualified acceptance or completion. Last,
it runs the whole call sequence against a host simulation, with an invented
evidence record that the code labels SIMULATION ONLY: `nextMove`,
`beginMoveObservation`, `noteMoveTransmission` and `advanceMove`, with each
download printed. Its terminals are built by hand, as a sole owner would publish
them. That run shows the calls, not a drive: an application supplies its own
reviewed record, or the gate stays closed. See the
[move guide](../../docs/position_parameters.md).

Between those two it calls `prepareMoveRelative` with the same simulated record
and an illustrative idle `ACTUAL` witness, and prints the `UNSUPPORTED`
`RELATIVE_BASIS` refusal: no specimen has measured what the drive adds a
relative displacement to on the `005F` edge, so no relative basis is supported
and the example runs no relative simulation.

From the repository root, with the native tools described in
[getting started](../../docs/getting_started.md):

```sh
cmake -S . -B .build/position -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/position --target position_preview
.build/position/examples/position_preview/position_preview
```

On Windows the executable has the `.exe` suffix. To use an installed core,
configure this directory directly with `-DCMAKE_PREFIX_PATH` pointing at its
installation prefix. The full package verifier also builds and runs this example
against a relocated install.
