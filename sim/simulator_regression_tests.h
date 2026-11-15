/**
 * @file simulator_regression_tests.h
 * @brief Entry point for native-simulator runtime regression tests.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

namespace clockfw::sim::tests {

/** @brief Runs scheduler-remainder (#11) and host-patch persistence (#9) regressions. */
int runRuntimeRegressions();

}  // namespace clockfw::sim::tests
