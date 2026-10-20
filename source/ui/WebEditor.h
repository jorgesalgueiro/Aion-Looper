#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "plugin/Processor.h"
class AionWebEditor final : public juce::AudioProcessorEditor {
public:
    explicit AionWebEditor(AionProcessor&);
    ~AionWebEditor() override;
    void resized() override;
private:
    class Browser final : public juce::WebBrowserComponent {
    public:
        Browser(AionProcessor& processor,const Options& options) : WebBrowserComponent(options),owner(processor) {}
        bool pageAboutToLoad(const juce::String& url) override {
            owner.cancelExport(); // Native navigation precedes unload RPC/new-page bootstrap.
            owner.cancelMidiLearn();
            return url.startsWith(getResourceProviderRoot()) || url == "about:blank";
        }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    private:
        AionProcessor& owner;
    };
    static juce::WebBrowserComponent::Options options(AionProcessor&);
    static std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String&);
    AionProcessor& owner;
    Browser browser;
};
