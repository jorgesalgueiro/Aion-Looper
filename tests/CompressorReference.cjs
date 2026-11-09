// SPDX-License-Identifier: AGPL-3.0-only
// Execute unchanged original compressor construction/preset methods. No product
// AudioContext or preserved-resource edits. PCM is checked by CompressorWebAudio.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),effects=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8').replace(/\r\n/g,'\n'),loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
function method(source,start,end){const a=source.indexOf(start),b=source.indexOf(end,a);assert(a>=0&&b>a);return source.slice(a,b).trim();}
const names=['threshold','ratio','knee','attack','release','gain','mix'];
const initial=vm.runInNewContext('('+effects.match(/    compressor: (\{[^}]+\})/)[1]+')');
const defaults=names.map(name=>initial[name]);
const presets=vm.runInNewContext(method(effects,'const COMPRESSOR_PRESETS = {','\nconst DELAY_PRESETS = {')+'\nCOMPRESSOR_PRESETS');
const create=method(loops,'   _createCompressor(inputNode) {','    _createArpDelay(inputNode) {');
const apply=method(effects,'    static applyCompressorPreset(presetName) {','    /**');
const smooth=method(effects,'    static smoothSetParam(','    static updateLoopNode(');
function presetValues(name){
 const params={},state={loops:[{activePresets:{}}]};
 const Original=vm.runInNewContext(`(class {${apply}})`,{state,InputManager:{},DroneSynth:{}});
 Original.COMPRESSOR_PRESETS=presets;Original.activeTab=0;
 Original.update=(effect,param,value)=>{assert.equal(effect,'compressor');params[param]=value;};
 Original.drawCompressorCurve=()=>{};Original.renderEffectsPanel=()=>{};
 Original.applyCompressorPreset(name);assert.equal(state.loops[0].activePresets.compressor,name);
 return names.map(n=>params[n]);
}
function graphContract(){
 const nodes=[];function node(kind){const n={kind,links:[],connect(other){this.links.push(other);}};for(const p of ['threshold','ratio','knee','attack','release','gain'])n[p]={value:0,setValueAtTime(v,t){this.value=v;assert.equal(t,0);}};nodes.push(n);return n;}
 const Original=vm.runInNewContext(`(class {${create}})`,{state:{audioContext:{createDynamicsCompressor:()=>node('compressor'),createGain:()=>node('gain')}},AudioEngine:{currentTime:0},effects:{compressor:initial}});
 const graph=new Original();graph.loop={params:{}};const input=node('input'),result=graph._createCompressor(input);
 assert.deepEqual(Array.from(result.nodes,n=>n.kind),['compressor','gain','gain','gain','gain']);
 const [comp,makeup,dry,wet,out]=result.nodes;
 assert.deepEqual(input.links,[comp,dry]);assert.deepEqual(comp.links,[makeup]);assert.deepEqual(makeup.links,[wet]);assert.deepEqual(dry.links,[out]);assert.deepEqual(wet.links,[out]);
 for(const p of names.slice(0,5))assert.equal(comp[p].value,initial[p]);
 assert.equal(makeup.gain.value,2);assert.equal(dry.gain.value,0);assert.equal(wet.gain.value,1);
}
if(require.main===module){
 graphContract();const run=spawnSync(process.argv[2],[],{input:'presets\n',encoding:'utf8'});assert.equal(run.status,0,run.stderr);
 const native=JSON.parse(run.stdout);assert.equal(native.length,11);
 for(const [i,name]of Object.keys(presets).entries()){
  assert.equal(native[i].name,name);const expected=presetValues(name).map(Math.fround);
  assert.deepEqual(native[i].values.map(Math.fround),expected,name+' actual calculated makeup and stored Float32 parameters');
 }
 assert.notEqual(presetValues('default')[5],initial.gain,'Applying default preset differs from untouched default gain');
 console.log('PASS unchanged compressor graph: delayed wet, immediate dry, linear gain; all 11 preset handlers and exact Float32 values');
}
module.exports={create,smooth,names,defaults,presets,presetValues};
