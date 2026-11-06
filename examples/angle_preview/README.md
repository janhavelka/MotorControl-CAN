# Native wrapped-angle preview

This offline example calls the installed `LICHUAN_C::preparePositionParameters`
and `LICHUAN_C::prepareMoveAngle` APIs. It uses illustrative application inputs:
1000 command increments per motor turn, five motor turns per load turn, command
polarity +1, host origin 100, native limits -10000..10000, and software limits
-5000..5000. These are assumptions for arithmetic, not manufacturer defaults,
readback or qualification of any drive.

From an illustrative idle, stationary `ACTUAL` witness at host zero it requests
10 load degrees along the shortest path, rejects exact half-turn ties, and rounds
to nearest with a maximum error of half a command increment. The result is
absolute target 239 increments and signed quantization error +1/9 increment.
The preview prints these values and the frozen configuration/reference epochs
before any admission. An application must establish its command scale,
direction, origin and multi-turn reference independently; feedback counts do
not establish command increments per turn.

The final `prepareMoveAngle` call intentionally supplies a closed
`MoveEvidence` gate. It returns `UNSUPPORTED` without changing the context.
This example has no owner, reservation or observed drive history and leaves
those inputs empty: the closed capability check refuses before they are used.
A live caller must supply its actual owner reservation, matching current
firmware identity, latest acknowledged `000F`, fresh admission read, and a
reviewed acceptance/completion evidence record. The angle resolves to an
absolute endpoint and uses the ordinary finite move lifecycle; its host
`ACTUAL` reference does not require a relative `005F` wire-base measurement.
See the [position guide](../../docs/position_parameters.md) and
[axis coordinate guide](../../docs/axis_contract.md).

No SDK, CAN traffic, enable, movement or physical measurement occurs. Success
means that the preview and expected closed-gate refusal behaved as described.

From the repository root:

```sh
cmake -S . -B .build/angle -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/angle --target angle_preview
.build/angle/examples/angle_preview/angle_preview
```

On Windows the executable has the `.exe` suffix. For an installed core,
configure this directory directly with `-DCMAKE_PREFIX_PATH` pointing to the
installation prefix. The package verifier builds and runs that consumer too.
