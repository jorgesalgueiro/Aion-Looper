// SPDX-License-Identifier: AGPL-3.0-only
// Original factory/update/preset methods execute unchanged. This tests the
// settings/target adapter, not AudioParam trajectories or a product CLI.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),probe=process.argv[2];assert(probe,'settings test executable required');
const source=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8').replace(/\r\n/g,'\n');
const {factory,hostDefaults,presets}=require('./ArpDelayReference.cjs');
function method(start,end){const a=source.indexOf(start),b=source.indexOf(end,a+start.length);assert(a>=0&&b>a,start);return source.slice(a,b).trim();}
const update=method('    static update(effectType, param, value) {','    static smoothSetParam(');
const live=method('    static updateLoopNode(','    static renderEffectsPanel(');
const apply=method('    static applyGenericPreset(effectName, presetName) {','    /**');
const scaleNames=vm.runInNewContext('('+source.match(/const ARPDELAY_SCALES = (\[[\s\S]*?\n\]);/)[1]+')');
function run(mode,input=''){const r=spawnSync(probe,[mode],{input,encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(r.status,0,r.stderr||r.stdout);return r.stdout.trim();}
const catalog=JSON.parse(run('catalog')),names=Object.keys(hostDefaults),dsp=['time','feedback','mix','stay','scale','sync','bpm','amplitude','range','humanize','panSpeed','panDepth'];
assert.deepEqual(catalog.controls.map(c=>c.name),names);assert.deepEqual(catalog.controls.map(c=>c.initial),names.map(n=>hostDefaults[n]));assert.deepEqual(catalog.scales,Array.from(scaleNames));
assert.deepEqual(catalog.presets.map(p=>p.name),Object.keys(presets));
for(const p of catalog.presets){
 const params={...hostDefaults},state={loops:[{activePresets:{},params:{arpDelay:params}}]};
 const Original=vm.runInNewContext(`(class {${apply}})`,{state});Original.ARPDELAY_PRESETS=presets;Original.activeTab=0;
 Original.update=(effect,key,value)=>{assert.equal(effect,'arpDelay');params[key]=value;};Original.renderEffectsPanel=()=>{};
 Original.applyGenericPreset('arpDelay',p.name);
 assert.equal(state.loops[0].activePresets.arpDelay,p.name);assert.deepEqual(p.values,names.map(n=>params[n]));
}
// Slider bounds come from the original markup, not a second hand-written table.
for(const control of catalog.controls){
 if(['stay','sync','scale'].includes(control.name))continue;
 const suffix=control.name[0].toUpperCase()+control.name.slice(1),m=source.match(new RegExp(`'arpDelay${suffix}', ([\\d.]+), ([\\d.]+)`));assert(m,control.name);
 assert.equal(control.min,Number(m[1]));assert.equal(control.widgetMax,Number(m[2]));
}
assert.match(source,/id="arpDelayScale" min="0" max="\$\{EffectManager.ARPDELAY_SCALES.length-1\}"/);
const cases=[],expected=[];
function factoryTargets(params,bpm){
 let targets;const node=()=>({connect(){},frequency:{},gain:{},start(){}});
 const state={bpm,audioContext:{createStereoPanner:node,createOscillator:node,createGain:node}};
 const Original=vm.runInNewContext(`(class {${factory}})`,{state,effects:{arpDelay:hostDefaults},Log:{error(...args){assert.fail(args.join(' '));}},AudioWorkletNode:function(ctx,id,options){targets=options.parameterData;return node();}});
 const graph=new Original();graph.loop={params:{arpDelay:params}};const result=graph._createArpDelay(node());
 return dsp.map((n,p)=>Math.fround(p===10?result.nodes[2].frequency.value:p===11?result.nodes[3].gain.value:n==='bpm'?Math.max(30,Math.min(300,targets[n])):targets[n]));
}
function add(params,bpm,mask,preset=0,targets=factoryTargets(params,bpm),enabled=true){
 const seed=0xFEDCBA98;cases.push(`${bpm} ${seed} ${mask} ${enabled?1:0} ${preset} ${names.map(n=>Object.is(params[n],-0)?'-0':params[n]).join(' ')}`);
 expected.push({ok:true,mask:mask?maskFromControls(mask):0,seed,enabled,values:targets});
}
function maskFromControls(mask){let result=1<<6;for(let p=0;p<names.length;++p)if(mask&(1<<p))result|=1<<dsp.indexOf(names[p]==='repetitions'?'feedback':names[p]);return result;}
for(const bpm of [1,29.999999,30,120,137.123456789,300,301,999]){
 add({...hostDefaults},bpm,0,0,undefined,false);
 for(let p=0;p<catalog.presets.length;++p)add({...hostDefaults,...presets[catalog.presets[p].name]},bpm,2047,p+1);
}
add({...hostDefaults,time:4,amplitude:2,scale:3.5,stay:1,sync:1},137,2047);
add({...hostDefaults,stay:-0,sync:-0},137,2047);
// Actual original live handler determines which targets and BPM are written.
for(let p=0;p<names.length;++p){
 const control=catalog.controls[p],key=control.name;
 const values=['stay','sync'].includes(key)?[0,1]:[control.min,control.widgetMax,(control.min+control.widgetMax)/2];
 // Continuous stored repetitions must divide in double precision before fround.
 if(key==='repetitions')values.push(.07,1.23456789,6.123456789,9.9);
 for(const value of values){
  const params={...hostDefaults},calls=[],initial=factoryTargets(params,137),actual=[...initial];
  const nodes=[{parameters:new Map(dsp.slice(0,10).map(n=>[n,{name:n}]))},{},{frequency:{name:'panSpeed'}},{gain:{name:'panDepth'}}];
  const loop={params:{arpDelay:params},activePresets:{arpDelay:'frozen'},state:'playing',graph:{nodes:{effects:{arpDelay:nodes}}}},state={bpm:173.123456789,loops:[loop],customEffects:{}};
  const Original=vm.runInNewContext(`(class {\n${update}\n${live}\n})`,{state,effects:{arpDelay:hostDefaults},document:{getElementById:()=>null},AudioEngine:{currentTime:1}});
  Original.activeTab=0;Original.smoothSetParam=(param,v)=>{assert(param);calls.push(param.name);actual[dsp.indexOf(param.name)]=Math.fround(v);};
  Original.update('arpDelay',key,value);
  assert.equal(loop.activePresets.arpDelay,'');assert.equal(params[key],value);assert.deepEqual(calls,[key==='repetitions'?'feedback':key,'bpm']);
  add(params,state.bpm,1<<p,0,actual);
 }
}
// Exhaustively compare bit selection, including no-gesture and full preset masks.
for(let mask=0;mask<2048;++mask)add({...hostDefaults},120,mask);
const actual=run('compile',cases.join('\n')+'\n').split('\n').map(JSON.parse);
assert.deepEqual(actual,expected);
console.log(JSON.stringify({result:'PASS',scope:'Unchanged original Arp controls, presets, factory/live target conversion and masks; no product UI/CLI/state or envelope claim',cases:cases.length,controls:names.length,presets:catalog.presets.length,scales:catalog.scales.length,exactFloat32Targets:true}));
