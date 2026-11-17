// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DriveSettings.h"
namespace aion {
// Persistent fixed-storage 4x oversampled native nonlinear family. Preparation
// is exclusive/off audio. Controls are bounded audio-owner updates; no tables,
// allocators, compiler, file access or locks are used in process/tick/reset.
class Drive {
public:
 bool prepare(double,DriveKind,const DriveSettings&)noexcept;
 bool set(const DriveSettings&)noexcept;
 void reset()noexcept;
 bool process(const float*,const float*,float*,float*,unsigned)noexcept;
 std::array<float,2> tick(float,float)noexcept;
 bool active()const noexcept{return ready&&(!sleeping||bypassTarget!=0);}
 // Pure double precision antiderivative evaluation for independent verification.
 static double transfer(DriveKind,double)noexcept;
 static double integrated(DriveKind,double)noexcept;
 static double antialiased(DriveKind,double,double)noexcept;
private:
 struct Halfband {
  std::array<double,8> state{};double delayed=0;
  std::array<double,2> up(double,bool)noexcept;
  double down(std::array<double,2>,bool)noexcept;
 };
 struct Channel {
  Halfband up1,up2,down1,down2,dry1,dry2;
  double previous=0,zeroPrevious=0,dryPrevious=0,dcInput=0,dcOutput=0,ic1=0,ic2=0;
 };
 std::array<Channel,2> channels{};
 DriveValues current{},target{};
 double rate=48000,smoothing=0,bypass=0,bypassTarget=0,dcPole=0;
 DriveKind kind=DriveKind::distortion;bool ready=false,sleeping=true;
 DriveValues mapped(const DriveSettings&)const noexcept;
 double channel(Channel&,double)noexcept;
};
}
