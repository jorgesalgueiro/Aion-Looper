// Runs the unchanged original AudioWorklet DSP with real Float32 parameter/output arrays.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');let source;
const destination={},connections=[];
const uiContext={document:{createElement:()=>({}),head:{appendChild:e=>source=e.textContent}},console,
 state:{audioContext:{destination,currentTime:0},bpm:120,masterStartTime:0,timeSig:{num:4},metronome:{volume:0.5}},
 AudioWorkletNode:class{constructor(){this.parameters={get:()=>({setValueAtTime(){}})};}connect(target){connections.push(target);}}};
vm.createContext(uiContext);vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/scales.js'),'utf8'),uiContext);
vm.runInContext('MetronomeScheduler.init()',uiContext);
assert.deepEqual(connections,[destination],'Unchanged original metronome routes directly to destination');
assert(source?.includes("registerProcessor('metronome-processor'"));
let rate=48000,Processor;const context={AudioWorkletProcessor:class{},getWorkletSampleRate:()=>rate,registerProcessor:(name,processor)=>{assert.equal(name,'metronome-processor');Processor=processor;},currentFrame:0};
vm.createContext(context);vm.runInContext(source,context);
let cases=0,samples=0,maximum=0;
for(const sr of [8000,44100,48000,96000])for(const block of [64,128])for(const meter of [1,4,7]){
 rate=sr;const processor=new Processor(),commands=[`prepare ${sr}`],expected=[];
 let frame=meter===7?2**32+17:meter===1?0:12345;
 const render=(seconds,bpm,origin,volume,playing)=>{
  const frames=Math.max(1,Math.round(seconds*sr));
  for(let offset=0;offset<frames;offset+=block){
   const n=Math.min(block,frames-offset),output=[new Float32Array(n),new Float32Array(n)];context.currentFrame=frame;
   const params={bpm:Float32Array.of(bpm),origin:Float32Array.of(origin),volume:Float32Array.of(volume),playing:Float32Array.of(playing),beatsPerBar:Float32Array.of(meter)};
   assert.equal(processor.process([], [output],params),true);assert.deepEqual(output[0],output[1]);
   expected.push(...output[0]);commands.push(`render ${frame} ${n} ${params.bpm[0]} ${params.origin[0]} ${meter} ${params.volume[0]} ${playing}`);frame+=n;
  }
 };
 render(0.71,120,0,0.5,1);render(0.003,120,0,0.5,0);
 // Restart off-grid, fractional tempo/origin, hot/zero volume and meter accents.
 render(0.83,137.3,0.137,0.37,1);render(0.13,999,-0.234,2,1);
 render(0.04,999,-0.234,0,1);render(0.12,999,-0.234,0.5,1);
 render(0.02,999,-0.234,0.5,0);
 render(0.57,73.7,(frame+sr*0.2)/sr,0.8,1);
 const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:24*1024*1024});assert.equal(run.status,0,run.stderr);
 const actual=run.stdout.trim().split(/\s+/).map(Number);assert.equal(actual.length,expected.length);
 assert(expected.some(x=>Math.abs(x)>0.01),'Oracle must contain audible clicks');
 for(let i=0;i<actual.length;++i){const error=Math.abs(actual[i]-expected[i]);maximum=Math.max(maximum,error);assert(error<=1e-6,`Metronome ${sr}/${block}/${meter} sample ${i}: ${actual[i]} vs JS ${expected[i]}`);}
 cases++;samples+=actual.length;
}
// Original a-rate BPM/origin inputs: native per-sample updates must match too.
rate=48000;const p=new Processor(),n=128,out=[new Float32Array(n),new Float32Array(n)];context.currentFrame=23990;
const params={bpm:Float32Array.from({length:n},(_,i)=>120+i*0.01),origin:Float32Array.from({length:n},(_,i)=>i*0.000001),volume:Float32Array.of(0.5),playing:Float32Array.of(1),beatsPerBar:Float32Array.of(4)};
p.process([], [out],params);
const commands=['prepare 48000',...Array.from({length:n},(_,i)=>`render ${23990+i} 1 ${params.bpm[i]} ${params.origin[i]} 4 0.5 1`)];
const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8'});assert.equal(run.status,0,run.stderr);
const values=run.stdout.trim().split(/\s+/).map(Number);assert.equal(values.length,n);
for(let i=0;i<n;++i){const error=Math.abs(values[i]-out[0][i]);maximum=Math.max(maximum,error);assert(error<=1e-6,'A-rate metronome parity');}
// Connected native Engine: use its accepted master-clock telemetry as the worklet
// parameters, then compare the actual output pipeline (including output protection).
let connected=0;
for(const sr of [8000,48000])for(const block of [64,128]){
 rate=sr;const processor=new Processor();
 const commands=[`prepare ${sr}`,'engine 8192 120 4 0.5 1 '+block+' 0','engine 10001 73.7 7 0.37 1 '+block+' 1','engine 5000 999 1 2 1 '+block+' 2','engine 1001 137.3 5 0.8 0 '+block+' 0','engine 16001 137.3 5 0.8 1 '+block+' 0'];
 const result=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(result.status,0,result.stderr);
 for(const line of result.stdout.trim().split(/\r?\n/)){
  const [frame,count,bpm,origin,meter,volume,playing,...actual]=line.split(' ').map(Number);assert.equal(actual.length,count);context.currentFrame=frame;
  const output=[new Float32Array(count),new Float32Array(count)];
  processor.process([], [output],{bpm:Float32Array.of(bpm),origin:Float32Array.of(origin),beatsPerBar:Float32Array.of(meter),volume:Float32Array.of(volume),playing:Float32Array.of(playing)});
  for(let i=0;i<count;++i){const expected=Math.max(-1,Math.min(1,output[0][i]));const error=Math.abs(actual[i]-expected);maximum=Math.max(maximum,error);assert(error<=1e-6,`Connected metronome ${sr}/${block} frame ${frame+i}: ${actual[i]} vs ${expected}`);samples++;}
 }
 connected++;
}
console.log(JSON.stringify({scope:'Unchanged original metronome worklet PCM and connected Engine/master-clock output, Float32 parameters, accents/decay/grid/start-stop, a-rate BPM/origin',cases:cases+1+connected,connectedEngineCases:connected,comparedSamples:samples+n,maximumAbsoluteError:maximum,tolerance:1e-6}));
