// Original methods with audio-node/timer IO stubs: identity/state, not browser PCM.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const root=path.resolve(__dirname,'..');let serial=0,timer=0;const timers=new Map();
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 class Node{constructor(){this.id=++serial;this.gain=param();this.playbackRate=param();}connect(){}disconnect(){}start(time,offset){this.started={time,offset};}stop(){}}
 class Source extends Node{}
 const audio={state:'running',currentTime:1,createBufferSource:()=>new Source(),createGain:()=>new Node(),createChannelMerger:()=>new Node(),createAnalyser:()=>new Node()};
 const ctx={audio,window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,
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
 console.log(JSON.stringify({passed:true,cases,scope:'Unchanged Loop play/restart/stop/teardown/clear, AudioGraph retrigger and complete original main-loop meter input gating; IO stubs, not browser audio or native analyser teardown parity.'},null,2));
})().catch(e=>{console.error(e);process.exitCode=1;});
