# Common API and ownership proof

Prompt 6's offline C++11 experiment freezes the minimum contracts described in
[architecture](../../docs/architecture.md). It reuses prompt 5's pinned CANopenNode
client and fake controller. Synthetic objects 0x2000/0x2001 are test inputs, not
Lichuan/N5/EPOS4 sequences. Nothing here is an installed driver.

```sh
cmake -S experiments/api_ownership -B .build/api_ownership -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/api_ownership
ctest --test-dir .build/api_ownership --output-on-failure
```

The configure step verifies the retained stack hashes. It needs native C99/C++11,
CMake and a generator; it downloads nothing and needs no SDK, Python, siblings
or hardware. Tests use always-active checks, including Release/NDEBUG builds.

Optional focused source-documentation check, tested with Doxygen 1.13.2:
`doxygen experiments/api_ownership/Doxyfile` from the repository root. It checks
every public proof header with warnings treated as failures. The production
documentation and CI gates remain prompt 8 work.

| Files / target | Purpose |
| --- | --- |
| `Contracts.h`, `Operations.h/.cpp` / `api_contract` | Pure values, synthetic read and acknowledged target-write contexts; no stack headers or allocation/I/O/clock |
| `CanBusOwner.h/.cpp` / `api_owner` | Application fixed storage and sole RX boundary around the selected client |
| `main.cpp` / `api_ownership` | Copied payload, read/write flow, retained results, urgent/fair scheduling, deadlines, cancellation, abort, event overflow and boot-up invalidation |
| `fixtures_test.cpp` / `api_fixtures` | Core-only N5/EPOS4 contrasts, optional geometry, scales, capability gates and protocol-neutral binding |
| Reused `canopen_service_fit` test | Original selected-engine exchange/segmentation/failure proof |

The measured Release host layout on x64 MinGW GCC 15.1 is `ServiceWork=112`,
`ServiceEvidence=104`, `CanBusOwner=1384` bytes. These are `sizeof` values, not
peak stack, embedded RAM, timing or heap measurements. The core/owner no-allocation
claim is source-inspected. Tests use standard output only for reporting.

Keep test-only profiles and bridge dependencies out of installed headers in
prompt 7. Production arithmetic remains prompts 9–10; complete services, real
motion and controller qualification retain their assigned later stages. Read
[owner lifetimes](../../docs/can_owner.md) and
[service premises](../../docs/canopen_services.md) before reusing this proof.
