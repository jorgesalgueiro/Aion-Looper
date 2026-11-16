// SPDX-License-Identifier: AGPL-3.0-only
// Execute the unchanged factory, generic preset handler and curve builder.
// This is NOT WebAudio interpolation/oversampling, tone-filter or sound parity.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),probe=process.argv[2];assert(probe,'Native settings executable required');
const read=name=>fs.readFileSync(path.join(root,'resources/ui/Modules',name),'utf8').replace(/\r\n/g,'\n');
const effects=read('effects.js'),graph=read('loopTracks.js');
function section(source,start,end){const a=source.indexOf(start),b=source.indexOf(end,a+start.length);assert(a>=0&&b>a,start);return source.slice(a,b).trim();}
const curve=section(graph,'    static makeDistortionCurve(amount, shape) {','    _createFuzz(');
const factory=section(graph,'    _createDistortion(inputNode) {','    static makeDistortionCurve(');
const apply=section(effects,'    static applyGenericPreset(effectName, presetName) {','    /**');
// If the independent checkout exists, verify that the preserved oracle matches it.
const reference=path.join(root,'.reference/Ascii-Looper/Modules');
if(fs.existsSync(path.join(reference,'loopTracks.js'))){
 const original=fs.readFileSync(path.join(reference,'loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
 assert.equal(section(original,'    static makeDistortionCurve(amount, shape) {','    _createFuzz('),curve);
 assert.equal(section(original,'    _createDistortion(inputNode) {','    static makeDistortionCurve('),factory);
}
const defaults=vm.runInNewContext('('+effects.match(/    distortion: (\{[\s\S]*?\n    \}),/)[1]+')');
const presets=vm.runInNewContext('('+effects.match(/const DISTORTION_PRESETS = (\{[\s\S]*?\n\});/)[1]+')');
const markup=vm.runInNewContext('({'+section(effects,"    'T': { key: 'distortion'","    'F': { key: 'fuzz'")+'}).T');
const run=(mode,input='')=>{const r=spawnSync(probe,[mode],{input,encoding:'utf8',maxBuffer:96*1024*1024});assert.equal(r.status,0,r.stderr||r.error?.message||r.stdout.slice(-500));return r.stdout.trim();};
const catalog=JSON.parse(run('catalog')),names=Object.keys(defaults);
assert.equal(catalog.curveSize,4097);assert.deepEqual(catalog.controls.map(c=>c.name),names);
for(const [p,c]of catalog.controls.entries()){
 const widget=markup.controls.find(w=>w.p===c.name);assert(widget,c.name);
 assert.equal(c.initial,defaults[c.name]);assert.equal(c.min,widget.min);assert.equal(c.max,widget.max);assert.equal(c.step,widget.step);
 assert.equal(names[p],c.name);
}
assert.deepEqual(catalog.presets.map(p=>p.name),Object.keys(presets));
for(const preset of catalog.presets){
 const params={...defaults},state={loops:[{activePresets:{},params:{distortion:params}}]};
 const Original=vm.runInNewContext(`(class {${apply}})`,{state});Original.DISTORTION_PRESETS=presets;Original.activeTab=0;
 Original.update=(effect,key,value)=>{assert.equal(effect,'distortion');params[key]=value;};Original.renderEffectsPanel=()=>{};
 Original.applyGenericPreset('distortion',preset.name);
 assert.equal(state.loops[0].activePresets.distortion,preset.name);assert.deepEqual(preset.values,names.map(n=>params[n]));
}
const node=()=>({connect(){},gain:{value:1},frequency:{value:350}});
const context={createGain:node,createWaveShaper:node,createBiquadFilter:node};
const Original=vm.runInNewContext(`(class AudioGraph {${factory}\n${curve}})`,{state:{audioContext:context},effects:{distortion:defaults},Float32Array});
function expected(values){const g=new Original();g.loop={params:{distortion:Object.fromEntries(names.map((n,p)=>[n,values[p]]))}};
 const result=g._createDistortion(node()),n=result.nodes;
 assert.equal(n[1].oversample,'4x');assert.equal(n[2].type,'lowpass');assert.equal(n[2].frequency.value,values[2]);
 assert.equal(n[3].gain.value,values[3]);assert.equal(n[4].gain.value,1-values[4]);assert.equal(n[5].gain.value,values[4]);
 return n[1].curve;
}
const cases=[names.map(n=>defaults[n]),...catalog.presets.map(p=>p.values)];
function add(amount,shape){cases.push([amount,shape,defaults.tone,defaults.volume,defaults.mix]);}
for(let amount=0;amount<=100;++amount)for(const shape of [0,.5,1])add(amount,shape);
for(let shape=0;shape<=100;++shape)for(const amount of [0,40,100])add(amount,shape/100);
for(const amount of [1e-12,.0001,.123456789,39.999999999,99.999999999])for(const shape of [.000001,.123456789,.499999999,.999999999])add(amount,shape);
for(let p=0;p<names.length;++p)for(let cc=0;cc<128;++cc){const values=names.map(n=>defaults[n]),c=catalog.controls[p];values[p]=(cc/127)*(c.max-c.min)+c.min;cases.push(values);}
const rows=run('curves',cases.map(v=>v.join(' ')).join('\n')+'\n').split(/\r?\n/);assert.equal(rows.length,cases.length);
const order=bits=>(bits&0x80000000)?0x80000000-(bits&0x7fffffff):0x80000000+bits;
let maxUlp=0,maxAbsoluteError=0,samples=0;
for(let c=0;c<cases.length;++c){
 const want=expected(cases[c]),wanted=Buffer.from(want.buffer,want.byteOffset,want.byteLength),actual=Buffer.from(rows[c],'hex');
 assert.equal(actual.length,4097*4);assert.equal(actual.readUInt32LE(2048*4),0,'Exact positive-zero anchor');
 for(let i=0;i<4097;++i){const value=actual.readFloatLE(i*4),ulp=Math.abs(order(actual.readUInt32LE(i*4))-order(wanted.readUInt32LE(i*4)));
  assert(Number.isFinite(value)&&value>=-1&&value<=1);assert(ulp<=2,`Curve ${c} sample ${i}: ${ulp} ULP`);
  if(i)assert(value>=actual.readFloatLE((i-1)*4),'Monotonic curve');
  maxUlp=Math.max(maxUlp,ulp);maxAbsoluteError=Math.max(maxAbsoluteError,Math.abs(value-want[i]));++samples;
 }
}
console.log(JSON.stringify({result:'PASS',cases:cases.length,samples,maxUlp,maxAbsoluteError,toleranceFloat32Ulp:2,presets:6,oracle:'Unchanged original distortion factory/generic preset handler/makeDistortionCurve; preparation only, T audio path remains unported'}));
