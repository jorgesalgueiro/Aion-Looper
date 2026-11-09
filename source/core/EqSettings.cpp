// SPDX-License-Identifier: AGPL-3.0-only
#include "core/EqSettings.h"
namespace aion {
EqValues eqPresetValues(unsigned id) noexcept {
    auto p=eqDefaults();
    // Original EQ_PRESETS overlays the original flat baseline. No other effect
    // or master-only presets are exposed through the per-loop EQ command.
    switch(id){
    case 1:p[0]=30;p[1]=80;p[2]=3;p[3]=400;p[4]=-4;p[5]=1;p[21]=8000;p[22]=3;p[23]=18000;break;
    case 2:p[0]=40;p[1]=120;p[2]=-2;p[3]=300;p[4]=-4;p[5]=.7f;p[21]=5000;p[22]=0;p[23]=22000;break;
    case 3:p[0]=100;p[1]=200;p[2]=-2;p[3]=3000;p[4]=3;p[5]=.7f;p[21]=10000;p[22]=2;p[23]=20000;break;
    case 4:p[0]=90;p[1]=200;p[2]=0;p[3]=1000;p[4]=-4;p[5]=2;p[21]=5000;p[22]=0;p[23]=20000;break;
    case 5:p[0]=80;p[1]=150;p[2]=0;p[3]=2500;p[4]=3;p[5]=.7f;p[21]=6000;p[22]=0;p[23]=12000;break;
    case 6:p[0]=30;p[1]=100;p[2]=0;p[3]=1000;p[4]=0;p[5]=.7f;p[21]=12000;p[22]=3;p[23]=22000;break;
    case 7:p[0]=30;p[1]=80;p[2]=4;p[3]=300;p[4]=-3;p[5]=1;p[21]=2000;p[22]=0;p[23]=5000;break;
    case 8:p[0]=300;p[1]=400;p[2]=-12;p[3]=1500;p[4]=8;p[5]=2;p[21]=4000;p[22]=-12;p[23]=5000;break;
    case 9:p[0]=50;p[1]=120;p[2]=1;p[3]=800;p[4]=-2;p[5]=.7f;p[21]=8000;p[22]=2;p[23]=16000;break;
    case 10:p[0]=400;p[1]=500;p[2]=-6;p[3]=2000;p[4]=4;p[5]=1.5f;p[21]=3000;p[22]=-8;p[23]=4000;break;
    default:break;
    }
    return p;
}
bool validEqSettings(const EqSettings& settings) noexcept {
    return validEqValues(settings.values)&&settings.preset<=eqPresetNames.size()
        &&(!settings.preset||settings.values==eqPresetValues(settings.preset));
}
}
