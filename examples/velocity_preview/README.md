# Finite velocity preview

This offline installed-core example converts an illustrative load rate to motor
rpm and demonstrates the closed firmware gate. It needs no stack, MCU or drive.
The ratio is an application input, not a measured LC60H2127 or CL86-C fact.

```sh
cmake -S examples/velocity_preview -B build/velocity -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build build/velocity
```

Run `velocity_preview` in that build directory. Actual runs use
`AxisSession::requestVelocity` and the ordinary owner; see the
[velocity contract](../../docs/velocity.md). Native fake-owner tests exercise
the full sequence and its urgent stop without qualifying physical behavior.
