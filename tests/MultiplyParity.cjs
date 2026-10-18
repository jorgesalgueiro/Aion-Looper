// Actual unchanged Multiply/capture/history methods; browser recorder and graph IO are stubbed.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const buffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c],copyToChannel:(d,c)=>channels[c].set(d)};};
 const audio={state:'running',currentTime:0,sampleRate:48000,destination:{},createBuffer:(count,length,rate)=>buffer(Array.from({length:count},()=>Array(length).fill(0)),rate)};
 const ctx={window:{},document:{createElement:()=>({}),head:{appendChild(){}},getElementById:()=>null},effects:{},FACTORY_EFFECTS:{},console,setTimeout,clearTimeout,audio,InputManager:{getRecordingNode:()=>ctx.haveInput?{connect(){},disconnect(){}}:null},AudioWorkletNode:class {constructor(){this.port={postMessage(){}};}connect(){}disconnect(){}},Log:{warn(){},error(...args){throw Error(args.join(' '));}},alert:()=>{ctx.alerted=true;}};vm.createContext(ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(source,ctx);
 // Retain the reference's recording DC pass rather than a hand-written substitute.
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(source);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=source.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<source.length);if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,loops:[],undoStack:[],redoStack:[],isRecording:false,recordingLoopId:-1,isFinishingRecording:false,loopRecorder:null,syncEnabled:false,playingSources:{},soloState:{active:false,loopId:-1,previousStates:{}}};
 UIManager.updateLoop=()=>{};UIManager.updateStatus=()=>{};
 const dc=finalBuffer=>{${source.slice(start,end)}};
 Loop.prototype.restart=function(){this.restarts=(this.restarts||0)+1;};
 globalThis.reset=(b,playing)=>{state.isRecording=false;state.recordingLoopId=-1;state.loopRecorder=null;state.isFinishingRecording=false;state.undoStack=[];state.redoStack=[];state.loops=Array.from({length:20},(_,i)=>new Loop(i));const l=state.loops[0];AudioEngine.seamlessLoopCrossfade(b,0.015);dc(b);l.audioBuffer=b;l.duration=b.duration;l.state=playing?'playing':'stopped';};
 globalThis.multiply=()=>state.loops[0].multiply();
 globalThis.finish=async chunks=>{state.loops[0].multiply();await LoopManager.processMultiplyData(chunks,0);};
 globalThis.editZero=async()=>{LoopManager.pushUndoState(0);const take=audio.createBuffer(state.loops[0].audioBuffer.numberOfChannels,1,audio.sampleRate);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,0,0.8);};
 globalThis.perform=name=>state.loops[0][name]();
 globalThis.current=()=>state.loops[0];
 globalThis.history=()=>[...state.loops.flatMap(l=>[l.undoStack.length,l.redoStack.length]),state.undoStack.at(-1)??-1,state.redoStack.at(-1)??-1];`,ctx);
 let cases=0,frames=0,snapshots=0,maximum=0;
 const exercise=async({rate,length,capture,mono,playing,input})=>{
  audio.sampleRate=rate;ctx.haveInput=input;ctx.alerted=false;
  const wave=(n,seed)=>[Array.from({length:n},(_,i)=>Math.fround(0.1+0.3*Math.sin(i*0.11+seed))),Array.from({length:n},(_,i)=>Math.fround(-0.08+0.2*Math.cos(i*0.073+seed)))];
  const raw=wave(length,0);ctx.reset(buffer(mono?[raw[0]]:raw,rate),playing);
  const commands=[`prepare 1048576 ${rate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','waitStill 0 0'];
  if(!playing)commands.push('stop 0 0');
  const pcm=[],metadata=[];
  const inspect=()=>{
   const l=ctx.current(),b=l.audioBuffer;
   metadata.push({row:commands.length,want:[b.length,l.state==='playing'?2:3]});commands.push('length 0 0');
   metadata.push({row:commands.length,want:Array.from(ctx.history())});commands.push('history 0 0');
   commands.push('retrigger 0 0');
   for(let i=0;i<b.length;++i){pcm.push({row:commands.length,want:(b.getChannelData(0)[i]+b.getChannelData(mono?0:1)[i])*0.5*Math.SQRT1_2});commands.push('tick 0 0');}
   // Rendering above is a separate probe read and must not alter the next reference control state.
   if(l.state==='stopped')commands.push('stop 0 0');
  };
  inspect();
  if(!mono&&rate===48000&&length===512&&capture===513&&playing&&input){
   // Multiply saves local history directly. It must preserve the reference's
   // global redo head even when it clears that loop's local redo branch.
   await ctx.editZero();commands.push('overdub 0 0','tick 0 0.25','finish 0 0','waitStill 0 0');inspect();
   ctx.perform('undo');commands.push('undo 0 0');inspect();
  }
  for(let pass=0;pass<2;++pass){
   ctx.multiply();commands.push(`multiply 0 ${input?1:0}`);
   if(playing&&input){
    assert.equal(ctx.current().state,'multiplying');
    const rawTake=wave(capture,pass+1),chunks=[];
    for(let i=0;i<capture;i+=64)chunks.push((mono?[rawTake[0]]:rawTake).map(c=>Float32Array.from(c.slice(i,i+64))));
    commands.push(...rawTake[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+rawTake[1][i]}`),`multiply 0 1`);
    await ctx.finish(chunks);
   }
   commands.push('waitStill 0 0');inspect();
  }
  if(ctx.current().undoStack.length){ctx.perform('undo');commands.push('undo 0 0');inspect();ctx.perform('redo');commands.push('redo 0 0');inspect();}
  const run=spawnSync(process.argv[2],['--audio'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:40*1024*1024});assert.equal(run.status,0,run.stderr);
  const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
  rows.forEach((r,i)=>assert.equal(Number(r.split(' ')[0]),1,`Rejected ${commands[i]}, ${JSON.stringify({rate,length,capture,mono,playing,input})}`));
  for(const m of metadata){assert.deepEqual(rows[m.row].split(' ').map(Number).slice(1),m.want,`Multiply metadata ${commands[m.row]} case ${JSON.stringify({rate,length,capture,mono,playing,input})}`);snapshots++;}
  for(const p of pcm){for(const actual of rows[p.row].split(' ').map(Number).slice(1)){const delta=Math.abs(actual-p.want);maximum=Math.max(maximum,delta);assert(delta<=1e-6,`Multiply PCM ${actual} vs JS ${p.want}`);}frames++;}
  cases++;
 };
 for(const mono of [true,false])for(const rate of [8000,48000,96000]){
  for(const [length,capture] of [[1,0],[17,1],[512,127],[17,128],[512,513],[4097,8193]])await exercise({mono,rate,length,capture,playing:true,input:true});
  for(const [playing,input] of [[false,true],[false,false],[true,false]])for(const length of [1,512,4097])await exercise({mono,rate,length,capture:0,playing,input});
 }
 console.log(JSON.stringify({scope:'Unchanged JS Multiply, short capture, stopped/no-input double, unequal-length local history and committed PCM; browser rendering/host/hardware excluded',cases,metadataSnapshots:snapshots,comparedFrames:frames,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true}));
})().catch(e=>{console.error(e);process.exitCode=1;});
