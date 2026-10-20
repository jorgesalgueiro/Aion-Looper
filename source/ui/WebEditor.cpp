#include "ui/WebEditor.h"
#include "UiData.h"
#include <cstring>
std::optional<juce::WebBrowserComponent::Resource> AionWebEditor::resource(const juce::String& request) {
    const auto path = request == "/" ? juce::String("index.html") : request.substring(1);
    if (path.contains("..") || path.containsChar('\\') || path.containsChar('%') || path.containsChar('?')) return std::nullopt;
    juce::MemoryInputStream memory(AionUi::originalui_zip, AionUi::originalui_zipSize, false);
    juce::ZipFile archive(memory);
    const int index = archive.getIndexOfFileName(path, false);
    if (index < 0) return std::nullopt;
    std::unique_ptr<juce::InputStream> stream(archive.createStreamForEntry(index));
    if (!stream) return std::nullopt;
    juce::MemoryBlock data; stream->readIntoMemoryBlock(data);
    juce::String mime = "application/octet-stream";
    if (path.endsWith(".html")) {
        mime = "text/html";
        auto html = juce::String::fromUTF8(static_cast<const char*>(data.getData()), static_cast<int>(data.getSize()));
        // Runtime-only preview policy. Embedded source bytes remain untouched.
        // No legacy inline handlers, CDN imports, AudioContext or Worklets execute.
        html = html.replaceFirstOccurrenceOf("<head>", "<head><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'none'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'none'; base-uri 'none'; form-action 'none'\">");
        data.replaceAll(html.toRawUTF8(), static_cast<size_t>(html.getNumBytesAsUTF8()));
    } else if (path.endsWith(".css")) mime = "text/css";
    else if (path.endsWith(".js")) mime = "text/javascript";
    else if (path.endsWith(".ico")) mime = "image/x-icon";
    std::vector<std::byte> bytes(data.getSize());
    std::memcpy(bytes.data(), data.getData(), data.getSize());
    return juce::WebBrowserComponent::Resource{std::move(bytes), mime};
}
juce::WebBrowserComponent::Options AionWebEditor::options(AionProcessor& processor) {
    auto settings = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withNativeFunction("aionRequest", [&processor](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion done) { done(processor.request(args)); })
        .withResourceProvider(resource)
        .withUserScript(juce::String::fromUTF8(AionUi::bridge_js, AionUi::bridge_jsSize))
        .withUserScript(juce::String::fromUTF8(AionUi::loopview_js, AionUi::loopview_jsSize))
        .withUserScript(juce::String::fromUTF8(AionUi::controls_js, AionUi::controls_jsSize))
        .withUserScript(juce::String::fromUTF8(AionUi::midiview_js, AionUi::midiview_jsSize))
        .withUserScript(juce::String::fromUTF8(AionUi::midi_js, AionUi::midi_jsSize))
        .withUserScript(juce::String::fromUTF8(AionUi::looper_js, AionUi::looper_jsSize));
   #if JUCE_WINDOWS
    settings = settings.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
   #endif
    return settings;
}
AionWebEditor::AionWebEditor(AionProcessor& owner)
    : AudioProcessorEditor(owner), owner(owner), browser(owner,options(owner)) {
    addAndMakeVisible(browser); setResizable(true, true); setSize(1200, 800);
    browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
}
AionWebEditor::~AionWebEditor() {owner.releaseStutter();owner.cancelExport();owner.cancelMidiLearn();}
void AionWebEditor::resized() { browser.setBounds(getLocalBounds()); }
