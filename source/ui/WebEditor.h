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
    class Browser final : public juce::WebBrowserComponent
       #if JUCE_WINDOWS
        , private juce::ComponentMovementWatcher
       #endif
    {
    public:
        Browser(AionProcessor& processor,const Options& options) : WebBrowserComponent(options),
           #if JUCE_WINDOWS
            ComponentMovementWatcher(this),
           #endif
            owner(processor) {}
        bool pageAboutToLoad(const juce::String& url) override {
            owner.releaseStutter(); // Navigation must not depend on unload RPC/new-page bootstrap.
            owner.cancelExport();
            owner.cancelMidiLearn();
            const bool allowed=url.startsWith(getResourceProviderRoot()) || url == "about:blank";
           #if JUCE_WINDOWS
            if(allowed&&isShowing()&&!restoring)visibleDocument=url;
           #endif
            return allowed;
        }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    private:
       #if JUCE_WINDOWS
        // JUCE's Windows hide policy calls goBack on show before the asynchronous
        // blank navigation necessarily enters history. Own the SAME unloading
        // policy, restoring an explicit document rather than relying on history.
        void componentMovedOrResized(bool,bool) override {}
        void componentPeerChanged() override {componentVisibilityChanged();}
        void componentVisibilityChanged() override {
            const bool showing=isShowing();if(showing==wasShowing)return;
            wasShowing=showing;
            if(showing){
                if(wasHidden){restoring=true;if(blankLoaded)restoreDocument();}
            }else{
                wasHidden=true;restoring=true;blankPending=true;blankLoaded=false;
                owner.releaseStutter();owner.cancelExport();owner.cancelMidiLearn();
                goToURL("about:blank");
            }
        }
        void restoreDocument(){
            if(visibleDocument=="about:blank")restoring=false;
            else goToURL(visibleDocument);
        }
        void pageFinishedLoading(const juce::String& url) override {
            if(url=="about:blank"&&blankPending){
                // Wait for the old document to unload, including its pagehide
                // cancellation, before restoring after even a zero-ms hide.
                blankPending=false;blankLoaded=true;if(isShowing())restoreDocument();
            }else if(isShowing()&&!blankPending&&url==visibleDocument){restoring=false;blankLoaded=url=="about:blank";}
        }
        juce::String visibleDocument=getResourceProviderRoot();
        bool wasShowing=false,wasHidden=false,restoring=false,blankPending=false,blankLoaded=false;
       #endif
        AionProcessor& owner;
    };
    static juce::WebBrowserComponent::Options options(AionProcessor&);
    static std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String&);
    AionProcessor& owner;
    Browser browser;
};
