// Execute unchanged Loop.toggleEffect, AudioGraph build/retrigger and buffer DSP.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]};};
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){},start(){},stop(){},gain:param(),playbackRate:param()});
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:node,createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(loops,context);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],customEffects:{}};
 UIManager.updateLoop=()=>{};
 globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};
 globalThis.setup=b=>{const loop=new Loop(0);loop.audioBuffer=b;loop.duration=b.duration;loop.state='playing';state.loops=[loop];loop.graph=new AudioGraph(loop);if(!loop.graph.build())throw Error('Reference graph failed');};
 globalThis.toggle=()=>state.loops[0].toggleEffect('reverse');
 globalThis.restart=()=>state.loops[0].graph.retrigger(0,state.loops[0]);
 globalThis.stored=()=>state.loops[0].audioBuffer;
 globalThis.source=()=>state.loops[0].graph.source.buffer;
 globalThis.normalize=()=>state.loops[0].normalize();
 globalThis.edit=async(take,offset,feedback)=>{const finalBuffer=take;${loops.slice(start,end)};AudioEngine.seamlessLoopCrossfade(take,0.008);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,offset,feedback);};`,context);
 let cases=0,samples=0,maximum=0;
 for(const mono of [true,false])for(const rate of [8000,48000,96000])for(const length of [1,4,17,512,4097]){
  const raw=[Array.from({length},(_,i)=>Math.fround(0.12+0.4*Math.sin(i*0.11))),Array.from({length},(_,i)=>Math.fround(-0.08+0.2*Math.cos(i*0.073)))];
  const base=makeBuffer(mono?[raw[0]]:raw,rate);context.condition(base);context.setup(base);
  const commands=[`prepare 81920 ${rate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','waitStill 0 0','retrigger 0 0'];
  const checks=[];let phase=0,speed=1;
  const render=(count,label)=>{const source=context.source();for(let n=0;n<count;++n){const i=Math.floor(phase),j=(i+1)%length,f=phase-i;const value=c=>Math.fround(source.getChannelData(c)[i]+Math.fround(source.getChannelData(c)[j]-source.getChannelData(c)[i])*Math.fround(f));const expected=(value(0)+value(mono?0:1))*0.5*Math.SQRT1_2;checks.push({row:commands.length,expected,label});commands.push('tick 0 0');phase=(phase+speed)%length;}};
  const restart=()=>{commands.push('retrigger 0 0');context.restart();phase=0;};
  render(Math.min(length+3,1024),'forward');
  commands.push('reverse 0 1');context.toggle();render(19,'toggle-on leaves forward source');
  restart();render(Math.min(length*2+3,2048),'reversed source and wrap');
  commands.push('reverse 0 0');context.toggle();render(19,'toggle-off leaves reversed source');
  const oldSource=context.source();commands.push('normalize 0 0','waitStill 0 0');context.normalize();assert.equal(context.source(),oldSource,'Reference reverse source remains independent of NORM');render(47,'NORM leaves playing reverse copy');
  restart();render(Math.min(length+3,1024),'next forward source sees normalized storage');
  commands.push('reverse 0 1');context.toggle();restart();
  for(const rateMultiplier of [0.25,0.5,Math.fround(0.37),2,1]){speed=rateMultiplier;commands.push(`speed 0 ${speed}`);restart();render(137,'fractional reverse source interpolation model');}
  const offset=Math.floor(length/3);render(offset,'advance edit offset');
  for(const replacement of [false,true]){
   const count=Math.max(1,Math.floor(length*1.7));const take=[Array.from({length:count},(_,i)=>Math.fround(0.07+0.3*Math.cos(i*0.17))),Array.from({length:count},(_,i)=>Math.fround(-0.04+0.2*Math.sin(i*0.09)))];
   const editOffset=phase;
   commands.push('feedback 0 0.8',`${replacement?'substitute':'overdub'} 0 0`);
   const source=context.source();
   for(let i=0;i<count;i++){const index=Math.floor(phase);const expected=(source.getChannelData(0)[index]+source.getChannelData(mono?0:1)[index])*0.5*Math.SQRT1_2;checks.push({row:commands.length,expected,label:'old reverse source during edit'});commands.push(`tick 0 ${take[0][i]}${mono?'':' '+take[1][i]}`);phase=(phase+1)%length;}
   await context.edit(makeBuffer(mono?[take[0]]:take,rate),editOffset/rate,replacement?0:0.8);
   commands.push('finish 0 0','waitStill 0 0');restart();render(Math.min(length*2+3,2048),'reversed committed edit');
  }
  const run=spawnSync(process.argv[2],['--audio'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:24*1024*1024});assert.equal(run.status,0,run.stderr);
  const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);rows.forEach((row,i)=>assert.equal(Number(row.split(' ')[0]),1,'Rejected '+commands[i]));
  for(const check of checks){const values=rows[check.row].split(' ').map(Number);for(const actual of values.slice(1)){const error=Math.abs(actual-check.expected);maximum=Math.max(maximum,error);assert(error<=1e-6,`${check.label}, rate ${rate}, length ${length}: ${actual} vs JS source ${check.expected}`);}samples++;}
  cases++;
 }
 console.log(JSON.stringify({scope:'Unchanged JS reverse toggle/graph source and conditioned PCM; linear interpolation model only, browser envelopes/resampling/latency excluded',cases,comparedFrames:samples,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true}));
})().catch(error=>{console.error(error);process.exitCode=1;});
