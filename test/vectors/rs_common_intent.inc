// SPDX-License-Identifier: MIT
// Retained MotorControl-RS common-intent vectors, read by test/move_common_test.cpp.
//
// What they are: exact position intents and what MotorControlRS::preparePosition (MotorControl-RS
// src/Axis.cpp, declared in include/MotorControlRS/Axis.h) returns for them. They are pure host
// arithmetic, not CAN, device, firmware or bench facts. RS is the authority for the meaning of the
// shared intent names; these rows are its observed output at the RS_MOVE_REVISION below, compiled
// from exactly the RS_MOVE_SOURCE files listed with their Git blob IDs at that revision.
//
// History: packet 21 computed the same expected values at MotorControl-RS a13f767 with a scratch
// build that was not retained. At a13f767 and at RS_MOVE_REVISION the blobs of src/Axis.cpp,
// src/Units.cpp, src/UnitFactors.h and every listed header except ActionOperation.h are identical;
// that header differs only in two comments. The producer reproduced every row at both revisions.
//
// Check or regenerate (optional; normal builds, tests and package consumers never need it):
//   python scripts/rs_move_vectors.py --rs <MotorControl-RS checkout>
// It reads the RS_MOVE_REVISION files with git archive (no checkout, no write to RS), compiles
// scripts/rs_move_vectors.cpp against them with g++ (or --compiler), runs it on the inputs below,
// and compares inputs, outputs and the source closure with this file. --revision REV or --worktree
// compare with other RS source; any difference is reported as an audit difference and this file is
// left unchanged. --candidate PATH writes a reviewed replacement elsewhere; replacing this file is
// a deliberate audit decision recorded with its revision, not a routine regeneration.
//
// Shared fixture, applied identically by both consumers: generation 5; command polarity +1;
// unbounded native limits; no soft limits; relative bases ACTUAL only; host origin known at the
// RS_MOVE_AXIS origin; witness idle, stationary, native-known, basis ACTUAL, source QUALIFIED,
// observed and supplied at 1000 us with maximum age 1000 us; every scale source QUALIFIED. Every
// request field is set because defaults differ (RS relative defaults true, CAN false; RS's
// allowance is double, CAN's Rational): wrapped false, path SHORTEST, tie REJECT, basis ACTUAL,
// approximate false, no approximation allowance. CAN also sets referencePolicy OPTIONAL, which RS
// does not have.
//
// RS_MOVE_REVISION(MotorControl-RS commit)
// RS_MOVE_SOURCE(path in MotorControl-RS, Git blob ID at that commit)
// RS_MOVE_AXIS(name, command steps per motor turn n, d, motor turns per load turn n, d,
//              full steps per motor turn n, d (0, 1 = no full-step geometry), origin native)
// RS_MOVE_VECTOR(name, axis, value n, d, unit, frame, relative, rounding,
//                allowance in halves of a native increment, witness native position,
//                effective native, endpoint native, rounding error times 6)
//   The last column is RS PreparedTarget::roundingError (effective minus requested) times 6, so
//   thirds and halves of an increment are both exact integers.
// RS_MOVE_REFUSAL(name, axis, value n, d, unit, frame, relative, rounding, allowance halves,
//                 witness, RS Err name, RS Status::detail)
//   RS refuses the request; CAN must refuse it too. The RS code and detail are RS evidence only.
//
// An includer defines the macros it consumes; the rest default to nothing and all are undefined
// at the end of this file.

#ifndef RS_MOVE_REVISION
#define RS_MOVE_REVISION(...)
#endif
#ifndef RS_MOVE_SOURCE
#define RS_MOVE_SOURCE(...)
#endif
#ifndef RS_MOVE_AXIS
#define RS_MOVE_AXIS(...)
#endif
#ifndef RS_MOVE_VECTOR
#define RS_MOVE_VECTOR(...)
#endif
#ifndef RS_MOVE_REFUSAL
#define RS_MOVE_REFUSAL(...)
#endif

