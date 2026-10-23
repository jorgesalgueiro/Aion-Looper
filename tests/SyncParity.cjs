// Unchanged sync/recording methods; browser IO/timers only are stubbed.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const buffer=(count,length,rate)=>{const channels=Array.from({length:count},()=>new Float32Array(length));return {length,duration:length/rate,sampleRate:rate,numberOfChannels:count,getChannelData:c=>channels[c],copyToChannel:(d,c)=>channels[c].set(d)};};
 const timers=new Map();let timerId=0;
 const audio={state:'running',currentTime:0,sampleRate:8000,baseLatency:0,outputLatency:0,destination:{},createBuffer:buffer,createOscillator:()=>({frequency:{value:0},connect(){},start(){},stop(){}}),createGain:()=>({gain:{value:0},connect(){}}),createAnalyser:()=>({fftSize:512,disconnect(){}})};
 const ctx={window:{},document:{createElement:()=>({}),head:{appendChild(){}},getElementById:()=>null},effects:{},FACTORY_EFFECTS:{},console,audio,
 setTimeout:(fn,delay)=>{timers.set(++timerId,{fn,delay});return timerId;},clearTimeout:id=>timers.delete(id),
 InputManager:{getRecordingNode:()=>({channelCount:2,connect(){},disconnect(){}})},EffectManager:{setActiveTab(){}},
 AudioWorkletNode:class{constructor(){this.port={postMessage(){}};this.context=audio;}connect(){}disconnect(){}},Log:{warn(){},debug(){},error(...args){throw Error(args.join(' '));}}};
 vm.createContext(ctx);
 for(const file of ['audioEngine.js','scales.js','loopTracks.js'])vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules',file),'utf8'),ctx);
 vm.runInContext(`const state={audioContext:audio,loops:[],isRecording:false,recordingLoopId:-1,isFinishingRecording:false,recordingTimeout:null,recordingStartTimeout:null,loopRecorder:null,masterStartTime:1e-100,bpm:600,bars:1,timeSig:{num:1,den:4},loopLength:0.1,countIn:{visual:false,audio:false},autoPlayAfterRecord:false,autoRecordNext:false,inputLatencyMs:0,playingSources:{},undoStack:[],redoStack:[],soloState:{active:false,loopId:-1,previousStates:{}}};
 AudioEngine.resume=async()=>true;
 for(const name of ['updateLoop','updateStatus','updateExportButtons','updateLoopDisplays'])UIManager[name]=()=>{};
 UIManager.generateWaveformPeaks=()=>[];
 globalThis.reset=(mode,bpm,num,bars,countIn=false,countInAudio=false)=>{state.syncMode=mode;state.syncEnabled=mode!=='off';state.bpm=bpm;state.timeSig.num=num;state.bars=bars;state.loopLength=60/bpm*num*bars;state.countIn.visual=countIn;state.countIn.audio=countInAudio;state.isRecording=false;state.isFinishingRecording=false;state.recordingTimeout=null;state.recordingStartTimeout=null;state.loopRecorder=null;state.recordingLoopId=-1;state.loops=Array.from({length:20},(_,i)=>new Loop(i));};
 globalThis.start=()=>LoopManager.startRecording(0);
 globalThis.stop=()=>LoopManager.stopRecording();
 globalThis.current=()=>state.loops[0];
 globalThis.pending=()=>state.recordingTimeout;
 globalThis.armTimer=()=>state.recordingStartTimeout;
 globalThis.forceStop=()=>LoopManager.stopRecording(true);
 globalThis.process=chunks=>LoopManager.processRecordedData(chunks,0,1,LoopManager.transportGeneration);
 globalThis.effective=(mode,bpm,num,bars,dynamic,original,length,speed)=>{state.syncEnabled=mode!==0;state.bpm=bpm;state.loopLength=60/bpm*num*bars;const l=state.loops[0];l.duration=length/audio.sampleRate;l.dynamicRecorded=dynamic;l.originalBpm=original;l.playbackRate=speed;return l.effectivePlaybackRate;};
 globalThis.grid=()=>SyncManager.getNextGridTime();
 globalThis.bar=()=>SyncManager.getNextBarTime(audio.currentTime);
 `,ctx);
 let cases=0,frames=0,snapshots=0,maximum=0;
 const runProbe=(commands)=>{const run=spawnSync(process.argv[2],['--audio'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);const rows=run.stdout.trim().split(/\r?\n/).map(l=>l.split(' ').map(Number));assert.equal(rows.length,commands.length);rows.forEach((r,i)=>assert.equal(r[0],1,`Rejected ${commands[i]}`));return rows;};
 for(const mode of ['static','dynamic'])for(const bpm of [600,137.5])for(const rate of [8000,48000,96000])for(const mono of [true,false])for(const scenario of ['short','full','second','long','hot']){
  if(bpm===137.5&&rate!==8000)continue;
  timers.clear();audio.sampleRate=rate;audio.currentTime=1/rate;ctx.reset(mode,bpm,1,1);await ctx.start();
  const start=mode==='static'?Math.ceil(ctx.current().graph.startTime*rate):1;
  const barFrames=rate*60/bpm,target=Math.round(barFrames),tail=Math.floor(rate*0.02);
  const capture=scenario==='short'?1:scenario==='full'?(mode==='static'?target+tail:Math.ceil(barFrames)-1):scenario==='second'?Math.floor(barFrames*0.4):Math.floor(barFrames*2.1);
  // Preserve exactly the input stream the native scheduled capture sees. In static
  // mode the original recorder also receives the pre-arm prefix, then extracts it.
  const raw=buffer(mono?1:2,(mode==='static'?start-1:0)+capture,rate);
  const waves=[Array.from({length:capture},(_,i)=>Math.fround((scenario==='hot'?3:1)*(0.1+0.3*Math.sin(i*0.11)))),Array.from({length:capture},(_,i)=>Math.fround((scenario==='hot'?3:1)*(-0.08+0.2*Math.cos(i*0.073))))];
  for(let c=0;c<raw.numberOfChannels;++c)raw.getChannelData(c).set(waves[c],mode==='static'?start-1:0);
  const commands=[`prepare 1048576 ${rate}`,`sync ${mode==='static'?1:2} ${bpm} 1 1 0`,'autoplay 0 0','tick 0 0','record 0 0'];
  const initial=commands.length;commands.push('transport 0 0');
  if(mode==='static')commands.push(...Array(start-1).fill('tick 0 0'));
  if(mode==='dynamic'&&scenario==='second'){
   // First tap well before the boundary, then a second tap to finish immediately.
   const first=Math.floor(capture/2);
   commands.push(...waves[0].slice(0,first).map((v,i)=>`tick 0 ${v}${mono?'':' '+waves[1][i]}`),'finish 0 0');
   audio.currentTime=(1+first)/rate;ctx.stop();assert(ctx.pending(),'first dynamic tap queues a bar stop');
   commands.push(...waves[0].slice(first).map((v,i)=>`tick 0 ${v}${mono?'':' '+waves[1][i+first]}`),'finish 0 0');
  }else if(mode==='dynamic'&&scenario==='full'){
   const first=Math.floor(rate*0.025);
   commands.push(...waves[0].slice(0,first).map((v,i)=>`tick 0 ${v}${mono?'':' '+waves[1][i]}`),'finish 0 0');
   audio.currentTime=(1+first)/rate;ctx.stop();const timer=timers.get(ctx.pending());assert(timer);assert(Math.abs(audio.currentTime+timer.delay/1000-ctx.bar())<1e-10);timer.fn();
   commands.push(...waves[0].slice(first).map((v,i)=>`tick 0 ${v}${mono?'':' '+waves[1][i+first]}`),'tick 0 0');
  }else{
   commands.push(...waves[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+waves[1][i]}`));
   if(mode==='static'&&capture>=target+tail)commands.push('tick 0 0');else commands.push(mode==='dynamic'?'stopImmediate 0 0':'finish 0 0');
  }
  if(mode==='static')timers.get(ctx.armTimer()).fn();
  else if(['short','long','hot'].includes(scenario))ctx.forceStop();
  const chunks=[];for(let i=0;i<raw.length;i+=128)chunks.push(Array.from({length:raw.numberOfChannels},(_,c)=>raw.getChannelData(c).slice(i,i+128)));
  await ctx.process(chunks);const expected=ctx.current().audioBuffer;assert(expected,'Reference must commit a clip');
  commands.push('waitStill 0 0');const committed=commands.length;commands.push('transport 0 0','sync 0 600 1 1 0','retrigger 0 0');
  const render=commands.length;commands.push(...Array(expected.length).fill('tick 0 0'));
  const rows=runProbe(commands);assert.equal(rows[initial][1],mode==='static'?8:1);if(mode==='static')assert.equal(rows[initial][4],start);snapshots++;
  assert.equal(rows[committed][1],3);assert.equal(rows[committed][2],expected.length,`${mode}/${scenario} length`);snapshots++;
  for(let i=0;i<expected.length;++i){const want=(expected.getChannelData(0)[i]+expected.getChannelData(mono?0:1)[i])*0.5*Math.SQRT1_2;for(const actual of rows[render+i].slice(1)){const error=Math.abs(actual-want);maximum=Math.max(maximum,error);assert(error<=1e-6,`${mode}/${scenario}/${bpm}/${rate}/${mono}: PCM ${i}, native ${actual}, JS ${want}`);}frames++;}
  cases++;
 }
 // Reference arm/count-in/cancel timing across the original default and uneven meters.
 for(const [bpm,num,bars,den] of [[120,4,2,4],[137.5,3,1,8],[93.75,7,2,8],[600,1,1,4]])for(const rate of [8000,48000])for(const countIn of [false,true])for(const countInAudio of [false,true])for(const fraction of [0,0.97]){
  const tap=Math.max(1,Math.floor(60/bpm*num*bars*rate*fraction));
  timers.clear();audio.sampleRate=rate;audio.currentTime=tap/rate;ctx.reset('static',bpm,num,bars,countIn,countInAudio);await ctx.start();
  const deadline=Math.ceil(ctx.current().graph.startTime*rate);
  const commands=[`prepare 1048576 ${rate}`,`sync 1 ${bpm} ${num} ${bars} ${countIn?1:0} ${den} ${countInAudio?1:0}`,`advance ${tap} 0`,'record 0 0','transport 0 0','finish 0 0',`advance ${deadline-tap+128} 0`,'transport 0 0'];
  const rows=runProbe(commands);assert.equal(rows[4][1],8);assert(Math.abs(rows[4][4]-deadline)<=1,`Reference count-in deadline ${rows[4][4]} vs ${deadline}`);assert.equal(rows[7][1],0,'Canceled arm must not resurrect');snapshots+=2;cases++;
 }
 let rateChecks=0;
 for(const dynamic of [false,true]){
  audio.sampleRate=8000;ctx.reset('off',120,4,2);
  const commands=['prepare 81920 8000',`sync ${dynamic?2:0} ${dynamic?600:120} 1 1 0`,'autoplay 0 0','record 0 0',...Array.from({length:512},(_,i)=>`tick 0 ${Math.fround(0.2*Math.sin(i*0.11))}`),'stopImmediate 0 0','waitStill 0 0'],checks=[];
  for(const mode of [0,1,2])for(const bpm of [60,137.5,300])for(const bars of [1,2])for(const speed of [0.25,0.5,1,2]){
   commands.push(`sync ${mode} ${bpm} 3 ${bars} 0 8`,`speed 0 ${speed}`);checks.push({row:commands.length,want:Math.fround(ctx.effective(mode,bpm,3,bars,dynamic,dynamic?600:120,512,speed))});commands.push('transport 0 0');
  }
  const rows=runProbe(commands);for(const check of checks){assert(Math.abs(rows[check.row][10]-check.want)<=1e-6,'Unchanged effectivePlaybackRate differs');rateChecks++;}
 }
 console.log(JSON.stringify({scope:'Unchanged static/dynamic capture control and committed extraction/fade/DC PCM; zero latency, browser IO/visual timers stubbed',cases,rateChecks,metadataSnapshots:snapshots,comparedFrames:frames,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true}));
})().catch(e=>{console.error(e);process.exitCode=1;});
