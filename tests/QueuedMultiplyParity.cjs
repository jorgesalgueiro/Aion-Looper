// Queued MULT: unchanged Loop.multiply/multiplyInstant, history, Loop.play and AudioGraph.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c],copyToChannel:(data,c)=>channels[c].set(data)};};
 const param=()=>({value:1,events:[],setValueAtTime(value,time){this.events.push({value,time});},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){this.disconnected=true;},start(time,offset){this.started={time,offset};},stop(time){this.stopped=true;this.stoppedAt=time;},gain:param(),playbackRate:param()});
 class BufferSource {constructor(){Object.assign(this,node());}}
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:()=>new BufferSource(),createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},AudioBufferSourceNode:BufferSource,effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/scales.js'),'utf8'),context);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(loops,context);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],customEffects:{}};
 UIManager.updateLoop=()=>{}; UIManager.updateStatus=()=>{};state.playingSources={};
 globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};
 globalThis.setup=b=>{const loop=new Loop(0);loop.audioBuffer=b;loop.duration=b.duration;loop.state='playing';state.loops=[loop];loop.graph=new AudioGraph(loop);if(!loop.graph.build())throw Error('Reference graph failed');};
 globalThis.toggle=()=>state.loops[0].toggleEffect('reverse');
 globalThis.setReverse=value=>{if(state.loops[0].effects.reverse!==value)state.loops[0].toggleEffect('reverse');};
 globalThis.restart=()=>state.loops[0].graph.retrigger(0,state.loops[0]);
 globalThis.stored=()=>state.loops[0].audioBuffer;
 globalThis.source=()=>state.loops[0].graph.source.buffer;
 globalThis.sourceNode=()=>state.loops[0].graph.source;
 globalThis.volumeEvents=()=>state.loops[0].graph.nodes.volume.gain.events;
 globalThis.normalize=()=>state.loops[0].normalize();
 globalThis.edit=async(take,offset,feedback)=>{const finalBuffer=take;${loops.slice(start,end)};AudioEngine.seamlessLoopCrossfade(take,0.008);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,offset,feedback);};`,context);

 vm.runInContext(`state.bpm=120;state.bars=1;state.timeSig={num:4};state.masterStartTime=0;state.loopLength=2;
 globalThis.setDelay=value=>UIManager.setLoopStartDelay(0,value);
 globalThis.configure=(mode,bpm,origin,num)=>{state.syncEnabled=mode!==0;state.syncMode=mode===1?'static':mode===2?'dynamic':'off';state.bpm=bpm;state.masterStartTime=origin;state.timeSig.num=num;state.loopLength=60/bpm*num;state.loops[0].originalBpm=120;};
 globalThis.nudge=direction=>UIManager.nudgeLoopStart(0,direction);
 globalThis.quantize=()=>UIManager.quantizeLoopStart(0);
 globalThis.delayValue=()=>state.loops[0].startDelay;
 globalThis.rtrg=()=>state.loops[0].retrigger();
 globalThis.stopped=()=>{state.loops[0].state="stopped";};
 globalThis.grid=()=>SyncManager.getNextGridTime();
 globalThis.scheduledPlay=time=>state.loops[0].play(time);
 globalThis.scheduleStop=()=>state.loops[0].scheduleStop();
 globalThis.stopTime=()=>state.loops[0].graph.source.stoppedAt;
 globalThis.cleanup=()=>{const source=state.loops[0].graph.source;state.loops[0]._teardown();return {source,stopped:state.loops[0].state==='stopped',detached:state.loops[0].graph===null};};
 globalThis.applyPhase=()=>state.loops[0].restart(audio.currentTime+0.05);
 globalThis.rate=()=>Math.fround(state.loops[0].effectivePlaybackRate);
 globalThis.offset=()=>state.loops[0].graph.startOffset;
 globalThis.phaseSolo=value=>{state.loops[0].isMutedBySolo=value;};
 globalThis.phaseSpeed=value=>{state.loops[0].playbackRate=value;};
 globalThis.userSpeed=value=>UIManager.setLoopSpeed(0,value);
 `,context);

 vm.runInContext(`state.undoStack=[];state.redoStack=[];UIManager.updateLoopDisplays=()=>{};
 globalThis.seedHistory=()=>{state.undoStack=[];state.redoStack=[];const l=state.loops[0];l.state='stopped';l.graph=null;};
 globalThis.multiply=()=>state.loops[0].multiply();
 globalThis.noGraph=()=>state.loops[0].graph===null;

 globalThis.restore=redo=>state.loops[0][redo?'redo':'undo']();
 globalThis.queue=()=>{state.loops[0].state='queued';};
 globalThis.info=()=>{const l=state.loops[0];return [l.audioBuffer.length,l.state,l.undoStack.length,l.redoStack.length];};`,context);
 const ni=process.argv.indexOf('--native'),probe=ni<0?null:process.argv[ni+1];if(ni>=0)assert(probe);
 let cases=0,frames=0,silent=0,maximum=0;
 for(const mono of [true,false])for(const rate of [8000,48000,96000])for(const size of [1,17,512,4097])for(const reverse of [false,true])for(const late of [false,true])for(const repeated of [false,true]){
  audio.currentTime=0;vm.runInContext('state.syncEnabled=false;state.bpm=120;',context);
  const raw=[Array.from({length:size},(_,i)=>Math.fround(.12+.4*Math.sin(i*.11))),Array.from({length:size},(_,i)=>Math.fround(-.08+.2*Math.cos(i*.073)))];
  const b=makeBuffer(mono?[raw[0]]:raw,rate);context.condition(b);context.setup(b);context.seedHistory();
  const commands=[`prepare 81920 ${rate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0','stop 0 0'];
  let frame=size,bpm=137.3,origin=frame-frame*120/bpm;audio.currentTime=frame/rate;
  context.configure(1,bpm,origin/rate,1);commands.push(`sync 1 ${bpm} 1 1`);context.setReverse(reverse);commands.push(`reverse 0 ${reverse?1:0}`);context.setDelay(.217);commands.push('delay 0 .217');
  const checks=[],infos=[];const silence=until=>{while(frame<until){checks.push({row:commands.length,value:0});commands.push('tick 0 0');frame++;silent++;}audio.currentTime=frame/rate;};
  let grid=context.grid();if(grid-audio.currentTime<=.100){silence(Math.ceil(grid*rate)+1);grid=context.grid();}
  const deadline=Math.ceil(grid*rate-1e-9);commands.push('play 0 0');context.queue();
  const inspect=()=>{const info=context.info();assert.equal(info[1],'queued');assert(context.noGraph(),'MULT does not construct a queued source');infos.push({row:commands.length,want:info});commands.push('inspect 0 0');};
  const multiply=()=>{const old=context.stored(),previous=context.info();context.multiply();const doubled=context.stored();assert.notEqual(doubled,old);assert.equal(doubled.length,old.length*2);for(let c=0;c<old.numberOfChannels;c++){assert.deepEqual(doubled.getChannelData(c).slice(0,old.length),old.getChannelData(c));assert.deepEqual(doubled.getChannelData(c).slice(old.length),old.getChannelData(c));}assert.equal(context.info()[2],previous[2]+1);commands.push('multiply 0 0','wait 0 0');inspect();};
  multiply();
  if(repeated){multiply();for(const redo of [false,true,false]){context.restore(redo);commands.push(`${redo?'redo':'undo'} 0 0`);inspect();}}
  if(late)silence(deadline+Math.ceil(rate*.023));
  const info=context.info(),length=info[0];audio.currentTime=frame/rate;await context.scheduledPlay(grid);commands.push('birth 0 0');
  const src=context.source(),node=context.sourceNode(),start=node.started.time*rate,offset=node.started.offset*rate;
  assert.equal(src.length,length,'Source uses the latest restored buffer length');
  if(frame<deadline)silence(deadline);
  for(let n=0;n<Math.min(length+7,155);++n){const phase=((offset+(frame-start)*context.rate())%length+length)%length,i=Math.floor(phase),next=(i+1)%length,f=Math.fround(phase-i);
   const value=c=>Math.fround(src.getChannelData(c)[i]+Math.fround(src.getChannelData(c)[next]-src.getChannelData(c)[i])*f);
   checks.push({row:commands.length,value:(value(0)+value(mono?0:1))*.5*Math.SQRT1_2});commands.push('tick 0 0');frame++;
  }
  if(probe){const run=spawnSync(probe,['--callback-start'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
  const rows=run.stdout.trim().split(/\r?\n/).map(l=>l.split(' ').map(Number));assert.equal(rows.length,commands.length);
  rows.forEach((row,i)=>assert.equal(row[0],1,'Rejected '+commands[i]+' '+JSON.stringify({rate,size,mono,reverse,late})));
  for(const {row,want} of infos){const v=rows[row];assert.equal(v[1],want[0]);assert.equal(v[2],9,'Native remains queued');assert.equal(v[5],want[2]);assert.equal(v[6],want[3]);}
  for(const c of checks)for(const actual of rows[c.row].slice(6,8)){const e=Math.abs(actual-c.value);maximum=Math.max(maximum,e);assert(e<=1e-6,'Restored source PCM '+JSON.stringify({e,actual,want:c.value,rate,size,reverse,late}));frames++;}
  }cases++;
 }
 console.log(JSON.stringify({result:'PASS',mode:probe?'native_source_comparison':'reference_only',cases,comparedChannelSamples:frames,silentFrames:silent,maximumAbsoluteError:probe?maximum:null,tolerance:probe?1e-6:null,scope:'Unchanged queued Multiply/doubling/history before early/late source birth, repeated doubling and Undo/Redo, original grid and forward/reverse source PCM; browser envelopes/resampling and asynchronous worker latency excluded'}));
})().catch(error=>{console.error(error);process.exitCode=1;});
