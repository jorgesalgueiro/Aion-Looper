// Original graph methods + complete main-loop gate/readMeter; audio/timer IO stubs.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..'),timers=new Map(),commands=[],expected=[];let timer=0,level=0;
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 class Node{constructor(){this.gain=param();this.playbackRate=param();}connect(){}disconnect(){}start(){}stop(){}getFloatTimeDomainData(array){const p=[1,-1,.5,-.5];for(let i=0;i<array.length;++i)array[i]=p[i%4]*Math.fround(level);}}
 class Source extends Node{}
 const contexts=new Map();const makeAudio=()=>({state:'running',currentTime:1,createBufferSource:()=>new Source(),createGain:()=>new Node(),createChannelMerger:()=>new Node(),createAnalyser:()=>new Node()});
 const audio=makeAudio();contexts.set(1,audio);
 const ctx={audio,window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,
  setTimeout:(fn,ms)=>{const id=++timer;timers.set(id,{fn,ms});return id;},clearTimeout:id=>timers.delete(id),
  AudioBufferSourceNode:Source,OscillatorNode:class{},AudioWorkletNode:class{},ScriptProcessorNode:class{}};
 vm.createContext(ctx);for(const file of ['audioEngine.js','loopTracks.js'])vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules',file),'utf8'),ctx);
 const main=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');const start=main.indexOf("const active = ['playing', 'stopping', 'overdubbing', 'substituting', 'multiplying'].includes(loop.state);");assert(start>=0);const end=main.indexOf('UIManager.renderMeter(',start);assert(end>start);
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],playingSources:{},customEffects:{},soloState:{active:false},undoStack:[],redoStack:[]};UIManager.updateLoop=()=>{};UIManager.updateStatus=()=>{};
 globalThis.make=id=>{const loop=new Loop(id);state.loops=[loop];return loop;};globalThis.setContext=audio=>state.audioContext=audio;
 globalThis.read=(loop,playbackTime)=>{${main.slice(start,end)}};`,ctx);
 const names=['empty','stopped','armed','queued','recording','restarting','finalizing','playing','stopping','overdubbing','substituting','multiplying'];
 const analysers=new WeakMap(),arrays=new WeakMap();let analyserSerial=0,dataSerial=0;
 const identity=(object,map,isAnalyser)=>{if(!object)return 0;if(!map.has(object))map.set(object,isAnalyser?++analyserSerial:++dataSerial);return map.get(object);};
 // One persistent native graph owner; clear mirrors changing the original loop slot.
 const loop=ctx.make(19);let current=audio;
 const step=async(command)=>{
  const [op,...args]=command.split(' '),v=args.map(Number);
  if(op==='buffer'){loop.audioBuffer=v[0]?{duration:1,length:48000}:null;loop.duration=v[0]?1:0;}
  else if(op==='state')loop.state=names[v[0]];
  else if(op==='play')await loop.play(1);
  else if(op==='retain'){assert(loop.graph&&!loop.graph.isDestroyed);const old=loop.graph;await loop.restart(current.currentTime+.05);assert.equal(loop.graph,old);}
  else if(op==='stop'){current.currentTime=v[0];if(!v[2]&&loop.graph)loop.graph.nodes.volume=null;loop.stop(v[1]);}
  else if(op==='teardown'){const pending=timers.get(loop.stopTimeout);if(pending)pending.fn();else loop._teardown();}
  else if(op==='destroy'){if(loop.graph)loop.graph.cleanup();}
  else if(op==='clear')loop.clear();
  else if(op==='read'){
   const [context,running,now,playback,amplitude]=v;if(context&&!contexts.has(context))contexts.set(context,makeAudio());current=context?contexts.get(context):null;
   if(current){current.state=running?'running':'suspended';current.currentTime=now;}else assert.equal(now,0);
   ctx.setContext(current);level=amplitude;ctx.read(loop,playback);
  }else throw Error(command);
  const p=loop.peak||{},pending=timers.get(loop.stopTimeout);
  expected.push([names.indexOf(loop.state),Number(!!loop.audioBuffer),Number(!!loop.graph),Number(!!loop.graph?.isDestroyed),identity(loop.analyser,analysers,true),identity(loop.analyserData,arrays,false),pending?pending.ms/1000:-1,p.rms||0,p.peak||0,p.linearPeak||0,Number.isFinite(p.peakDb)?p.peakDb:-60,p.rmsPercent||0,p.peakPercent||0,Number(!!p.clipped)]);
  commands.push(command);
 };
 const trace=async(lines)=>{for(const line of lines)await step(line);};
 await trace(['play','buffer 1','play','read 1 1 1 1 1.25','retain','read 1 1 1.01 1 0.1','stop 1.01 0 1','read 1 1 1.01 1.01 0.1','teardown','read 1 1 1.01 1.01 1.25','clear','read 1 1 1.1 1.1 1.25','buffer 1','play','read 1 1 1.2 1.2 0.1','clear','buffer 1','play','read 1 1 2 2 1.25','stop 2 2.5 1','read 1 1 2.6 2.6 0','play','read 1 1 2.7 2.7 0.2','stop 2.7 3 1','read 1 1 4 4 0','teardown','read 1 1 4 4 0','play','destroy','read 1 1 4.1 4.1 1.25','stop 4.1 0 0','read 1 1 4.2 4.2 1.25','play']);
 // All input-gate states, including states that refuse stop. Keep the graph.
 for(let state=0;state<names.length;++state)await trace([`state ${state}`,`read 1 1 ${5+state/10} ${5+state/10} 0.8`]);
 await trace(['state 7','read 1 0 7 7 1.25','read 1 1 7.1 7.1 0.3','read 2 1 7.2 7.2 0.2','read 0 0 0 0 0','read 2 1 8 7.8 1.25']);
 for(let i=0;i<280;++i){if(i%35===0)await step('retain');const now=8+i/100;await step(`read 2 1 ${now} ${i<12?7.8:i%9===0?now-.1:now} ${i%13===0?1.1:.15}`);}
 await trace(['clear','buffer 1','state 3','stop 11 12 1','read 2 1 11 11 1','teardown','play','stop 12 0 1','clear','read 2 1 12 12 1']);
 for(const state of [0,1,2,4,6])await trace([`state ${state}`,'stop 12 0 1']);
 // A queued state change during a scheduled stop does not cancel its timer.
 // Preserve the original, even though plain play-while-stopping cancels it.
 await trace(['buffer 1','state 7','stop 12 13 1','state 3','play','teardown']);
 const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);
 const actual=run.stdout.trim().split('\n').map(line=>line.trim().split(/\s+/).map(Number));assert.equal(actual.length,expected.length);let maximum=0;
 for(let i=0;i<expected.length;++i){assert.equal(actual[i].length,14);for(let j=0;j<14;++j){const error=Math.abs(actual[i][j]-expected[i][j]);maximum=Math.max(maximum,error);assert(Number.isFinite(actual[i][j])&&error<=(j<6?0:1e-12*Math.max(1,Math.abs(expected[i][j]))),JSON.stringify({step:i,command:commands[i],field:j,actual:actual[i],expected:expected[i]}));}}
 console.log(JSON.stringify({parity:true,observations:expected.length,maximumAbsoluteError:maximum,relativeTolerance:1e-12,scope:'Unchanged Loop/AudioGraph lifecycle, complete main-loop gate and readMeter composition on supplied Float32 windows; explicit timer delivery with IO stubs. Native graph notification binding, browser PCM, audible-time transport and UI excluded.'},null,2));
})().catch(e=>{console.error(e);process.exitCode=1;});