RS_MOVE_REVISION("cd27b12389f568198b72cecb6606123ea4c089bc")

RS_MOVE_SOURCE("include/MotorControlRS/ActionOperation.h", "de85fca37c939169ab7be660248c50f93a205b3f")
RS_MOVE_SOURCE("include/MotorControlRS/Axis.h", "7ce5d45faa8dd6b83ff659ecbdb0a53c1ffe2b14")
RS_MOVE_SOURCE("include/MotorControlRS/ReadOperation.h", "6af7cd5947300ea3eaa0186a5f8e23c56470381a")
RS_MOVE_SOURCE("include/MotorControlRS/Status.h", "37d1c54edd4d4cad73fba81e2f242857f0e2c09d")
RS_MOVE_SOURCE("include/MotorControlRS/Units.h", "f6ca2ba7c6bd091a2d16a048c7788db0b9bec079")
RS_MOVE_SOURCE("include/MotorControlRS/VelocityOperation.h", "5516d3418c68b6e055aadd452aa9d7218e28d321")
RS_MOVE_SOURCE("src/Axis.cpp", "1e0423bb6ad737bdfbc913ffaa329df5e8a049db")
RS_MOVE_SOURCE("src/UnitFactors.h", "ab425d16682450d74172e438097136b12efdc001")
RS_MOVE_SOURCE("src/Units.cpp", "724ca0a884eee85b2ebf76d906960415c627fb38")

RS_MOVE_AXIS("stepper", 3200, 1, 5, 1, 200, 1, 0)
RS_MOVE_AXIS("servo", 10000, 1, 1, 1, 0, 1, 0)

RS_MOVE_VECTOR("2500 steps", "stepper", 2500, 1, STEPS, NATIVE, true, EXACT, 0, 1000, 2500, 3500, 0)
RS_MOVE_VECTOR("+1/3 motor turn", "stepper", 1, 3, TURNS, MOTOR, true, NEAREST, 1, 50, 1067, 1117, 2)
RS_MOVE_VECTOR("-1/3 motor turn", "stepper", -1, 3, TURNS, MOTOR, true, NEAREST, 1, 50, -1067, -1017, -2)
RS_MOVE_VECTOR("1/5 load turn", "stepper", 1, 5, TURNS, LOAD, true, EXACT, 0, 0, 3200, 3200, 0)
RS_MOVE_VECTOR("90 load degrees", "stepper", 90, 1, DEGREES, LOAD, true, EXACT, 0, -100, 4000, 3900, 0)
RS_MOVE_VECTOR("7/2 steps", "stepper", 7, 2, STEPS, NATIVE, true, NEAREST, 1, 0, 4, 4, 3)
RS_MOVE_VECTOR("5/2 steps", "stepper", 5, 2, STEPS, NATIVE, true, NEAREST, 1, 0, 2, 2, -3)
RS_MOVE_VECTOR("1 full step", "stepper", 1, 1, FULL_STEPS, MOTOR, true, EXACT, 0, 0, 16, 16, 0)
RS_MOVE_VECTOR("servo 1/4 turn", "servo", 1, 4, TURNS, MOTOR, true, EXACT, 0, 0, 2500, 2500, 0)
RS_MOVE_VECTOR("servo -1/3 turn", "servo", -1, 3, TURNS, MOTOR, true, NEAREST, 1, 0, -3333, -3333, 2)
RS_MOVE_VECTOR("absolute 1/3 motor turn", "stepper", 1, 3, TURNS, MOTOR, false, NEAREST, 1, 0, 1067, 1067, 2)

RS_MOVE_REFUSAL("servo 1 full step", "servo", 1, 1, FULL_STEPS, MOTOR, true, EXACT, 0, 0, INVALID_CONFIG, 102)

#undef RS_MOVE_REVISION
#undef RS_MOVE_SOURCE
#undef RS_MOVE_AXIS
#undef RS_MOVE_VECTOR
#undef RS_MOVE_REFUSAL
