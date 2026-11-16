// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <faust/gui/CInterface.h>
namespace aion::detail {
// The upstream C and C++ API headers define conflicting opaque DSP type names.
// Keep the official C UI entry point in its own translation unit.
void buildInterpreterUi(void* instance,UIGlue* ui);
}
