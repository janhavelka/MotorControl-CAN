# Portable core ESP-IDF component consumer

This separate application checks native ESP-IDF component consumption of the
portable library. It builds and links real public functions through
`REQUIRES MotorControlCAN`; the standalone probe firmware has its own gate.
The compile fixture is ESP32-S3 with ESP-IDF **5.5.5**, commit
`b774170ff46c393eeb5e495ea37936038d3f4f4f`, matching the
[reviewed SDK evidence](../../docs/adapters/twai_sdk.md). It does not establish
support or qualification for other targets.

From an exported ESP-IDF 5.5.5 shell at the repository root, use fresh stage
and build paths. Quoted arguments preserve paths containing spaces:

```powershell
$consumerRepository = (Get-Location).Path
$coreStage = Join-Path $consumerRepository '.build/idf-core-stage/MotorControlCAN'
if (Test-Path -LiteralPath $coreStage) { throw 'Choose a fresh core stage path' }
New-Item -ItemType Directory -Path $coreStage -Force | Out-Null
Copy-Item -LiteralPath CMakeLists.txt,library.json -Destination $coreStage
Copy-Item -LiteralPath include,src -Destination $coreStage -Recurse
$consumerBuild = Join-Path $consumerRepository '.build/idf-core-consumer'
if (Test-Path -LiteralPath $consumerBuild) { throw 'Choose a fresh consumer build path' }
idf.py -C test/idf_consumer -B "$consumerBuild" -D 'IDF_TARGET=esp32s3' -D "MOTORCONTROLCAN_COMPONENT_PATH=$coreStage" build
```

The caller must stage only the exported root `CMakeLists.txt`, `include/`,
`src/` and `library.json` in a directory named `MotorControlCAN`. CMake rejects
missing entries, a different component name and extra top-level entries.
`library.json` records package provenance; the ESP-IDF component branch of
the root CMake file does not read it. Examples, adapters, CANopenNode, tests,
references and repository-private headers are absent from the stage.

Expected evidence is a successful ESP-IDF configure, compilation of the staged
core sources and consumer `main.cpp`, and linking of
`MotorControlCANConsumer.elf` and firmware image. Retain the command, SDK
revision, compiler version, build log and component/source paths in the stage
report. SDK startup dependencies are part of the IDF application; they are not
dependencies of the portable library.

The consumer compiles the public headers and contains checked exact target
preparation above the binary64 integer precision boundary, stale-generation
rejection with preserved output, and extended RTR frame validation/rejection.
Computed results are retained in volatile storage so the linked application
uses library symbols. A firmware build does not execute these checks; native
tests supply runtime software evidence. There is no demonstration console
output, hardware setup, controller initialization, CAN transmission or motion.
This gate does not flash or run firmware and provides no physical evidence.
