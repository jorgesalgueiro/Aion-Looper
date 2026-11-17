// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "DistortionSettings.h"
#include <span>
namespace aion {
enum class DriveKind:std::uint8_t {distortion,overdrive,fuzz};
inline constexpr std::array<std::string_view,3> driveNames{"distortion","overdrive","fuzz"};
inline constexpr std::array<char,3> driveCodes{'T','O','F'};
inline constexpr std::array<std::array<unsigned,2>,14> driveMidiParameters{{{0,0},{0,1},{0,2},{0,3},{0,4},{1,0},{1,2},{1,3},{1,4},{2,0},{2,1},{2,2},{2,3},{2,4}}};
// Fixed slots: drive/amount/gain, shape/bias (unused for O), tone, volume, mix.
using DriveValues=std::array<double,5>;
inline constexpr std::uint32_t allDriveFields=(1u<<18)-1;
struct DriveSettings {DriveValues values{};bool enabled=false;std::uint8_t preset=0;bool operator==(const DriveSettings&)const noexcept=default;};
struct DrivePreset {std::string_view name;DriveValues values;};
std::span<const DistortionControl,5> driveControls(DriveKind)noexcept;
std::span<const DrivePreset> drivePresets(DriveKind)noexcept;
DriveSettings driveDefaults(DriveKind)noexcept;
bool validDriveSettings(DriveKind,const DriveSettings&)noexcept;
bool setDriveControl(DriveKind,DriveSettings&,unsigned,double)noexcept;
bool applyDrivePreset(DriveKind,DriveSettings&,unsigned)noexcept;
using DriveBank=std::array<DriveSettings,3>;
inline DriveBank defaultDriveBank()noexcept{return {driveDefaults(DriveKind::distortion),driveDefaults(DriveKind::overdrive),driveDefaults(DriveKind::fuzz)};}
inline bool validDriveBank(const DriveBank& bank)noexcept{for(unsigned i=0;i<3;++i)if(!validDriveSettings(static_cast<DriveKind>(i),bank[i]))return false;return true;}
}
