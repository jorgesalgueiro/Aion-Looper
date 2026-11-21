// SPDX-License-Identifier: AGPL-3.0-only
const assert=require('node:assert/strict'),{histogram,locations,sourceRanges,classify}=require('../tools/AttributeDriveProfile.cjs');
function fixture(){const b=Buffer.alloc(69);b.write('gmon');b.writeUInt32LE(1,4);b.writeBigUInt64LE(4096n,21);b.writeBigUInt64LE(4112n,29);b.writeUInt32LE(4,37);b.writeUInt32LE(100,41);b.writeUInt16LE(3,61);b.writeUInt16LE(7,65);return b;}
const h=histogram(fixture());assert.equal(h.total,10);assert.deepEqual(h.bins,[{address:4096,count:3},{address:4104,count:7}]);
for(const mutate of [b=>b.write('xxxx'),b=>b.writeUInt32LE(2,4),b=>b[20]=1,b=>b.writeUInt32LE(0xffffffff,37),b=>b.writeUInt32LE(0,41),b=>b.writeBigUInt64LE(4095n,29),b=>b.fill(0,61)]){const b=fixture();mutate(b);assert.throws(()=>histogram(b));}
assert.throws(()=>histogram(fixture().subarray(0,63)));assert.throws(()=>histogram(Buffer.alloc(0)));
const text='0x00001000\naion::Drive::tick(float, float)\n/a/source/core/Drive.cpp:109\n0x00001008\naion::Drive::Down<8ul>::tick()\n/a/source/core/Drive.cpp:99 (discriminator 2)\n';
const parsed=locations(text,[4096,4104]);assert.equal(parsed[1][0].line,99);
assert.throws(()=>locations(text,[4104,4096]));assert.throws(()=>locations(text,[4096]));assert.throws(()=>locations('0x1000\nmissing pair',[4096]));
const r={toneStart:100,toneEnd:104,controlsStart:109,controlsEnd:110};
const frame=(name,line=96,file='/a/source/core/Drive.cpp')=>({name,line,file});
assert.equal(classify(parsed[0],r),'control_smoothing');assert.equal(classify(parsed[1],r),'downsampling_wet_and_dry');
assert.equal(classify([frame('aion::Drive::Up<5ul>::tick()')],r),'upsampling');
assert.equal(classify([frame('operator*',28,'/a/source/core/StereoFrame.h'),frame('aion::(anonymous namespace)::aa(...)'),frame('aion::Drive::stereo(...)')],r),'nonlinear');
assert.equal(classify([frame('aion::(anonymous namespace)::branch<8ul>(...)')],r),'resampling_unresolved');
assert.equal(classify([frame('aion::Drive::stereo(...)',102)],r),'dc_tone_and_mix');
assert.equal(classify([frame('aion::Drive::tick(...)',114)],r),'drive_other');
assert.equal(classify([frame('juce::String',12,'/a/JUCE.cpp')],r),'other_executable');
assert.equal(classify([{name:'??',file:'??',line:0}],r),'unresolved_executable');
assert.equal(locations('0x1000\n??\n??:?',[4096])[0][0].line,0);
assert.throws(()=>sourceRanges('unrelated code'));
const fs=require('node:fs'),path=require('node:path');const live=sourceRanges(fs.readFileSync(path.join(__dirname,'../source/core/Drive.cpp'),'utf8'));assert(live.toneStart<live.toneEnd&&live.controlsStart<live.controlsEnd);
console.log('PASS bounded profile histogram, malformed/truncated input rejection, address order, inline stacks and explicit unresolved attribution');
