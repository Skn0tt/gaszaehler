/*
 * SPDX-FileCopyrightText: 2025 Simon Knott
 * SPDX-License-Identifier: MIT
 *
 * Project-level overrides for CHIP device identity.
 */

#pragma once

#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME "skn0tt"
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "gaszaehler"

/* Software version string: set to git commit hash at build time via CMake.
   Falls back to "unknown" if GIT_VERSION is not defined. */
#ifdef GIT_VERSION
#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING GIT_VERSION
#else
#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING "unknown"
#endif
