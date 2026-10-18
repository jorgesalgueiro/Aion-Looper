#include "plugin/Processor.h"
#include "ui/WebEditor.h"
#include <cmath>
AionProcessor::AionProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AionState", {std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{"masterGain", 1}, "Master gain", juce::NormalisableRange<float>{0.0f, 2.0f}, 0.0f)}) {
    master = parameters.getRawParameterValue("masterGain");
}
void AionProcessor::prepareToPlay(double rate, int) { engine.prepare(rate); }
bool AionProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet().isDisabled() || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}
void AionProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    const juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < audio.getNumChannels(); ++c) audio.clear(c, 0, audio.getNumSamples());
    engine.setTarget(master->load(std::memory_order_relaxed));
    engine.transport = {};
    if (auto* head = getPlayHead()) if (auto position = head->getPosition()) {
        engine.transport.hostValid = true;
        engine.transport.playing = position->getIsPlaying();
        if (auto bpm = position->getBpm(); bpm && std::isfinite(*bpm) && *bpm > 0.0) engine.transport.bpm = *bpm;
        if (auto ppq = position->getPpqPosition(); ppq && std::isfinite(*ppq)) engine.transport.ppq = *ppq;
    }
    engine.process(audio.getArrayOfWritePointers(), audio.getNumChannels(), audio.getNumSamples());
}
juce::AudioProcessorEditor* AionProcessor::createEditor() { return new AionWebEditor(*this); }
void AionProcessor::getStateInformation(juce::MemoryBlock& data) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, data);
}
void AionProcessor::setStateInformation(const void* data, int size) {
    if (size <= 0 || size > 1024 * 1024) return;
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName("AionState")) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::var AionProcessor::request(const juce::Array<juce::var>& args) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    auto* result = new juce::DynamicObject(); juce::var reply(result);
    result->setProperty("protocol", 1); result->setProperty("ok", false);
    if (args.size() != 1 || args[0].getDynamicObject() == nullptr) return reply;
    const auto operation = args[0].getProperty("operation", {}).toString();
    if (operation == "hello") {
        result->setProperty("ok", true); result->setProperty("stage", "architecture-foundation");
        result->setProperty("legacyAudioEnabled", false); result->setProperty("masterGain", master->load());
    } else if (operation == "setParameter" && args[0].getProperty("id", {}).toString() == "masterGain") {
        const auto value = args[0].getProperty("value", {});
        if (!(value.isDouble() || value.isInt())) return reply;
        const double number = static_cast<double>(value);
        if (!std::isfinite(number) || number < 0.0 || number > 2.0) return reply;
        auto* parameter = parameters.getParameter("masterGain");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(number))); parameter->endChangeGesture();
        result->setProperty("ok", true);
    }
    return reply;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AionProcessor(); }
