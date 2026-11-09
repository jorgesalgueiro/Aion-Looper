// SPDX-License-Identifier: AGPL-3.0-only
// Unchanged handler + effect setter and original widget bounds. No product JS edits.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const main=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8'),fx=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
const method=(source,name)=>{const a=source.indexOf('    static '+name+'('),b=source.indexOf('\n    }',a)+6;assert(a>=0&&b>a);return source.slice(a,b);};
const bands=[...fx.matchAll(/\$\{band\('[^']+', '([^']+)', '[FQG]', ([\d.-]+), ([\d.-]+), p\.[^)]+\)\}/g)].map(m=>[m[1],Number(m[2]),Number(m[3])]);assert.equal(bands.length,24);
const sandbox={document:{getElementById:()=>null,activeElement:null},window:{},AudioEngine:{currentTime:0},DroneSynth:{handleModWheel(){}},KeyMapManager:{setStatus(){}},localStorage:{setItem(){}},setTimeout:()=>0,clearTimeout(){}};vm.createContext(sandbox);
vm.runInContext(`const state={listeningForInput:{active:false},midiCCMap:{},midiLearn:{active:false},loops:Array.from({length:20},()=>({params:{eq:{}},activePresets:{eq:'Radio'},state:'playing',graph:{}}))};class EffectManager{static activeTab=19;static drawEQVisualizer(){} static updateLoopNode(){} ${method(fx,'update')}}class App{${method(main,'handleMIDIMessage')}};globalThis.oracle=(track,p,min,max,channel,value)=>{EffectManager.activeTab=track;state.midiCCMap[20+globalThis.parameter]={type:'effect',e:'eq',p,min,max};App.handleMIDIMessage({data:[175+channel,20+globalThis.parameter,value]});return state.loops[track].params.eq[p];};`,sandbox);
const input=[],expected=[];
for(let channel=1;channel<=16;++channel)for(const [p,[name,min,max]]of bands.entries())for(const value of [...Array(128).keys(),64,63,0,127,0]){
    const track=channel%2?19:0;sandbox.parameter=p;input.push(`${track} ${channel} ${p} ${value}`);expected.push(Math.fround(sandbox.oracle(track,name,min,max,channel,value)));
}
const run=spawnSync(process.argv[2],[],{input:'midicc\n'+input.join('\n')+'\n',encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(run.status,0,run.stderr);const actual=run.stdout.trim().split(/\s+/).map(Number);assert.equal(actual.length,expected.length);
for(let i=0;i<actual.length;++i)assert(Math.abs(actual[i]-expected[i])<=Math.max(1e-8,Math.abs(expected[i])*1e-8),`${input[i]}: ${actual[i]} != ${expected[i]}`); // Probe prints nine significant digits.
// First learn CC is consumed, even zero. The following same-number CC applies.
vm.runInContext(`state.midiCCMap={};state.midiLearn={active:true,type:'effect',e:'eq',p:'p1Gain',min:-24,max:24};state.loops[19].params.eq.p1Gain=3;EffectManager.activeTab=19;App.handleMIDIMessage({data:[176,24,0]});globalThis.consumed=state.loops[19].params.eq.p1Gain;App.handleMIDIMessage({data:[191,24,127]});globalThis.applied=state.loops[19].params.eq.p1Gain;`,sandbox);assert.equal(sandbox.consumed,3);assert.equal(sandbox.applied,24);
console.log(`PASS ${actual.length} EQ CC values: unchanged handler/setter, 24 original widget ranges, all channels, selected tabs 1/20, zero/repeats/nonmonotonic values; original learn consumes only its first packet`);
