// SPDX-License-Identifier: AGPL-3.0-only
// Low-level pinned-bytecode regression for the embedding adaptation. This does
// not add a product bytecode loader or authorize arbitrary/unvalidated programs.
#include <faust/dsp/interpreter-dsp.h>
#include "faust/InterpreterInstance.h"
#include <fbc_opcode.hh>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
namespace {
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct FactoryDelete {void operator()(interpreter_dsp_factory* p)const{deleteInterpreterDSPFactory(p);}};
using Factory=std::unique_ptr<interpreter_dsp_factory,FactoryDelete>;
std::string error;
Factory compile(const std::string& source,bool ftz){
    const char* args[]{"-single","-scal","-ftz",ftz?"2":"0"};
    Factory f(createInterpreterDSPFactoryFromString("InstructionTest",source,4,args,error));
    if(!f)std::cerr<<error<<'\n';check(!!f,"Instruction fixture compilation");return f;
}
void replaceOpcode(std::string& text,FBCInstruction::Opcode before,FBCInstruction::Opcode after){
    const auto old="opcode "+std::to_string(before)+" "+gFBCInstructionTable[before];
    const auto replacement="opcode "+std::to_string(after)+" "+gFBCInstructionTable[after];
    const auto pos=text.find(old);check(pos!=std::string::npos,"Expected original opcode present");
    check(text.find(old,pos+old.size())==std::string::npos,"Exactly one opcode replaced");
    text.replace(pos,old.size(),replacement);
}
}
int main(){
    // Preserve valid compiler-generated heap/control/loop structure, replacing
    // only the two scalar cast opcodes with their representation-copy siblings.
    auto original=compile("process=int(_);",false);
    auto code=writeInterpreterDSPFactoryToBitcode(original.get());
    replaceOpcode(code,FBCInstruction::kCastInt,FBCInstruction::kBitcastInt);
    replaceOpcode(code,FBCInstruction::kCastReal,FBCInstruction::kBitcastReal);
    Factory bits(readInterpreterDSPFactoryFromBitcode(code,error));check(!!bits,"Bitcast fixture load");
    auto dsp=aion::detail::createInterpreterInstance(bits.get());check(!!dsp,"Bitcast instance");dsp->init(48000);
    std::array<float,128> input{},output{};float* in[]{input.data()},*out[]{output.data()};
    // Deliberately include signed zeros, denormals, infinities and NaN payloads:
    // representation copying must not perform floating arithmetic or conversions.
    constexpr std::uint32_t patterns[]{0,0x80000000,1,0x807fffff,0x00800000,0x80800000,
        0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc01234,0xffc05678};
    for(unsigned n:{1u,64u,128u}){
        for(unsigned i=0;i<n;++i)input[i]=std::bit_cast<float>(patterns[i%std::size(patterns)]);
        dsp->compute(static_cast<int>(n),in,out);
        for(unsigned i=0;i<n;++i)check(std::bit_cast<std::uint32_t>(input[i])==std::bit_cast<std::uint32_t>(output[i]),"Bitcast representation identity");
    }
    // A real compiler-generated recursive FTZ2 program executes kBitcastInt.
    auto ftz=compile("process=+~*(0.5);",true);
    check(writeInterpreterDSPFactoryToBitcode(ftz.get()).find("kBitcastInt")!=std::string::npos,"FTZ2 uses bitcast instruction");
    auto noFtz=compile("process=+~*(0.5);",false);
    for(unsigned block:{64u,128u}){
        auto f=aion::detail::createInterpreterInstance(ftz.get());
        auto g=aion::detail::createInterpreterInstance(noFtz.get());
        f->init(48000);g->init(48000);std::array<float,128> control{};float* normal[]{control.data()};
        for(unsigned pos=0;pos<256;pos+=block){
            input.fill(0);if(!pos)input[0]=1;
            f->compute(static_cast<int>(block),in,out);g->compute(static_cast<int>(block),in,normal);
            for(unsigned i=0;i<block;++i){const unsigned frame=pos+i;
                const float expected=frame<127?std::ldexp(1.f,-static_cast<int>(frame)):0.f;
                check(output[i]==expected,"FTZ2 preserves normals and clears recursive subnormals");
                check(control[i]==std::ldexp(1.f,-static_cast<int>(frame)),"Non-FTZ regression retains subnormal tail");
            }
        }
    }
    std::cout<<"PASS: bitcast payloads, signed zero, denormals and recursive FTZ2 at 64/128; switch/computed-goto platform coverage is separate\n";
}
