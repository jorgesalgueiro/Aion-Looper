// SPDX-License-Identifier: AGPL-3.0-only
// Execute the UNCHANGED original worklet, with deterministic test-only randomness.
// Direct worklet invocations, not browser AudioParam/panner/device acceptance.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
const original=source.match(/class ArpDelayProcessor extends AudioWorkletProcessor \{[\s\S]*?registerProcessor\('arp-delay-processor', ArpDelayProcessor\);/)[0];
const graph=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
const factory=graph.slice(graph.indexOf('    _createArpDelay(inputNode) {'),graph.indexOf('\n    /**',graph.indexOf('    _createArpDelay(inputNode) {'))).trim();
const presets=vm.runInNewContext('('+source.match(/const ARPDELAY_PRESETS = (\{[\s\S]*?\n\});/)[1]+')');
const hostDefaults=vm.runInNewContext('('+source.match(/    arpDelay: (\{[\s\S]*?\n    \}),/)[1]+')');
const names=['time','feedback','mix','stay','scale','sync','bpm','amplitude','range','humanize'];
const values=p=>[p.time,p.repetitions/11.1,p.mix,p.stay,p.scale,p.sync,120,p.amplitude,p.range,p.humanize].map(Math.fround);
function harness(rate,seed){
 let random=seed>>>0,draws=0,Processor;
 const math=Object.create(Math);math.random=()=>{random=(Math.imul(random,1664525)+1013904223)>>>0;++draws;return random/4294967296;};
 vm.runInNewContext(original,{AudioWorkletProcessor:class{},getWorkletSampleRate:()=>rate,Math:math,Float32Array,registerProcessor:(name,p)=>{assert.equal(name,'arp-delay-processor');Processor=p;}});
 return {create:()=>new Processor(),draws:()=>draws,reset:next=>{random=next>>>0;draws=0;},descriptors:Processor.parameterDescriptors};
}
function graphContract(){
 let parameters,oscillator=0;
 const node=()=>({connect(){},parameters:new Map(),frequency:{},gain:{},start(){++oscillator;}});
 const initial={...hostDefaults},state={bpm:137,audioContext:{createStereoPanner:node,createOscillator:node,createGain:node}};
 const Graph=vm.runInNewContext(`(class {${factory}})`,{state,effects:{arpDelay:initial},Log:{error(...args){assert.fail(args.join(' '));}},AudioWorkletNode:function(context,type,options){assert.equal(type,'arp-delay-processor');parameters=options.parameterData;return node();}});
 const g=new Graph();g.loop={params:{}};const result=g._createArpDelay(node());
 assert.equal(result.nodes.length,4);assert.equal(oscillator,1);assert.equal(parameters.feedback,2/11.1);assert.equal(parameters.bpm,137);assert.equal(result.nodes[2].frequency.value,0);assert.equal(result.nodes[3].gain.value,.8);
 const expectedRanges=[['Time',.01,2],['Repetitions',0,10],['Mix',0,1],['Amplitude',0,1.5],['Range',.25,3],['Humanize',0,1],['PanSpeed',0,5],['PanDepth',0,1]];
 for(const [suffix,min,max]of expectedRanges){const match=source.match(new RegExp(`'arpDelay${suffix}', ([\\d.]+), ([\\d.]+)`));assert(match,suffix);assert.equal(Number(match[1]),min);assert.equal(Number(match[2]),max);}
 assert.equal(Object.keys(presets).length,8);
 console.log('PASS unchanged original Arp Delay factory/defaults, 8 presets, ranges and always-present post-worklet pan stage');
}
function state(p,randomDraws){return {write:p.wPtr,clock:p.arpClock,sequenceIndex:p.seqIdx,step:p.arpStep,direction:p.arpDir,phase:p.gPhase,semitone:p.currentSemi,grainRate:p.gRate,grainTarget:p.gRateTarget,envelope:p.env,tail:p.tailEnv,randomDraws,sequence:Array.from(p.seq)};}
function input(kind,frame,ch,rate,block){
 if(kind==='silence')return 0;
 if(kind==='idle-wakeup')return frame<block*220?0:frame<block*260?(ch?.25:0):frame<block*300?(ch?.25:2e-5):0;
 if(kind==='impulse')return frame===0?(ch?-.3:.7):0;
 if(kind==='right')return ch?Math.sin(frame*.071)*.2:0;
 if(kind==='tail'&&frame>rate*.3)return 0;
 return .15*Math.sin(frame*(ch?.073:.117))+.05*Math.cos(frame*(ch?.021:.037));
}
let maximum=0,count=0,compared=0;
function compare(probe,{rate=48000,block=128,frames=32768,initial=values(hostDefaults),kind='tone',channels=2,events=[],label='default'}){
 const seed=123456789,h=harness(rate,seed);let p=h.create(),parameters=initial.map(Math.fround),frame=0;
 const commands=[`prepare ${rate} ${seed} ${parameters.join(' ')}`],expected=[null];
 const changes=[...events,[frames,null]];
 for(const [end,update]of changes){
  assert(end>=frame&&end<=frames&&end%block===0);
  if(end>frame){
   const inputs=Array.from({length:channels},()=>new Float32Array(end-frame)),out=[[],[]];
   for(let i=0;i<end-frame;++i)for(let ch=0;ch<channels;++ch)inputs[ch][i]=input(kind,frame+i,ch,rate,block);
   const params=Object.fromEntries(names.map((name,i)=>[name,Float32Array.of(parameters[i])]));
   for(let i=0;i<end-frame;i+=block){const output=[new Float32Array(block),new Float32Array(block)];assert.equal(p.process([inputs.map(a=>a.subarray(i,i+block))],[output],params),true);out[0].push(...output[0]);out[1].push(...output[1]);}
   commands.push(`render ${end-frame} ${channels} ${block} `+Array.from({length:end-frame},(_,i)=>inputs.map(a=>a[i]).join(' ')).join(' '));expected.push({pcm:out});frame=end;
   commands.push('state');expected.push({state:state(p,h.draws())});
  }
  if(update==='reset'){h.reset(seed);p=h.create();commands.push(`reset ${seed}`);expected.push(null);}
  else if(update){parameters=update.map(Math.fround);commands.push(`values ${parameters.join(' ')}`);expected.push(null);}
 }
 const run=spawnSync(probe,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:80*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);
 let peak=0,energy=0,errorEnergy=0,location=[];
 for(let n=0;n<lines.length;++n){const want=expected[n];if(!want){assert.equal(lines[n],'ok');continue;}
  if(want.state){const actual=JSON.parse(lines[n]);for(const [key,value]of Object.entries(want.state)){
   if(['phase','grainRate','grainTarget','envelope','tail'].includes(key)){const tolerance=key==='tail'?1e-6:key==='grainRate'||key==='grainTarget'?1e-12:1e-10;assert(Math.abs(actual[key]-value)<=tolerance,`${label}/${block} state ${key}: ${actual[key]} vs ${value}`);}
   else assert.deepEqual(actual[key],value,`${label}/${block} state ${key}`);
  }continue;}
  const actual=lines[n].split(' ').slice(1).map(Number);assert.equal(actual.length,want.pcm[0].length*2);
  for(let i=0;i<want.pcm[0].length;++i)for(let ch=0;ch<2;++ch){const a=actual[i*2+ch],b=want.pcm[ch][i],error=Math.abs(a-b);assert(Number.isFinite(a)&&Number.isFinite(b));energy+=b*b;errorEnergy+=(a-b)**2;if(error>peak){peak=error;location=[n,i,ch,a,b];}++compared;}
 }
 const rms=energy?Math.sqrt(errorEnergy/energy):Math.sqrt(errorEnergy);maximum=Math.max(maximum,peak);++count;
 assert(peak<=1e-6&&rms<=1e-5,JSON.stringify({label,rate,block,peak,rms,location}));
 if(kind==='idle-wakeup')assert(p.wPtr<frames,'Original idle gate actually suspended its clock');
 console.log(JSON.stringify({label,rate,block,peak,relativeRms:rms}));
}
if(require.main===module){
 const probe=process.argv[2];graphContract();
 for(const block of [64,128]){
  for(const rate of [8000,22050,32000,44100,48000,96000])compare(probe,{rate,block,frames:Math.ceil(Math.max(32768,rate*.9)/block)*block});
  for(let scale=0;scale<20;++scale){const initial=values({...hostDefaults,time:.01,scale,range:scale%3===0?.25:scale%3===1?1:3});compare(probe,{rate:8000,block,frames:8192,initial,label:'scale '+scale});}
  for(const [label,preset]of Object.entries(presets))compare(probe,{block,frames:65536,initial:values(preset),label});
  for(const kind of ['silence','impulse','right','tail','idle-wakeup'])compare(probe,{block,rate:8000,frames:65536,kind,label:kind});
  for(const channels of [0,1])compare(probe,{block,channels,rate:8000,frames:16384,initial:values({...hostDefaults,time:.01}),label:'channels '+channels});
  const base=values({...hostDefaults,time:.01,repetitions:10}),change=(p,v)=>{const a=[...base];a[p]=v;return a;};
  compare(probe,{block,rate:8000,frames:65536,initial:base,label:'STAY/range/scale/sync/BPM/feedback/reset',events:[[4096,change(3,1)],[8192,change(8,.25)],[12288,change(4,16)],[16384,change(5,1)],[20480,change(6,30)],[24576,change(6,300)],[28672,change(9,0)],[32768,'reset'],[36864,change(2,0)],[40960,change(7,0)],[45056,base]]});
 }
 console.log(JSON.stringify({result:'PASS',scope:'Unchanged original worklet only; no AudioParam/post-pan/connected-device claim',cases:count,comparedSamples:compared,maximumAbsoluteError:maximum,sourceSha256:crypto.createHash('sha256').update(original).digest('hex')}));
}
module.exports={original,factory,presets,hostDefaults,names,values,harness};
