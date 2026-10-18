#include "plugin/Processor.h"
#include "ui/WebEditor.h"
#include <cmath>
#include <iterator>
AionProcessor::AionProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "AionState", {std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{"masterGain", 1}, "Master gain", juce::NormalisableRange<float>{0.0f, 2.0f}, 0.0f),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"inputMonitor", 1}, "Input monitor", false),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autoPlayAfterRecord", 1}, "Auto-play after Rec", true)}) {
    master = parameters.getRawParameterValue("masterGain");
    monitor = parameters.getRawParameterValue("inputMonitor");
    autoPlay = parameters.getRawParameterValue("autoPlayAfterRecord");
}
void AionProcessor::prepareToPlay(double rate, int) { engine.prepare(rate); }
bool AionProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet().isDisabled() || b.getMainInputChannelSet() == juce::AudioChannelSet::mono()
            || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}
void AionProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    const juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < audio.getNumChannels(); ++c) audio.clear(c, 0, audio.getNumSamples());
    if (getTotalNumInputChannels() == 1 && audio.getNumChannels() >= 2)
        audio.copyFrom(1, 0, audio, 0, 0, audio.getNumSamples());
    engine.setTarget(master->load(std::memory_order_relaxed));
    engine.setMonitoring(monitor->load(std::memory_order_relaxed) >= 0.5f);
    engine.setAutoPlay(autoPlay->load(std::memory_order_relaxed) >= 0.5f);
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
    const auto fail = [result, &reply](const char* reason) { result->setProperty("error", reason); return reply; };
    if (args.size() != 1 || args[0].getDynamicObject() == nullptr) return fail("Expected one request object");
    const auto& request = args[0];
    const auto operation = request.getProperty("operation", {}).toString();
    if (operation == "hello" || operation == "snapshot") {
        aion::EngineView view{};
        while (engine.popView(view)) latest = view;
        result->setProperty("ok", true); result->setProperty("stage", "native-loop-preview");
        result->setProperty("legacyAudioEnabled", false);
        const float masterValue = master->load();
        result->setProperty("masterGain", std::isfinite(masterValue) ? juce::jlimit(0.0f, 2.0f, masterValue) : 0.0f);
        result->setProperty("inputMonitor", monitor->load() >= 0.5f);
        result->setProperty("autoPlayAfterRecord", autoPlay->load() >= 0.5f);
        result->setProperty("generation", static_cast<int>(latest.generation));
        result->setProperty("audioReady", latest.generation != 0 && latest.generation == engine.generation());
        result->setProperty("frame", static_cast<juce::int64>(latest.frame));
        result->setProperty("sampleRate", latest.sampleRate);
        result->setProperty("freeFrames", static_cast<int>(latest.freeFrames));
        result->setProperty("inputPeak", latest.inputPeak); result->setProperty("outputPeak", latest.outputPeak);
        result->setProperty("lastRequestId", static_cast<juce::int64>(latest.lastRequestId));
        result->setProperty("rejected", latest.rejected); result->setProperty("poolFull", latest.poolFull);
        result->setProperty("audioReset", latest.audioReset);
        const char* names[] = {"empty", "recording", "playing", "stopped", "overdubbing", "substituting"};
        juce::Array<juce::var> tracks;
        for (const auto& t : latest.tracks) {
            auto* object = new juce::DynamicObject();
            object->setProperty("state", names[static_cast<unsigned>(t.state)]);
            object->setProperty("length", static_cast<int>(t.length)); object->setProperty("position", static_cast<int>(t.position));
            object->setProperty("pan", t.pan); object->setProperty("playbackRate", t.playbackRate);
            object->setProperty("volume", t.volume); object->setProperty("feedback", t.feedback);
            object->setProperty("solo", t.solo); object->setProperty("muted", t.muted); object->setProperty("peak", t.peak);
            tracks.add(juce::var(object));
        }
        result->setProperty("tracks", juce::var(tracks));
    } else if (operation == "setParameter") {
        const auto id = request.getProperty("id", {}).toString();
        if (id != "masterGain" && id != "inputMonitor" && id != "autoPlayAfterRecord") return fail("Unknown parameter");
        const auto value = request.getProperty("value", {});
        if (!(value.isDouble() || value.isInt())) return fail("Expected numeric parameter value");
        const double number = static_cast<double>(value);
        const double maximum = id == "masterGain" ? 2.0 : 1.0;
        if (!std::isfinite(number) || number < 0.0 || number > maximum
            || (id != "masterGain" && number != 0 && number != 1)) return fail("Parameter outside range");
        auto* parameter = parameters.getParameter(id);
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(number))); parameter->endChangeGesture();
        result->setProperty("ok", true);
    } else if (operation == "loop") {
        const auto expiration = request.getProperty("expiresAt", {});
        if (!(expiration.isInt() || expiration.isInt64() || expiration.isDouble())) return fail("Missing command expiration");
        const double expiresAt = static_cast<double>(expiration);
        const auto now = static_cast<double>(juce::Time::currentTimeMillis());
        if (!std::isfinite(expiresAt) || expiresAt < now || expiresAt > now + 5000.0) return fail("Command expired; refresh state before retrying");
        const auto id = request.getProperty("track", {});
        const auto generation = request.getProperty("generation", {});
        if (!id.isInt() || static_cast<int>(id) < 0 || static_cast<int>(id) >= static_cast<int>(aion::LoopBank::trackCount)
            || !generation.isInt() || static_cast<int>(generation) <= 0) return fail("Invalid track or audio generation");
        const auto actionName = request.getProperty("action", {}).toString();
        const char* names[] = {"record", "finish", "play", "stop", "overdub", "substitute", "clear", "stopAll", "volume", "feedback", "mute", "clearAll", "solo", "retrigger", "pan", "speed"};
        unsigned action = 0;
        for (; action < std::size(names) && actionName != names[action]; ++action) {}
        if (action == std::size(names)) return fail("Unknown looper action");
        const auto value = request.getProperty("value", 0);
        if (!(value.isDouble() || value.isInt())) return fail("Expected numeric action value");
        const double number = static_cast<double>(value);
        if (!std::isfinite(number) || number < (actionName == "speed" ? 0.25 : 0.0) || number > (actionName == "pan" ? 10 : (actionName == "volume" || actionName == "speed" ? 2 : 1))
            || (actionName == "mute" && number != 0 && number != 1)) return fail("Action value outside range");
        const auto requestId = nextRequest + 1;
        if (!engine.enqueueLoop({static_cast<aion::LoopAction>(action), static_cast<std::uint32_t>(static_cast<int>(id)), requestId,
                                static_cast<std::uint32_t>(static_cast<int>(generation)), 0, static_cast<float>(number)}))
            return fail("Audio generation changed or command queue is full");
        nextRequest = requestId;
        result->setProperty("ok", true); result->setProperty("queued", true);
        result->setProperty("requestId", static_cast<juce::int64>(requestId));
    } else return fail("Unknown operation");
    return reply;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AionProcessor(); }
