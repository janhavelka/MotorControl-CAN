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
basis or target queue. Full finite motion remains pending in
[prompt 33](../../docs/prompts/33_position_moves.md).

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
