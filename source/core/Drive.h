// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DriveSettings.h"
#include "DriveFrame.h"
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
 using Frame=drive_detail::Frame;
 struct Halfband {
  std::array<Frame,8> state{};Frame delayed{};
  std::array<Frame,2> up(Frame,bool)noexcept;
  Frame down(std::array<Frame,2>,bool)noexcept;
 };
 struct Path {
  Halfband up1,up2,down1,down2,dry1,dry2;
  Frame previous{},primitive{},dryPrevious{},dcInput{},dcOutput{},ic1{},ic2{};
  double zeroPrevious=0; // Bias history is identical in both independent lanes.
 };
 Path path{};
 DriveValues current{},target{};
 double rate=48000,smoothing=0,bypass=0,bypassTarget=0,dcPole=0;
 DriveKind kind=DriveKind::distortion;bool ready=false,sleeping=true;
 DriveValues mapped(const DriveSettings&)const noexcept;
 Frame stereo(Frame)noexcept;
};
}
