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
 // Exact no-state-change return predicate; a reset during a fade still ticks.
 bool isBypassed()const noexcept{return !ready||(sleeping&&bypass==0&&bypassTarget==0);}
 // Pure double precision antiderivative evaluation for independent verification.
 static double transfer(DriveKind,double)noexcept;
 static double integrated(DriveKind,double)noexcept;
 static double antialiased(DriveKind,double,double)noexcept;
private:
 using Frame=drive_detail::Frame;
 template<std::size_t N> struct Up {
  std::array<Frame,N> state{};
  std::array<Frame,2> tick(Frame)noexcept;
 };
 template<std::size_t N> struct Down {
  std::array<Frame,N> state{};Frame delayed{};
  Frame tick(std::array<Frame,2>)noexcept;
 };
 struct Path {
  Up<8> up1;Up<5> up2;
  Down<8> down1,dry1;Down<5> down2,dry2;
  Frame previous{},primitive{},dryPrevious{},dcInput{},dcOutput{},ic1{},ic2{};
  double zeroPrevious=0; // Bias history is identical in both independent lanes.
  double toneIntegrator=-1,toneA1=0; // Derived coefficients, invalidated by reset.
 };
 Path path{};
 DriveValues current{},target{};
 double rate=48000,smoothing=0,bypass=0,bypassTarget=0,dcPole=0;
 DriveKind kind=DriveKind::distortion;bool ready=false,sleeping=true,controlsMoving=false;
 DriveValues mapped(const DriveSettings&)const noexcept;
 Frame stereo(Frame)noexcept;
};
}
