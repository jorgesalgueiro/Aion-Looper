#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Engine.h"
static_assert(std::atomic<float>::is_always_lock_free, "Audio parameter loads must be lock-free");
class AionProcessor final : public juce::AudioProcessor {
public:
    AionProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Aion-Looper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::var request(const juce::Array<juce::var>&); // Message thread only.
    juce::AudioProcessorValueTreeState parameters;
private:
    aion::Engine engine;
    std::atomic<float>* master = nullptr;
    std::atomic<float>* monitor = nullptr;
    std::atomic<float>* autoPlay = nullptr;
    std::atomic<float> *syncMode=nullptr,*syncBpm=nullptr,*syncBars=nullptr,*syncNum=nullptr,*syncDen=nullptr,*countIn=nullptr;
    std::atomic<float> *metroEnabled=nullptr,*metroVolume=nullptr;
    aion::EngineView latest{}; // Message-thread cached, coherent audio snapshot.
    // Session labels belong to the message thread, never the audio callback.
    std::array<juce::String, aion::LoopBank::trackCount> trackNames{};
    std::array<std::uint64_t, aion::LoopBank::trackCount> nameClearEpochs{};
    std::uint32_t nameGeneration = 0;
    std::uint32_t nextRequest = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AionProcessor)
};
