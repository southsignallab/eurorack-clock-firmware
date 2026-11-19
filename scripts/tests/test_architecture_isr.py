"""Regression tests for the interrupt-context lock check in check_architecture.py (F-030).

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_architecture.py"))
violations = CHECK["isr_lock_violations"]

ENGINE = """
void Engine::play() { applyTransport(); }
void Engine::applyTransport() { hal::InterruptLock interruptLock; applyUnsafe(); }
void Engine::playFromIsr() { applyUnsafe(); }
void Engine::applyUnsafe() {}
bool Engine::isPlaying() const { hal::InterruptLock interruptLock; return playing_; }
void Engine::countForTest() const { if (hal::InterruptLock::activeCountForTest() != 0U) { ++count_; } }
void Engine::processSchedulerTick() { countForTest(); }
"""


class IsrLockCheckTests(unittest.TestCase):
    def test_lock_free_interrupt_paths_pass(self) -> None:
        controller = """
void Controller::processSchedulerTick(const std::uint32_t nowUs) { requestPlay(); }
void Controller::requestPlay() { engine_.playFromIsr(); }
void Controller::closeWindow() { hal::InterruptLock interruptLock; open_ = false; }
"""
        self.assertEqual(violations({"engine.cpp": ENGINE, "controller.cpp": controller}), [])

    def test_transitive_lock_behind_an_object_call_is_reported(self) -> None:
        controller = """
void Controller::processSchedulerTick(const std::uint32_t nowUs) { requestPlay(); }
void Controller::requestPlay() { engine_.play(); }
"""
        found = violations({"engine.cpp": ENGINE, "controller.cpp": controller})
        self.assertEqual(len(found), 1)
        self.assertIn("Controller::requestPlay()", found[0])
        self.assertIn("calls play()", found[0])

    def test_lock_in_from_isr_function_and_in_its_helpers_is_reported(self) -> None:
        source = """
void Capture::captureFromIsr() { push(); }
void Capture::push() { hal::InterruptLock interruptLock; }
extern "C" void TIM3_IRQHandler() { hal::InterruptLock lock; }
"""
        found = violations({"capture.cpp": source})
        self.assertEqual(len(found), 2)
        self.assertTrue(any("Capture::push()" in item for item in found))
        self.assertTrue(any("TIM3_IRQHandler()" in item for item in found))

    def test_comments_and_static_counters_are_not_locks(self) -> None:
        source = """
void Engine::stopFromIsr() {
    // An InterruptLock here would only hide nesting.
    if (hal::InterruptLock::activeCountForTest() != 0U) { return; }
}
"""
        self.assertEqual(violations({"engine.cpp": source}), [])

    def test_call_to_a_non_isr_variant_with_isr_sibling_is_reported(self) -> None:
        source = """
void Engine::stop() { stopFromIsr(); }
void Engine::stopFromIsr() {}
void Controller::lossFromIsr() { engine_.stop(); }
"""
        found = violations({"x.cpp": source})
        self.assertEqual(len(found), 1)
        self.assertIn("calls stop()", found[0])

    def test_repository_interrupt_paths_are_lock_free(self) -> None:
        sources = {
            path.relative_to(ROOT).as_posix(): path.read_text(encoding="utf-8")
            for path in sorted((ROOT / "src").rglob("*.cpp"))
        }
        self.assertEqual(violations(sources), [])


if __name__ == "__main__":
    unittest.main()
