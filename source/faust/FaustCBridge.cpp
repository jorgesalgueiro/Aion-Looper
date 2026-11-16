// SPDX-License-Identifier: AGPL-3.0-only
#include "FaustCBridge.h"
#include <stdint.h>
#include <faust/dsp/interpreter-dsp-c.h>
namespace aion::detail {
void buildInterpreterUi(void* instance,UIGlue* ui){
    buildUserInterfaceCInterpreterDSPInstance(static_cast<interpreter_dsp*>(instance),ui);
}
}
