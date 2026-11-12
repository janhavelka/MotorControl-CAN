/** @file BenchNodes.h
 * @brief The bench fixture's drive count, shared by every bench record, parser and the bench main.
 * SPDX-License-Identifier: MIT
 *
 * The recorded fixture (docs/hardware_bench.md) has two drives: CANopen nodes 1 and 2, on adapter
 * slots 0 and 1, logical axes 1 and 2. Every per-node bench array and node check takes its bound from
 * BENCH_NODES. A fixture with more drives is a new recorded fixture with its own routes, caps and
 * runner rules, not only a larger number here. The SDO channel count is the application's capacity
 * set (Capacity.h): a build with more channels compiles the bench unchanged and leaves the other
 * channels unused; BenchRoute.h checks that there are at least BENCH_NODES. No dependencies, so the
 * bench's text parsers stay independent of the adapters.
 */
#pragma once

namespace MotorControlCANExample {
enum {
    BENCH_NODES = 2 ///< Fixture drives: node n on adapter slot n - 1.
};
} // namespace MotorControlCANExample
