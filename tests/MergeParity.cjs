// Execute the unchanged user-level merge and capture conditioning, not a model
// of the mixer. Compare native stored stereo WAV and committed playback/history.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
const method=name=>{const start=loops.indexOf(`    static ${name}(`),end=loops.indexOf('\n    }',start)+6;assert(start>=0);return loops.slice(start,end);};
const pending=[],alerts=[],context={console,window:{UIManager:null},document:{createElement:()=>({}),head:{appendChild(){}}},setTimeout:(fn,ms)=>ms===50?(pending.push(fn),0):setTimeout(fn,ms),alert:m=>alerts.push(m),confirm:()=>false};vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
vm.runInContext(`const state={audioContext:{state:'running'},loops:[],undoStack:[],redoStack:[]};class LoopManager{${method('async mergeLoops')} ${method('pushUndoState')}};const UIManager={generateWaveformPeaks:()=>[],updateLoop(){}};globalThis.oracle={state,merge:(d,s)=>LoopManager.mergeLoops(d,s),wav:b=>AudioEngine.bufferToWAV(b),fade:b=>AudioEngine.seamlessLoopCrossfade(b,0.015)};`,context);
const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;for(;depth;end++){if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
vm.runInContext(`globalThis.dc=finalBuffer=>{${loops.slice(start,end)}};`,context);
const buffer=(data,rate)=>({length:data[0].length,sampleRate:rate,numberOfChannels:data.length,getChannelData:c=>data[c],get duration(){return this.length/this.sampleRate;}});
const copy=b=>buffer(Array.from({length:b.numberOfChannels},(_,c)=>b.getChannelData(c).slice()),b.sampleRate);
const state=context.oracle.state;state.audioContext.createBuffer=(channels,length,rate)=>buffer(Array.from({length:channels},()=>new Float32Array(length)),rate);
const loop=(id,b)=>({id,audioBuffer:b,duration:b?.duration??0,originalBpm:133,dynamicRecorded:true,state:b?'playing':'empty',undoStack:[],saveStateForUndo(){this.undoStack.push(copy(this.audioBuffer));return true;},clear(){this.audioBuffer=null;this.state='empty';}});
let cases=0,bytes=0,maximum=0;
(async()=>{
 for(const rate of [8000,48000,96000])for(const mono of [false,true])for(const [length,sourceLength] of [[1,17],[17,1],[17,70],[512,1025],[4097,20003],[512,48001]])for(const clone of [false,true]){
  pending.length=0;state.undoStack=[];state.redoStack=[];
  const data=n=>Array.from({length:2},(_,c)=>Float32Array.from({length:n},(_,i)=>0.15+0.93*(c?Math.cos(i*0.093):Math.sin(i*0.17))));
  const raw=data(length),sourceRaw=data(sourceLength);if(mono){raw[1]=raw[0].slice();sourceRaw[1]=sourceRaw[0].slice();}
  const base=buffer(raw.map(c=>c.slice()),rate),source=buffer(sourceRaw.map(c=>c.slice()),rate);for(const b of [base,source]){context.oracle.fade(b);context.dc(b);}
  const original=Buffer.from(context.oracle.wav(base)),sourceWav=Buffer.from(context.oracle.wav(source));
  state.loops=[loop(0,clone?null:base),loop(1,source)];await context.oracle.merge(0,'1');const expected=state.loops[0];
  assert.equal(state.undoStack.length,clone?0:1);assert.equal(expected.state,clone?'stopped':'playing');assert.equal(expected.audioBuffer.length,clone?sourceLength:length);assert.equal(pending.length,1);assert.equal(state.loops[1].audioBuffer,source);
  const commands=[`prepare 655360 ${rate}`];
  const seed=(id,raw)=>commands.push(`record ${id} 0`,...Array.from({length:raw[0].length},(_,i)=>`tick 0 ${raw[0][i]} ${raw[1][i]}`),`finish ${id} 0`,'wait 0 0',`stop ${id} 0`);
  if(!clone)seed(0,raw);seed(1,sourceRaw);commands.push('merge 0 1','wait 0 0','inspect 0 0','export 0 0','export 1 0');
  if(!clone)commands.push('undo 0 0','export 0 0','redo 0 0','export 0 0');
  commands.push('solo 0 0','retrigger 0 0',...Array.from({length:expected.audioBuffer.length},()=>`tick 0 0`));
  const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:64*1024*1024});assert.equal(run.status,0,run.stderr);
  const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
  const exported=commands.map((c,i)=>c.startsWith('export ')?rows[i]:null).filter(v=>v!==null);
  const expectedWav=Buffer.from(context.oracle.wav(expected.audioBuffer));
  assert.deepEqual(Buffer.from(exported[0],'hex'),expectedWav,`Stored stereo bytes ${rate}/${mono}/${length}/${sourceLength}/${clone}`);
  assert.deepEqual(Buffer.from(exported[1],'hex'),sourceWav,'Merge never mutates source PCM');
  if(!clone){assert.deepEqual(Buffer.from(exported[2],'hex'),original,'Undo restores original');assert.deepEqual(Buffer.from(exported[3],'hex'),expectedWav,'Redo restores merge');}
  for(let i=0;i<commands.length;++i)if(!commands[i].startsWith('export '))assert.equal(Number(rows[i].split(' ')[0]),1,commands[i]);
  const inspect=rows[commands.indexOf('inspect 0 0')].split(' ').map(Number);assert.equal(inspect[1],expected.audioBuffer.length);assert.equal(inspect[2],3);assert.equal(inspect[5],clone?0:1);
  const rendered=rows.slice(-expected.audioBuffer.length);for(let i=0;i<rendered.length;++i){const r=rendered[i].split(' ').map(Number),want=(expected.audioBuffer.getChannelData(0)[i]+expected.audioBuffer.getChannelData(1)[i])*0.5*Math.SQRT1_2;for(const actual of r.slice(-2)){maximum=Math.max(maximum,Math.abs(actual-want));assert(Math.abs(actual-want)<=1e-6);}}
  bytes+=expectedWav.length;cases++;
 }
 const b=buffer([Float32Array.from([0.2,0.3])],48000);state.loops=[loop(0,null),loop(1,b)];await context.oracle.merge(0,'');assert.equal(state.loops[0].audioBuffer,null);await context.oracle.merge(0,'0');assert(alerts.at(-1).includes('itself'));await context.oracle.merge(1,'0');assert(alerts.at(-1).includes('empty'));
 console.log(JSON.stringify({result:'PASS',cases,storedBytes:bytes,maximumPlaybackError:maximum,tolerance:1e-6,oracle:'Unchanged LoopManager.mergeLoops + mixBuffersCircular/sliceBuffer, conditioned capture; exact stereo PCM16, circular/clipped mix, clone, source preservation and Undo/Redo'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
