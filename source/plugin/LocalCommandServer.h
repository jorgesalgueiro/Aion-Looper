#pragma once
#include <juce_events/juce_events.h>
#include <functional>
#include <filesystem>
#include <array>
#include <cmath>
#include <algorithm>

namespace aion {
// Standalone control only. One fixed mailbox/instance, one request per message
// tick. All files, JSON, callbacks and destruction stay off the audio callback.
// Clients serialize through an OS-held lock and atomically replace request.json.
class LocalCommandServer final : private juce::Timer {
public:
    using Handler=std::function<juce::var(const juce::var&)>;
    static constexpr juce::int64 maxRequestBytes=32768,maxReplyBytes=2097152;
    static juce::File defaultRoot(){
        const auto overridePath=juce::SystemStats::getEnvironmentVariable("AION_CLI_DIR",{});
        if(overridePath.isNotEmpty())return juce::File::isAbsolutePath(overridePath)?juce::File(overridePath):juce::File{};
       #if JUCE_LINUX || JUCE_BSD
        const auto config=juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME",{});
        const auto base=config.isEmpty()?juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".config"):
            juce::File::isAbsolutePath(config)?juce::File(config):juce::File{};
        return base==juce::File{}?base:base.getChildFile("Aion-Looper/cli");
       #else
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Aion-Looper/cli");
       #endif
    }
    explicit LocalCommandServer(Handler next,juce::File root=defaultRoot()):handler(std::move(next)),
        instance(juce::Uuid().toString()),token(juce::Uuid().toString()),directory(root==juce::File{}?root:root.getChildFile(instance)){
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        if(directory==juce::File{})return;
        if(directory.createDirectory().failed())return;
       #if ! JUCE_WINDOWS
        std::error_code error;
        std::filesystem::permissions(directory.getFullPathName().toStdString(),std::filesystem::perms::owner_all,error);
        if(error)return;
       #endif
        auto* info=new juce::DynamicObject();juce::var metadata(info);
        info->setProperty("protocol",1);info->setProperty("instance",instance);info->setProperty("token",token);
        if(!directory.getChildFile("endpoint.json").replaceWithText(juce::JSON::toString(metadata,true)))return;
        available=true;startTimer(25);
    }
    ~LocalCommandServer() override {
        JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED
        stopTimer();
        if(directory==juce::File{})return;
        // Only our fixed files in our freshly generated instance directory.
        for(const auto* name:{"endpoint.json","request.json","processing.json","response.json","client.lock"})directory.getChildFile(name).deleteFile();
        directory.deleteFile(); // Empty-directory removal only; never recursive.
    }
    bool ready() const noexcept {return available;}
    const juce::String& id() const noexcept {return instance;}
private:
    static bool boundedDepth(const char* text,int length) noexcept {
        unsigned depth=0;bool quoted=false,escaped=false;
        for(int i=0;i<length;++i){const char c=text[i];
            if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
            else if(c=='"')quoted=true;
            else if(c=='{'||c=='['){if(++depth>32)return false;}
            else if(c=='}'||c==']'){if(!depth)return false;--depth;}
        }
        return !quoted&&!depth;
    }
    void timerCallback() override {
        if(servicing)return;
        const juce::ScopedValueSetter<bool> guard(servicing,true);
        const auto incoming=directory.getChildFile("request.json"),input=directory.getChildFile("processing.json");
        if(input.existsAsFile()||!incoming.existsAsFile()||!incoming.moveFileTo(input))return;
        juce::var envelope;
        std::array<char,static_cast<std::size_t>(maxRequestBytes)+1> payload{};
        auto stream=input.createInputStream();
        const int bytes=stream?stream->read(payload.data(),static_cast<int>(payload.size())):0;
        stream.reset();
        if(bytes>0&&bytes<=maxRequestBytes&&std::find(payload.begin(),payload.begin()+bytes,'\0')==payload.begin()+bytes
            &&juce::CharPointer_UTF8::isValidString(payload.data(),bytes)&&boundedDepth(payload.data(),bytes))
            envelope=juce::JSON::parse(juce::String::fromUTF8(payload.data(),bytes));
        if(!input.deleteFile())return; // Claim before applying; never replay a failed unlink.
        const auto idValue=envelope.getProperty("id",{});
        const auto requestId=idValue.isString()?idValue.toString():juce::String{};
        auto* response=new juce::DynamicObject();juce::var packet(response);
        response->setProperty("protocol",1);response->setProperty("id",requestId);
        auto* failure=new juce::DynamicObject();juce::var failureResult(failure),result=failureResult;failure->setProperty("ok",false);
        const double now=static_cast<double>(juce::Time::currentTimeMillis());
        const auto expiry=envelope.getProperty("expiresAt",{});
        const auto version=envelope.getProperty("protocol",{}),credential=envelope.getProperty("token",{});
        const bool numeric=expiry.isInt()||expiry.isInt64()||expiry.isDouble();
        if(bytes<=0||bytes>maxRequestBytes)failure->setProperty("error","CLI request exceeds byte limit");
        else if(!envelope.isObject()||!version.isInt()||static_cast<int>(version)!=1
            ||!credential.isString()||credential.toString()!=token
            ||!idValue.isString()||requestId.length()!=32||!requestId.containsOnly("0123456789abcdef")
            ||!envelope["request"].isObject())failure->setProperty("error","Invalid CLI envelope");
        else if(!numeric||!std::isfinite(static_cast<double>(expiry))||static_cast<double>(expiry)<now
            ||static_cast<double>(expiry)>now+10000)failure->setProperty("error","CLI request expired or invalid deadline");
        else if(requestId==lastId)result=lastResult; // Most recent reply only; no client resubmission policy.
        else {
            try{result=handler(envelope["request"]);}catch(...){failure->setProperty("error","CLI handler failed; inspect state before retrying");}
            lastId=requestId;lastResult=result;
        }
        response->setProperty("result",result);
        auto text=juce::JSON::toString(packet,true);
        if(static_cast<juce::int64>(text.getNumBytesAsUTF8())>maxReplyBytes){
            failure->setProperty("error","CLI reply exceeds byte limit; inspect state before retrying");
            response->setProperty("result",failureResult);text=juce::JSON::toString(packet,true);
        }
        directory.getChildFile("response.json").replaceWithText(text);
    }
    Handler handler;
    const juce::String instance,token;
    const juce::File directory;
    juce::String lastId;
    juce::var lastResult;
    bool available=false,servicing=false;
};
}
