/**
 * @file startup_and_games.inc
 * @brief Startup order of the gate outputs, output-stage release and boot-game cases.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

namespace {

bool gFirstStorageReadSeen = false;
bool gGatesLowOutputsAtFirstRead = false;

/** Storage read hook: records the gate pins at the first read of the persistent store. */
void captureGatesAtFirstStorageRead() {
    if (gFirstStorageReadSeen) {
        return;
    }
    gFirstStorageReadSeen = true;
    bool ready = true;
    for (const std::uint32_t pin : pinmap::kGateChannelPins) {
        const auto mode = fakefw::pinModes.find(pin);
        const auto value = fakefw::pinValues.find(pin);
        ready = ready && mode != fakefw::pinModes.end() && mode->second == OUTPUT &&
            value != fakefw::pinValues.end() && value->second == LOW;
    }
    gGatesLowOutputsAtFirstRead = ready;
}

}  // namespace

void testGatesAreLowOutputsBeforePersistenceLoads() {
    // F-023 / W-099: the gate outputs are configured and LOW before anything else in begin(),
    // in particular before the persistent state is read.
    resetFakes();
    prepareDisplaySuccess();
    hal::PersistentStorage::resetForTest();
    commitCurrentBpm(117U, 10U);
    hal::PersistentStorage::simulateRebootForTest();
    fakefw::delayAdvanceOverrideMs = 333U;
    gFirstStorageReadSeen = false;
    gGatesLowOutputsAtFirstRead = false;
    hal::PersistentStorage::setReadHookForTest(captureGatesAtFirstStorageRead);
    app::ClockApplication application;
    application.begin();
    hal::PersistentStorage::setReadHookForTest(nullptr);
    CHECK(gFirstStorageReadSeen);
    CHECK(gGatesLowOutputsAtFirstRead);
    CHECK_EQ(application.bpmForTest(), 117U);
}

void testGateDriverWritesLowBeforeSwitchingToOutput() {
    // V3 / RM0368 8.3.1, 8.4.6: the output data bit is LOW before the pin becomes an output.
    resetFakes();
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    for (const std::uint32_t pin : pinmap::kGateChannelPins) {
        std::size_t firstLowWrite = fakefw::pinEvents.size();
        std::size_t firstOutputMode = fakefw::pinEvents.size();
        for (std::size_t index = 0U; index < fakefw::pinEvents.size(); ++index) {
            const fakefw::PinEvent& event = fakefw::pinEvents[index];
            if (event.pin != pin) {
                continue;
            }
            if (!event.modeChange && event.value == LOW && firstLowWrite == fakefw::pinEvents.size()) {
                firstLowWrite = index;
            }
            if (event.modeChange && event.value == OUTPUT && firstOutputMode == fakefw::pinEvents.size()) {
                firstOutputMode = index;
            }
        }
        CHECK(firstOutputMode < fakefw::pinEvents.size());
        CHECK(firstLowWrite < firstOutputMode);
        CHECK(!gates.channelStateHigh(0U));
    }
}

void testEnablingTheOutputStageStartsEngineAndDriverLow() {
    // F-031 / W-108: if the engine fires while the output stage is muted, it records a gate
    // HIGH that the driver holds LOW. Enabling the stage must start both sides LOW.
    const auto runUntilEngineGateHigh = [](engine::ClockEngine& engine) {
        std::uint32_t ticks = 0U;
        while (engine.gateHighMaskForTest() == 0U && ticks++ < 20'000U) {
            engine.beginSchedulerQuantumFromIsr();
            engine.processSchedulerTick();
        }
        CHECK(ticks < 20'000U);
    };
    ClockState state = makeDefaultState();
    state.operatingMode = OperatingMode::UnifiedClock;

    // Counter-check with the former sequence: the engine keeps its HIGH, the driver is LOW.
    resetFakes();
    hal::GateOutputDriver oldGates;
    oldGates.beginDisabled();
    engine::ClockEngine oldEngine(oldGates);
    oldEngine.begin(state);
    oldEngine.play();
    runUntilEngineGateHigh(oldEngine);
    oldGates.setAllChannelsLow();
    oldGates.enableOutputStage();
    CHECK(oldEngine.gateHighMaskForTest() != 0U);
    CHECK(!oldGates.channelStateHigh(0U));

    resetFakes();
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    engine.play();
    runUntilEngineGateHigh(engine);
    CHECK(!gates.channelStateHigh(0U));
    const std::uint32_t locksBefore = hal::InterruptLock::constructionCountForTest();
    engine.enableOutputStage();
    CHECK_EQ(hal::InterruptLock::constructionCountForTest(), locksBefore + 1U);  // one step
    CHECK(gates.outputStageEnabled());
    CHECK_EQ(engine.gateHighMaskForTest(), 0U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(!gates.channelStateHigh(channel));
    }
    // The next event raises the gate at the jack.
    std::uint32_t ticks = 0U;
    while (!gates.channelStateHigh(0U) && ticks++ < 20'000U) {
        engine.beginSchedulerQuantumFromIsr();
        engine.processSchedulerTick();
    }
    CHECK(ticks < 20'000U);
    CHECK((engine.gateHighMaskForTest() & 0x01U) != 0U);
}

void testFailedSpiPageIsSentAgain() {
    // F-043 / W-113: a page whose SPI transfer failed stays dirty and is sent with the next
    // frame, even when its content did not change in between.
    if constexpr (config::kDisplayTransport == config::DisplayTransport::Spi) {
        resetFakes();
        prepareDisplaySuccess();
        hal::OledDisplay display;
        CHECK(display.begin());
        display.clear();
        display.present();
        display.fillRectangle(0, 16, 20, 8);  // page 2
        SPI.failTransactions = 1U;            // the page's transfer fails
        display.present();
        const std::size_t page2 = 2U * hal::OledDisplay::kWidth;
        CHECK_EQ(display.presentedFramebufferForTest()[page2], 0U);
        const std::size_t sentAfterFailure = SPI.transfers.size();
        display.present();  // same content: the failed page goes out again
        CHECK(SPI.transfers.size() > sentAfterFailure);
        CHECK(display.presentedFramebufferForTest()[page2] != 0U);
        const std::size_t sentAfterRetry = SPI.transfers.size();
        display.present();  // nothing left to send
        CHECK_EQ(SPI.transfers.size(), sentAfterRetry);
    }
}
