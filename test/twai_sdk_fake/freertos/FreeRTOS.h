// SPDX-License-Identifier: MIT
#pragma once
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portNUM_PROCESSORS 2
// Single-core interrupt masking is nesting-aware; pending fake interrupts run
// only when the outermost critical section exits. No host threads are created.
void fakeEnterCritical(portMUX_TYPE*);
void fakeExitCritical(portMUX_TYPE*);
#define portENTER_CRITICAL(p) fakeEnterCritical(p)
#define portEXIT_CRITICAL(p) fakeExitCritical(p)
#define portENTER_CRITICAL_ISR(p) fakeEnterCritical(p)
#define portEXIT_CRITICAL_ISR(p) fakeExitCritical(p)
