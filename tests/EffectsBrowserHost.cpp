// SPDX-License-Identifier: AGPL-3.0-only
// Test-only WebView2 document with Blob-worklet permission. No processor,
// audio device, native product bootstrap, original-asset edits or installer.
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstring>
namespace {
class Window final:public juce::DocumentWindow {
 static juce::WebBrowserComponent::Options options(){
  return juce::WebBrowserComponent::Options{}
   .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
   .withResourceProvider([](const juce::String& path)->std::optional<juce::WebBrowserComponent::Resource>{
    if(path!="/"&&path!="/index.html")return std::nullopt;
    const juce::String html="<!doctype html><html><head><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'self' blob:; worker-src 'self' blob:; connect-src 'self' blob:\"><title>Effects audio test oracle</title></head><body id=\"effects-oracle\">Test-only offline AudioWorklet</body></html>";
    std::vector<std::byte> bytes(static_cast<std::size_t>(html.getNumBytesAsUTF8()));std::memcpy(bytes.data(),html.toRawUTF8(),bytes.size());
    return juce::WebBrowserComponent::Resource{std::move(bytes),"text/html"};
   });
 }
 juce::WebBrowserComponent browser{options()};
public:
 Window():DocumentWindow("Aion effects sound oracle",juce::Colours::black,DocumentWindow::closeButton){
  setContentNonOwned(&browser,false);centreWithSize(640,240);setVisible(true);browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
 }
 void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
};
class App final:public juce::JUCEApplication {
 std::unique_ptr<Window> window;
public:
 const juce::String getApplicationName() override{return "Aion effects browser test";}
 const juce::String getApplicationVersion() override{return "1";}
 bool moreThanOneInstanceAllowed() override{return true;}
 void initialise(const juce::String&) override{window=std::make_unique<Window>();}
 void shutdown() override{window.reset();}
};
}
START_JUCE_APPLICATION(App)
JUCE_MAIN_FUNCTION_DEFINITION
