// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace aion {
inline constexpr std::string_view defaultSignalChain="QCATFODBVKZG";
inline constexpr std::string_view signalChainCodes="BVDTFOCKQAZGH";
// Original built-in codes, including effects not ported yet. No strings or
// dynamic routing storage on audio. Custom effect registration is still pending.
struct SignalChain {
    std::array<char,13> codes{'Q','C','A','T','F','O','D','B','V','K','Z','G',0};
    std::uint8_t size=12;
    constexpr std::string_view text() const noexcept {return {codes.data(),size};}
    constexpr bool contains(char code) const noexcept {return text().find(code)!=std::string_view::npos;}
    bool operator==(const SignalChain&) const noexcept = default;
};
constexpr bool validSignalChain(const SignalChain& chain) noexcept {
    if(!chain.size||chain.size>chain.codes.size())return false;
    for(unsigned i=0;i<chain.codes.size();++i){
        if(i>=chain.size){if(chain.codes[i])return false;continue;}
        if(signalChainCodes.find(chain.codes[i])==std::string_view::npos)return false;
        for(unsigned j=0;j<i;++j)if(chain.codes[j]==chain.codes[i])return false;
    }
    return true;
}
// Message/test owner normalizer, mirroring the original built-in chain setter.
// Admission limits external input length before this function is called.
constexpr SignalChain normalizedSignalChain(std::string_view input) noexcept {
    SignalChain chain;chain.codes={};chain.size=0;
    for(char c:input){
        if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
        if(signalChainCodes.find(c)!=std::string_view::npos&&!chain.contains(c))chain.codes[chain.size++]=c;
    }
    return chain.size?chain:SignalChain{};
}
}
