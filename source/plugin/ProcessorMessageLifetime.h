#pragma once
#include <juce_events/juce_events.h>
#include <optional>

namespace aion {
// First base, destroyed last: retain ownership through all derived members,
// APVTS, AudioProcessor parameters and Timer bases. Never used by audio callbacks.
class ProcessorMessageLifetime {
protected:
    void retainMessageOwnershipForDestruction() {
        auto* manager=juce::MessageManager::getInstanceWithoutCreating();
        // An already stopped dispatch loop cannot grant a new lock. Its caller
        // must have quiesced callbacks before stopping global message dispatch.
        if(manager&&!manager->currentThreadHasLockedMessageManager()&&!manager->hasStopMessageBeenSent())
            shutdownLock.emplace();
    }
    ~ProcessorMessageLifetime()=default;
private:
    std::optional<juce::MessageManagerLock> shutdownLock;
};
}
