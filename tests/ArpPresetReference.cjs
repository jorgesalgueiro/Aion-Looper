// SPDX-License-Identifier: AGPL-3.0-only
// Unchanged original preset/update/live methods determine actual write order.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const source=fs.readFileSync(path.join(__dirname,'../resources/ui/Modules/effects.js'),'utf8').replace(/\r\n/g,'\n');
const {presets,hostDefaults}=require('./ArpDelayReference.cjs');
function method(start,end){const a=source.indexOf(start),b=source.indexOf(end,a+start.length);assert(a>=0&&b>a,start);return source.slice(a,b).trim();}
const methods=[method('    static update(effectType, param, value) {','    static smoothSetParam('),method('    static updateLoopNode(','    static renderEffectsPanel('),method('    static applyGenericPreset(effectName, presetName) {','    /**')];
const names=['time','feedback','mix','stay','scale','sync','bpm','amplitude','range','humanize','panSpeed','panDepth'];
const cases=[],expected=[];
for(const bpm of [1,30,120,137.123456789,173.123456789,300,999])for(const [preset,values]of Object.entries(presets)){
 const writes=[],params={...hostDefaults},nodes=[{parameters:new Map(names.slice(0,10).map(name=>[name,{name}]))},{},{frequency:{name:'panSpeed'}},{gain:{name:'panDepth'}}];
 const loop={params:{arpDelay:params},activePresets:{},state:'playing',graph:{nodes:{effects:{arpDelay:nodes}}}},state={bpm,loops:[loop],customEffects:{}};
 const Original=vm.runInNewContext(`(class {\n${methods.join('\n')}\n})`,{state,effects:{arpDelay:hostDefaults},document:{getElementById:()=>null},AudioEngine:{currentTime:1}});
 Original.ARPDELAY_PRESETS=presets;Original.activeTab=0;Original.renderEffectsPanel=()=>{};
 Original.smoothSetParam=(p,v)=>{assert(p);writes.push([names.indexOf(p.name),Math.fround(p.name==='bpm'?Math.max(30,Math.min(300,v)):v)]);};
 Original.applyGenericPreset('arpDelay',preset);
 assert.equal(writes.length,22);assert.equal(loop.activePresets.arpDelay,preset);assert.equal(Original.isBatchUpdating,false);
 for(const [key,value]of Object.entries(values))assert.equal(params[key],value);
 cases.push(`${bpm} ${Object.keys(presets).indexOf(preset)+1}`);expected.push(writes);
}
const result=spawnSync(process.argv[2],['preset-writes'],{input:cases.join('\n')+'\n',encoding:'utf8',maxBuffer:1024*1024});assert.equal(result.status,0,result.stderr||result.stdout);
assert.deepEqual(result.stdout.trim().split(/\r?\n/).map(JSON.parse),expected);
console.log(JSON.stringify({result:'PASS',cases:cases.length,writes:cases.length*22,scope:'Unchanged original preset/update methods, ordered controls and repeated BPM writes; audio trajectories are a separate gate'}));
