/**
 * @file unity_config.h
 * @brief Unity configuration of the native PlatformIO test suites.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * The native suites build with the same warnings as scripts/run_host_tests.py, including
 * -Wsign-conversion and -Werror (F-060). When this file exists, PlatformIO does not generate
 * its own Unity output adapter, which does not build with those flags; Unity then uses its
 * defaults and writes its report with putchar().
 */
#ifndef CLOCK_TEST_UNITY_CONFIG_H
#define CLOCK_TEST_UNITY_CONFIG_H

#endif  // CLOCK_TEST_UNITY_CONFIG_H
