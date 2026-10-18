// Hardware-free host for real WebView mouse integration tests. Production
// processor, APVTS, IPC, editor and DSP are linked without modification.
#include "plugin/Processor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <thread>

class MouseWorkflowHost final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Aion mouse test host"; }
    const juce::String getApplicationVersion() override { return "1"; }
    void initialise(const juce::String& args) override {
        blockSize = args.contains("128") ? 128 : 64;
        processor = std::make_unique<AionProcessor>();
        processor->setPlayConfigDetails(2, 2, 48000, blockSize);
        processor->prepareToPlay(48000, blockSize);
        window = std::make_unique<Window>();
        window->setContentOwned(processor->createEditor(), true);
        window->centreWithSize(1200, 800);
        window->setVisible(true);
        running.store(true);
        audio = std::thread([this] {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            std::uint64_t frame = 0;
            auto deadline = std::chrono::steady_clock::now();
            while (running.load()) {
                for (int i = 0; i < blockSize; ++i, ++frame) {
                    buffer.setSample(0, i, 0.25f * std::sin(float(frame % 48000) * 0.057595865f));
                    buffer.setSample(1, i, 0.17f * std::sin(float(frame % 48000) * 0.079848813f));
                }
                processor->processBlock(buffer, midi);
                deadline += std::chrono::nanoseconds(1000000000LL * blockSize / 48000);
                std::this_thread::sleep_until(deadline);
            }
        });
    }
    void shutdown() override {
        running.store(false);
        if (audio.joinable()) audio.join();
        window.reset();
        processor->releaseResources();
        processor.reset();
    }
    void systemRequestedQuit() override { quit(); }
private:
    struct Window : juce::DocumentWindow {
        Window() : DocumentWindow("Aion mouse test (synthetic audio, no device)", juce::Colours::black, allButtons) { setUsingNativeTitleBar(true); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    int blockSize = 64;
    std::atomic<bool> running{false};
    std::thread audio;
    std::unique_ptr<AionProcessor> processor;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(MouseWorkflowHost)
JUCE_MAIN_FUNCTION_DEFINITION
