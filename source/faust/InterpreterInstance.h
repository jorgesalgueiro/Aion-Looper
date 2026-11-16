// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <faust/dsp/interpreter-dsp.h>
#include <memory>
#include <new>
namespace aion::detail {
// OFF audio, with the embedding's compiler/factory mutex held where shared.
// Capture the allocator while the object/factory are alive. Faust 2.88.0's
// class operator delete instead reads fFactory after the DSP destructor, which
// is undefined behavior. The explicit destructor still unregisters the instance
// and releases its DSP resources before returning storage to its original owner.
struct InterpreterInstanceDelete {
    dsp_memory_manager* manager=nullptr;
    void operator()(interpreter_dsp* p)const noexcept {
        if(!p)return;
        p->~interpreter_dsp();
        if(manager)manager->destroy(p);else ::operator delete(static_cast<void*>(p));
    }
};
using InterpreterInstance=std::unique_ptr<interpreter_dsp,InterpreterInstanceDelete>;
inline InterpreterInstance createInterpreterInstance(interpreter_dsp_factory* factory){
    const auto manager=factory->getMemoryManager();
    return InterpreterInstance(factory->createDSPInstance(),InterpreterInstanceDelete{manager});
}
}
