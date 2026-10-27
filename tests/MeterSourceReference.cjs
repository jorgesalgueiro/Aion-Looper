// Original methods with audio-node/timer IO stubs: identity/state, not browser PCM.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const root=path.resolve(__dirname,'..');let serial=0,timer=0;const timers=new Map();
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 class Node{constructor(){this.id=++serial;this.gain=param();this.playbackRate=param();}connect(){}disconnect(){}start(time,offset){this.started={time,offset};}stop(){}}
 class Source extends Node{}
 const audio={state:'running',currentTime:1,createBufferSource:()=>new Source(),createGain:()=>new Node(),createChannelMerger:()=>new Node(),createAnalyser:()=>new Node()};
 let grid=1.2;
 const ctx={audio,window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,
  EffectManager:{},SyncManager:{getNextGridTime:()=>grid},Log:{debug(){}},
  setTimeout:(fn,ms)=>{const id=++timer;timers.set(id,{fn,ms});return id;},clearTimeout:id=>timers.delete(id),
  AudioBufferSourceNode:Source,OscillatorNode:class{},AudioWorkletNode:class{},ScriptProcessorNode:class{}};
 vm.createContext(ctx);vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8'),ctx);
 const main=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');const start=main.indexOf("const active = ['playing', 'stopping', 'overdubbing', 'substituting', 'multiplying'].includes(loop.state);");assert(start>=0);
 const end=main.indexOf('UIManager.renderMeter(',start);assert(end>start);
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],playingSources:{},customEffects:{},soloState:{active:false},undoStack:[],redoStack:[]};UIManager.updateLoop=()=>{};UIManager.updateStatus=()=>{};
 globalThis.make=id=>{const loop=new Loop(id);loop.audioBuffer={duration:1,length:48000};loop.duration=1;state.loops=[loop];return loop;};
 globalThis.gate=loop=>{let observed;const original=AudioEngine.readMeter;AudioEngine.readMeter=(a,d)=>{observed={analyser:a,data:d};};try{const playbackTime=audio.currentTime;${main.slice(start,end)}}finally{AudioEngine.readMeter=original;}return observed;};`,ctx);
 let cases=0;
 for(const id of [0,19])for(let round=0;round<4;++round){
  const loop=ctx.make(id);await loop.play(1);const graph=loop.graph,analyser=loop.analyser,data=loop.analyserData,source=graph.source;
  assert(analyser&&data.length===2048);assert.equal(ctx.gate(loop).analyser,analyser);
  await loop.restart(audio.currentTime+0.05);assert.equal(loop.graph,graph);assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);assert.notEqual(graph.source,source);assert.equal(graph.source.started.time,audio.currentTime+0.05);++cases;
  const second=graph.source;graph.retrigger(0,loop);assert.notEqual(graph.source,second);assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);assert.equal(graph.source.started.time,audio.currentTime+0.005);++cases;
  loop.stop();assert.equal(loop.state,'stopping');assert.equal(loop.analyser,analyser);const stop=timers.get(loop.stopTimeout);assert.equal(stop.ms,25);stop.fn();assert.equal(loop.analyser,null);assert.equal(loop.analyserData,data);assert.equal(ctx.gate(loop).analyser,null);++cases;
  await loop.play(1);assert.notEqual(loop.graph,graph);assert.notEqual(loop.analyser,analyser);assert.notEqual(loop.analyserData,data);++cases;
  const current=loop.analyser;for(const state of ['playing','stopping','overdubbing','substituting','multiplying','empty','stopped','armed','queued','recording','restarting','finalizing']){
   loop.state=state;const active=['playing','stopping','overdubbing','substituting','multiplying'].includes(state);assert.equal(ctx.gate(loop).analyser,active?current:null);++cases;
  }
  loop.state='playing';loop.graph.isDestroyed=true;assert.equal(ctx.gate(loop).analyser,null);loop.graph.isDestroyed=false;loop.clear();assert.equal(loop.graph,null);assert.equal(loop.analyser,null);assert.equal(loop.analyserData,null);assert.equal(ctx.gate(loop).analyser,null);++cases;
 }
 // Exercise the actual dispatcher and resume method, not a transcription of its
 // scheduling formula. A fresh analyser exists before the scheduled source is
 // audible; a native LoopState::playing snapshot cannot identify this birth.
 vm.runInContext(`globalThis.action=(id)=>LoopManager.handleAction(id,'short');
 globalThis.fresh=(id,sync)=>{state.loops=[];const loop=new Loop(id);state.loops[id]=loop;
 loop.audioBuffer={duration:1,length:48000};loop.duration=1;loop.state='stopped';
 state.syncEnabled=sync;state.masterStartTime=1;state.isRecording=false;state.samplers=[];
 state.playingSources={};return loop;};
 globalThis.panic=()=>LoopManager.stopAll();
 globalThis.replace=(id)=>{state.loops[id]=new Loop(id);};`,ctx);
 const near=(a,b)=>assert(Math.abs(a-b)<1e-12,`${a} != ${b}`);
 const noGraph=loop=>{assert.equal(loop.graph,null);assert(loop.analyser==null);assert(loop.analyserData==null);};
 let schedulingCases=0,resumeCases=0;
 for(const id of [0,19]){
  for(const [next,delivery] of [[1.2,1.1],[1.05,1],[1.2,1.23]]){
   timers.clear();audio.state='running';audio.currentTime=1;grid=next;
   const loop=ctx.fresh(id,true);await ctx.action(id);
   assert.equal(loop.state,'queued');noGraph(loop);
   const queued=timers.get(loop.queueTimeout);assert(queued);
   near(queued.ms,Math.max(0,(next-1)*1000-100));
   audio.currentTime=delivery;queued.fn();
   assert.equal(loop.queueTimeout,null);assert.equal(loop.state,'playing');
   assert(loop.graph&&loop.analyser&&loop.analyserData.length===2048);
   assert.equal(ctx.gate(loop).analyser,loop.analyser);
   near(loop.graph.source.started.time,Math.max(next,delivery));
   near(loop.graph.source.started.offset,Math.max(next,delivery)-1);
   ++schedulingCases;
  }
  // A second queued short press cancels before the callback: no graph was born.
  for(const cancel of ['action','stop','clear','panic']){
   timers.clear();audio.state='running';audio.currentTime=1;grid=1.2;
   const loop=ctx.fresh(id,true);await ctx.action(id);const pending=loop.queueTimeout;
   assert(timers.has(pending));
   if(cancel==='action')await ctx.action(id);
   else if(cancel==='panic')ctx.panic();else loop[cancel]();
   assert(!timers.has(pending));assert.equal(loop.queueTimeout,null);noGraph(loop);
   assert.equal(loop.state,cancel==='clear'?'empty':'stopped');++schedulingCases;
  }
  // The unchanged AudioEngine.resume awaits the context IO stub. Reject/cancel
  // while that promise is pending; no eager graph or analyser may survive it.
  for(const initial of ['stopped','queued','restarting']){
   timers.clear();audio.currentTime=1;audio.state='suspended';
   let resume;audio.resume=()=>new Promise(resolve=>{resume=resolve;});
   const loop=ctx.fresh(id,false);loop.state=initial;
   const pending=loop.play();assert.equal(typeof resume,'function');noGraph(loop);
   resume();await pending;assert.equal(loop.state,'stopped');noGraph(loop);++resumeCases;
  }
  for(const cancel of ['stop','clear','panic']){
   timers.clear();audio.currentTime=1;audio.state='suspended';
   let resume;audio.resume=()=>new Promise(resolve=>{resume=resolve;});
   const loop=ctx.fresh(id,false),pending=loop.play();noGraph(loop);
   if(cancel==='panic')ctx.panic();else loop[cancel]();
   audio.state='running';resume();await pending;noGraph(loop);
   assert.equal(loop.state,cancel==='clear'?'empty':'stopped');++resumeCases;
  }
  for(const cancel of ['replace','stop','clear','panic']){
   timers.clear();audio.currentTime=1;audio.state='suspended';
   let resume;audio.resume=()=>new Promise(resolve=>{resume=resolve;});
   const loop=ctx.fresh(id,true),pending=ctx.action(id);noGraph(loop);
   if(cancel==='replace')ctx.replace(id);
   else if(cancel==='panic')ctx.panic();else loop[cancel]();
   audio.state='running';resume();await pending;noGraph(loop);
   assert(loop.queueTimeout==null);assert.equal(timers.size,0);++resumeCases;
  }
  // Successful immediate resume creates exactly one graph; another play while
  // playing retains it, unlike a stopped/queued fresh graph notification.
  timers.clear();audio.currentTime=1;audio.state='suspended';
  let resume;audio.resume=()=>new Promise(resolve=>{resume=resolve;});
  const loop=ctx.fresh(id,false),pending=loop.play();noGraph(loop);
  audio.state='running';resume();await pending;
  const graph=loop.graph,analyser=loop.analyser,data=loop.analyserData;
  assert(graph&&analyser&&data.length===2048);await loop.play();
  assert.equal(loop.graph,graph);assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);++resumeCases;
 }
 let panicCases=0;
 for(const id of [0,19]){
  for(const state of ['playing','overdubbing','substituting','multiplying','stopping','restarting']){
   timers.clear();audio.state='running';audio.currentTime=1;
   const loop=ctx.fresh(id,false);await loop.play(1);loop.state=state;
   const analyser=loop.analyser,data=loop.analyserData;ctx.panic();
   const first=loop.stopTimeout;assert.equal(loop.state,'stopping');near(timers.get(first).ms,65);
   assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);
   ctx.panic();assert(!timers.has(first));assert.notEqual(loop.stopTimeout,first);near(timers.get(loop.stopTimeout).ms,65);
   timers.get(loop.stopTimeout).fn();assert.equal(loop.analyser,null);assert.equal(loop.analyserData,data);
   const count=timers.size;ctx.panic();assert.equal(loop.stopTimeout,null);assert.equal(timers.size,count);++panicCases;
  }
  // Panic cancels queued PLAY without replacing a retained old stop deadline.
  timers.clear();audio.currentTime=1;const loop=ctx.fresh(id,false);await loop.play(1);
  loop.stop();const retained=loop.stopTimeout,analyser=loop.analyser,data=loop.analyserData;
  loop.state='queued';loop.queueTimeout=ctx.setTimeout(()=>{},100);const queued=loop.queueTimeout;
  ctx.panic();assert.equal(loop.state,'stopped');assert.equal(loop.queueTimeout,null);assert(!timers.has(queued));
  assert.equal(loop.stopTimeout,retained);assert.equal(timers.get(retained).ms,25);
  assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);++panicCases;
 }
 let queuedStopCases=0;
 for(const id of [0,19])for(const attached of [false,true]){
  timers.clear();audio.state='running';audio.currentTime=1;const loop=ctx.fresh(id,true);
  if(attached){await loop.play(1);loop.stop();}
  else await ctx.action(id);
  loop.state='queued';if(!loop.queueTimeout)loop.queueTimeout=ctx.setTimeout(()=>{},100);
  const queued=loop.queueTimeout,old=loop.stopTimeout,analyser=loop.analyser,data=loop.analyserData;
  loop.stop();assert.equal(loop.queueTimeout,null);assert(!timers.has(queued));
  if(attached){assert.equal(loop.state,'stopping');assert(!timers.has(old));assert.notEqual(loop.stopTimeout,old);assert.equal(timers.get(loop.stopTimeout).ms,25);
   assert.equal(loop.analyser,analyser);assert.equal(loop.analyserData,data);}
  else{assert.equal(loop.state,'stopped');assert.equal(loop.stopTimeout,null);noGraph(loop);}
  ++queuedStopCases;
 }
 console.log(JSON.stringify({passed:true,cases,schedulingCases,resumeCases,panicCases,queuedStopCases,
  scope:'Unchanged Loop/AudioGraph methods, actual handleAction scheduling/cancellation and AudioEngine.resume with controlled context/timer IO. Includes pre-grid analyser birth, late source phase, pending-resume invalidation and identity retention. No native producer, browser PCM or renderer acceptance.'},null,2));
})().catch(e=>{console.error(e);process.exitCode=1;});
