# Native exact preparation, read, move and stop example

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

It reads identity and state through `prepareIdentity` / `prepareState`,
`nextRead`, `advanceRead` and checked `getIdentity`, using supplied synthetic
service completions. It then calls `prepareMoveAbsolute` with default
`MoveEvidence` and prints the `UNSUPPORTED` refusal: this example has no
measured acceptance/completion record. It runs the whole move call sequence
against a host simulation, with an invented
evidence record that the code labels SIMULATION ONLY: `nextMove`,
`beginMoveObservation`, `noteMoveTransmission` and `advanceMove`, with each
download printed. Its terminals are built by hand, as a sole owner would publish
them. That run shows the calls, not a drive: an application supplies its own
reviewed record, or the gate stays closed. See the
[move guide](../../docs/position_parameters.md).

Between those two it calls `prepareMoveRelative` with the same simulated record
and an illustrative idle `ACTUAL` witness, and prints the `UNSUPPORTED`
`RELATIVE_BASIS` refusal: this example has no measured record of what the
drive adds a relative displacement to on the `005F` edge. It supplies no
qualified wire basis and runs no relative simulation.

Finally it uses the simulated move's last acknowledged controlword and a
fresh typed state read to prepare a mode-1 `HALT`. First, `prepareStop` rejects
missing predecessor history with `INVALID_CONFIG` / `ActionError::STATE`,
leaving the action empty. With that explicit synthetic history it calls
`prepareStop`, `nextAction`, `noteActionTransmission` and `advanceAction`.
The fake supplies one acknowledged `6040 = 010F` download and subsequent
controller-not-running observations. The retained execution is `ACKNOWLEDGED`
and completion is `OBSERVED`; no physical stop or standstill was measured.
It sends no enable to prepare the stop. The local simulation is not device
qualification; see the [stop contract](../../docs/stop_actions.md).

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
