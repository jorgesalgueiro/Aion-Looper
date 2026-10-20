#include "core/Wav.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace aion {
std::vector<std::uint8_t> encodeWav(std::span<const float> left,std::span<const float> right,double sampleRate,unsigned channels) {
    if(channels<1||channels>2||(channels==2&&right.size()!=left.size())||left.size()>(std::numeric_limits<std::uint32_t>::max()-36u)/(2u*channels))
        throw std::length_error("WAV exceeds RIFF capacity");
    if(!std::isfinite(sampleRate))sampleRate=44100;
    // DataView.setUint32 truncates and wraps, rather than rounding.
    const auto uint32=[](double n){if(!std::isfinite(n))return std::uint32_t{0};double value=std::fmod(std::trunc(n),4294967296.0);if(value<0)value+=4294967296.0;return static_cast<std::uint32_t>(value);};
    const auto length=static_cast<std::uint32_t>(left.size()*channels*2);
    std::vector<std::uint8_t> bytes(44ull+length);
    const auto put=[&](std::size_t offset,std::uint32_t value,unsigned count){for(unsigned i=0;i<count;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8));};
    const auto text=[&](unsigned offset,const char* value){for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<std::uint8_t>(value[i]);};
    text(0,"RIFF");put(4,36+length,4);text(8,"WAVE");text(12,"fmt ");put(16,16,4);put(20,1,2);put(22,channels,2);
    put(24,uint32(sampleRate),4);put(28,uint32(sampleRate*channels*2),4);put(32,channels*2,2);put(34,16,2);text(36,"data");put(40,length,4);
    std::size_t offset=44;
    for(std::size_t i=0;i<left.size();++i)for(unsigned c=0;c<channels;++c){
        const double raw=c?right[i]:left[i],sample=std::isfinite(raw)?std::clamp(raw,-1.0,1.0):0;
        const auto value=static_cast<std::int16_t>(sample*(sample<0?32768:32767));
        put(offset,static_cast<std::uint16_t>(value),2);offset+=2;
    }
    return bytes;
}
}
