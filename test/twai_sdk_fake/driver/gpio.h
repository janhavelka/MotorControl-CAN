// SPDX-License-Identifier: MIT
#pragma once
#define GPIO_IS_VALID_OUTPUT_GPIO(p) ((p) >= 0 && (p) < 49 && (p) != 46)
#define GPIO_IS_VALID_GPIO(p) ((p) >= 0 && (p) < 49)
