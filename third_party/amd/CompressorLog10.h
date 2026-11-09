//
// Copyright (C) 2008-2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
// OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
// Portable scalar C++ adaptation of the non-FMA AMD log10f algorithm.
// Pinned upstream: 5760c0a6a5c650aaeaee81d5d83592fc2da66207
// https://github.com/amd/aocl-libm-ose/blob/5760c0a6a5c650aaeaee81d5d83592fc2da66207/src/isa/avx/masm/log10f.asm
// No assembly, runtime dependency or processor dispatch is included.
#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace aion::detail {
inline constexpr std::array<std::uint32_t,129> compressorLogLead{{
    0x00000000u,0x3b5d4000u,0x3bdc8000u,0x3c24c000u,0x3c5ac000u,0x3c884000u,0x3ca2c000u,0x3cbd4000u,
    0x3cd78000u,0x3cf1c000u,0x3d05c000u,0x3d128000u,0x3d1f4000u,0x3d2c0000u,0x3d388000u,0x3d450000u,
    0x3d518000u,0x3d5dc000u,0x3d6a0000u,0x3d760000u,0x3d810000u,0x3d870000u,0x3d8d0000u,0x3d92c000u,
    0x3d98c000u,0x3d9e8000u,0x3da44000u,0x3daa0000u,0x3dafc000u,0x3db58000u,0x3dbb4000u,0x3dc0c000u,
    0x3dc64000u,0x3dcc0000u,0x3dd18000u,0x3dd6c000u,0x3ddc4000u,0x3de1c000u,0x3de70000u,0x3dec8000u,
    0x3df1c000u,0x3df70000u,0x3dfc4000u,0x3e00c000u,0x3e034000u,0x3e05c000u,0x3e088000u,0x3e0b0000u,
    0x3e0d8000u,0x3e100000u,0x3e128000u,0x3e150000u,0x3e178000u,0x3e1a0000u,0x3e1c8000u,0x3e1ec000u,
    0x3e214000u,0x3e23c000u,0x3e260000u,0x3e288000u,0x3e2ac000u,0x3e2d4000u,0x3e2f8000u,0x3e31c000u,
    0x3e344000u,0x3e368000u,0x3e38c000u,0x3e3b0000u,0x3e3d4000u,0x3e3fc000u,0x3e420000u,0x3e440000u,
    0x3e464000u,0x3e488000u,0x3e4ac000u,0x3e4d0000u,0x3e4f4000u,0x3e514000u,0x3e538000u,0x3e55c000u,
    0x3e57c000u,0x3e5a0000u,0x3e5c0000u,0x3e5e4000u,0x3e604000u,0x3e624000u,0x3e648000u,0x3e668000u,
    0x3e688000u,0x3e6ac000u,0x3e6cc000u,0x3e6ec000u,0x3e70c000u,0x3e72c000u,0x3e74c000u,0x3e76c000u,
    0x3e78c000u,0x3e7ac000u,0x3e7cc000u,0x3e7ec000u,0x3e804000u,0x3e814000u,0x3e824000u,0x3e834000u,
    0x3e840000u,0x3e850000u,0x3e860000u,0x3e870000u,0x3e880000u,0x3e88c000u,0x3e89c000u,0x3e8ac000u,
    0x3e8bc000u,0x3e8c8000u,0x3e8d8000u,0x3e8e8000u,0x3e8f4000u,0x3e904000u,0x3e914000u,0x3e920000u,
    0x3e930000u,0x3e93c000u,0x3e94c000u,0x3e958000u,0x3e968000u,0x3e978000u,0x3e984000u,0x3e994000u,
    0x3e9a0000u,
}};
inline constexpr std::array<std::uint32_t,129> compressorLogTail{{
    0x00000000u,0x367a8e44u,0x368ed49fu,0x36c21451u,0x375211d6u,0x3720ea11u,0x37e9eb59u,0x37b87be7u,
    0x37bf2560u,0x33d597a0u,0x37806a05u,0x3820581fu,0x38223334u,0x378e3bacu,0x3810684fu,0x37feb7aeu,
    0x36a9d609u,0x37a68163u,0x376a8b27u,0x384c8fd6u,0x3885183eu,0x3874a760u,0x380d1154u,0x38ea42bdu,
    0x384c1571u,0x38ba66b8u,0x38e7da3bu,0x38eee632u,0x38d00911u,0x388bbedeu,0x378a0512u,0x3894c7a0u,
    0x38e30710u,0x36db2829u,0x3729d609u,0x38fa0e82u,0x38bc9a75u,0x383a9297u,0x38dc83c8u,0x37eac335u,
    0x38706ac3u,0x389574c2u,0x3892d068u,0x38615032u,0x3917acf4u,0x3967a126u,0x38217840u,0x38b420abu,
    0x38f9c7b2u,0x391103bdu,0x39169a6bu,0x390dd194u,0x38eda471u,0x38a38950u,0x37f6844au,0x395e1cdbu,
    0x390fcffcu,0x38503e9du,0x394b00fdu,0x38a9910au,0x39518a31u,0x3882d2c2u,0x392488e4u,0x397b0affu,
    0x388a22d8u,0x3902bd5eu,0x39342f85u,0x39598811u,0x3972e6b1u,0x34d53654u,0x360ca25eu,0x39785cc0u,
    0x39630710u,0x39424ed7u,0x39165101u,0x38be5421u,0x37e7b0c0u,0x394fd0c3u,0x38efaaaau,0x37a8f566u,
    0x3927c744u,0x383fa4d5u,0x392d9e39u,0x3803feaeu,0x390a268cu,0x39692b80u,0x38789b4fu,0x3909307du,
    0x394a601cu,0x35e67edcu,0x383e386du,0x38a7743du,0x38dccec3u,0x38ff57e0u,0x39079d8bu,0x390651a6u,
    0x38f7bad9u,0x38d0ab82u,0x38979e7du,0x381978eeu,0x397816c8u,0x39410cb2u,0x39015384u,0x3863fa28u,
    0x39f41065u,0x39c7668au,0x39968afau,0x39430db9u,0x38a18cf3u,0x39eb2907u,0x39a9e10cu,0x39492800u,
    0x385a53d1u,0x39ce0cf7u,0x3979c7b2u,0x389f5d99u,0x39ceefcbu,0x39646a39u,0x380d7a9bu,0x39ad6650u,
    0x390ac3b8u,0x39d9a9a8u,0x39548a99u,0x39f73c4bu,0x3980960eu,0x374b3d5au,0x39888f1eu,0x37679a07u,
    0x39826a13u,
}};
inline constexpr std::array<std::uint32_t,129> compressorLogInverse{{
    0x40000000u,0x3ffe03f8u,0x3ffc0fc1u,0x3ffa232du,0x3ff83e10u,0x3ff6603eu,0x3ff4898du,0x3ff2b9d6u,
    0x3ff0f0f1u,0x3fef2eb7u,0x3fed7304u,0x3febbdb3u,0x3fea0ea1u,0x3fe865acu,0x3fe6c2b4u,0x3fe52598u,
    0x3fe38e39u,0x3fe1fc78u,0x3fe07038u,0x3fdee95cu,0x3fdd67c9u,0x3fdbeb62u,0x3fda740eu,0x3fd901b2u,
    0x3fd79436u,0x3fd62b81u,0x3fd4c77bu,0x3fd3680du,0x3fd20d21u,0x3fd0b6a0u,0x3fcf6475u,0x3fce168au,
    0x3fcccccdu,0x3fcb8728u,0x3fca4588u,0x3fc907dau,0x3fc7ce0cu,0x3fc6980cu,0x3fc565c8u,0x3fc43730u,
    0x3fc30c31u,0x3fc1e4bcu,0x3fc0c0c1u,0x3fbfa030u,0x3fbe82fau,0x3fbd6910u,0x3fbc5264u,0x3fbb3ee7u,
    0x3fba2e8cu,0x3fb92144u,0x3fb81703u,0x3fb70fbbu,0x3fb60b61u,0x3fb509e7u,0x3fb40b41u,0x3fb30f63u,
    0x3fb21643u,0x3fb11fd4u,0x3fb02c0bu,0x3faf3adeu,0x3fae4c41u,0x3fad602bu,0x3fac7692u,0x3fab8f6au,
    0x3faaaaabu,0x3fa9c84au,0x3fa8e83fu,0x3fa80a81u,0x3fa72f05u,0x3fa655c4u,0x3fa57eb5u,0x3fa4a9cfu,
    0x3fa3d70au,0x3fa3065eu,0x3fa237c3u,0x3fa16b31u,0x3fa0a0a1u,0x3f9fd80au,0x3f9f1166u,0x3f9e4cadu,
    0x3f9d89d9u,0x3f9cc8e1u,0x3f9c09c1u,0x3f9b4c70u,0x3f9a90e8u,0x3f99d723u,0x3f991f1au,0x3f9868c8u,
    0x3f97b426u,0x3f97012eu,0x3f964fdau,0x3f95a025u,0x3f94f209u,0x3f944581u,0x3f939a86u,0x3f92f114u,
    0x3f924925u,0x3f91a2b4u,0x3f90fdbcu,0x3f905a38u,0x3f8fb824u,0x3f8f177au,0x3f8e7835u,0x3f8dda52u,
    0x3f8d3dcbu,0x3f8ca29cu,0x3f8c08c1u,0x3f8b7034u,0x3f8ad8f3u,0x3f8a42f8u,0x3f89ae41u,0x3f891ac7u,
    0x3f888889u,0x3f87f781u,0x3f8767abu,0x3f86d905u,0x3f864b8au,0x3f85bf37u,0x3f853408u,0x3f84a9fau,
    0x3f842108u,0x3f839930u,0x3f83126fu,0x3f828cc0u,0x3f820821u,0x3f81848eu,0x3f810204u,0x3f808081u,
    0x3f800000u,
}};
inline float compressorLog10(float x) noexcept {
    auto bits=std::bit_cast<std::uint32_t>(x);
    if(!(bits&0x7fffffffu))return -std::numeric_limits<float>::infinity();
    if(bits&0x80000000u)return std::numeric_limits<float>::quiet_NaN();
    if(bits>=0x7f800000u)return x;
    const auto f=[](std::uint32_t u){return std::bit_cast<float>(u);};
    float exponent=static_cast<float>(static_cast<int>(bits>>23)-127);
    if((bits>>23)==0){
        const float normal=f((bits&0x7fffffu)|0x3f800000u)-1;
        bits=std::bit_cast<std::uint32_t>(normal);
        exponent=static_cast<float>(static_cast<int>(bits>>23)-253);
    }
    const float difference=x-1;
    if(difference<.0625f&&difference>-.0625f){
        const float r=x-1,u=r/(2+r),correction=r*u,v=u+u,v2=v*v;
        const float cubic=v*v2,polynomial=f(0x3daaaaabu)+v2*f(0x3c4ccccdu);
        const float q=cubic*polynomial-correction;
        const float top=f(std::bit_cast<std::uint32_t>(r)&0xffff0000u);
        const float low=r-top,delta=q+low;
        const float a=top*f(0x3a37b152u),b=delta*f(0x3a37b152u);
        const float c=delta*f(0x3ede0000u),d=top*f(0x3ede0000u);
        return ((a+b)+c)+d;
    }
    const auto mantissa=(bits&0x7f0000u)+((bits&0x8000u)<<1);
    const auto index=mantissa>>16;
    const float fraction=f(mantissa|0x3f000000u)-f((bits&0x7fffffu)|0x3f000000u);
    const float r=fraction*f(compressorLogInverse[index]);
    const float r2=r*r,polynomial=r*f(0x3eaaaaabu)+.5f;
    const float correction=r+polynomial*r2;
    const float low=(exponent*f(0x39826a13u)-correction*f(0x3ede5bd9u))+f(compressorLogTail[index]);
    const float high=exponent*f(0x3e9a0000u)+f(compressorLogLead[index]);
    return high+low;
}
}
