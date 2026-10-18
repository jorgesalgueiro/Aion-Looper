// Unchanged Loop/LoopManager history methods and DSP are the oracle; audio IO is stubbed.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const buffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c],copyToChannel:(data,c)=>channels[c].set(data)};};
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>buffer(Array.from({length:count},()=>Array(length).fill(0)),rate)};
 const ctx={window:{},document:{createElement:()=>({}),head:{appendChild(){}},getElementById:()=>null},effects:{},FACTORY_EFFECTS:{},console,setTimeout,clearTimeout,audio};vm.createContext(ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(source,ctx);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(source);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=source.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<source.length);if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,loops:[],undoStack:[],redoStack:[],syncEnabled:false,playingSources:{},isRecording:false,recordingLoopId:-1,soloState:{active:false,loopId:-1,previousStates:{}}};
 UIManager.updateLoop=()=>{};UIManager.updateStatus=()=>{};
 const dc=finalBuffer=>{${source.slice(start,end)}};
 globalThis.reset=()=>{state.loops=Array.from({length:20},(_,i)=>new Loop(i));state.undoStack=[];state.redoStack=[];};
 globalThis.seed=(id,b)=>{AudioEngine.seamlessLoopCrossfade(b,0.015);dc(b);const l=state.loops[id];l.audioBuffer=b;l.duration=b.duration;l.state='stopped';};
 globalThis.edit=async(id,take,offset,feedback)=>{dc(take);AudioEngine.seamlessLoopCrossfade(take,0.008);LoopManager.pushUndoState(id);await AudioEngine.mixBuffersCircular(state.loops[id].audioBuffer,take,offset,feedback);};
 globalThis.perform=(action,id)=>{if(action==='normalize')state.loops[id].normalize();else if(action==='clear')state.loops[id].clear();else if(action==='undoGlobal'){const id=state.undoStack.at(-1);if(id!==undefined)state.loops[id].undo();}else if(action==='redoGlobal'){const id=state.redoStack.at(-1);if(id!==undefined)state.loops[id].redo();}else state.loops[id][action]();};
 globalThis.history=()=>({counts:state.loops.flatMap(l=>[l.undoStack.length,l.redoStack.length]),undo:state.undoStack.at(-1)??-1,redo:state.redoStack.at(-1)??-1});
 globalThis.stored=id=>state.loops[id].audioBuffer;
 `,ctx);
 let cases=0,frames=0,metadataChecks=0,maximum=0;
 const exercise=async({rate,length,mono,global=false})=>{
  ctx.reset();const commands=[`prepare 81920 ${rate}`],checks=[],histories=[];
  const wave=(count,seed)=>[Array.from({length:count},(_,i)=>Math.fround(0.1+0.3*Math.sin(i*0.11+seed))),Array.from({length:count},(_,i)=>Math.fround(-0.08+0.2*Math.cos(i*0.073+seed)))];
  const addHistory=()=>{histories.push({row:commands.length,want:ctx.history()});commands.push('history 0 0');};
  const checkPcm=id=>{const b=ctx.stored(id);if(!b)return;commands.push('stopAll 0 0',`retrigger ${id} 0`);for(let i=0;i<b.length;++i){checks.push({row:commands.length,want:(b.getChannelData(0)[i]+b.getChannelData(mono?0:1)[i])*0.5*Math.SQRT1_2});commands.push('tick 0 0');}commands.push(`stop ${id} 0`);};
  const seed=id=>{const raw=wave(length,id);ctx.seed(id,buffer(mono?[raw[0]]:raw,rate));commands.push(`record ${id} 0`,...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),`finish ${id} 0`,'waitStill 0 0',`stop ${id} 0`);addHistory();};
  const edit=async(id,n)=>{const take=wave(Math.max(1,Math.floor(length*1.7)),n*0.37+1),offset=Math.floor(length/3),feedback=n%3===0?0:n%3===1?0.8:1;
   await ctx.edit(id,buffer(mono?[take[0]]:take,rate),offset/rate,feedback);
   commands.push('stopAll 0 0',`feedback ${id} ${feedback}`,`retrigger ${id} 0`,...Array(offset).fill('tick 0 0'),`${feedback===0?'substitute':'overdub'} ${id} 0`,...take[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+take[1][i]}`),`finish ${id} 0`,'waitStill 0 0',`stop ${id} 0`);addHistory();if(!global)checkPcm(id);
  };
  const action=(name,id=0)=>{const h=ctx.history();let accepted=true;
   if(name==='undo')accepted=h.counts[id*2]>0;
   if(name==='redo')accepted=h.counts[id*2+1]>0;
   if(name==='undoGlobal')accepted=h.undo>=0;
   if(name==='redoGlobal')accepted=h.redo>=0;
   checks.push({row:commands.length,accepted});commands.push(`${name} ${id} 0`);ctx.perform(name,id);if(name==='normalize')commands.push('waitStill 0 0');addHistory();
   if(accepted&&!global&&name!=='clear')checkPcm(id);
  };
  if(global){
   for(let id=0;id<20;++id)seed(id);
   for(let round=0;round<6;++round)for(let id=0;id<20;++id)await edit(id,round+id);
   // Individual restoration interacts with global entries and per-loop eviction.
   for(let n=0;n<5;++n)action('undo',0);
   action('redoGlobal');action('redo',0);await edit(1,99);
   while(ctx.history().undo>=0)action('undoGlobal');
   action('undoGlobal');
   while(ctx.history().redo>=0)action('redoGlobal');
   action('redoGlobal');
   for(let id=0;id<20;++id)action('clear',id);
  }else{
   seed(0);checkPcm(0);action('undo');action('redo');
   for(let n=0;n<8;++n)await edit(0,n);
   action('normalize');
   for(let n=0;n<6;++n)action('undoGlobal');
   for(let n=0;n<6;++n)action('redoGlobal');
   action('undo');action('undo');await edit(0,12);action('redo');
   action('undo');action('clear');action('redo');
  }
  const run=spawnSync(process.argv[2],['--audio'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
  const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
  const rejected=new Set(checks.filter(c=>c.accepted===false).map(c=>c.row));
  rows.forEach((line,i)=>assert.equal(Number(line.split(' ')[0]),rejected.has(i)?0:1,`Unexpected acceptance at ${commands[i]}`));
  for(const h of histories){const values=rows[h.row].split(' ').map(Number).slice(1);assert.deepEqual(values,[...h.want.counts,h.want.undo,h.want.redo],`History mismatch, global=${global}, command ${h.row}`);metadataChecks++;}
  for(const c of checks){if(c.want===undefined)continue;for(const actual of rows[c.row].split(' ').map(Number).slice(1)){const error=Math.abs(actual-c.want);maximum=Math.max(maximum,error);assert(error<=1e-6,`Undo PCM ${actual} vs JS ${c.want}`);}frames++;}
  cases++;
 };
 for(const mono of [true,false])for(const rate of [8000,48000,96000])for(const length of [17,512,4097])await exercise({mono,rate,length});
 await exercise({mono:false,rate:48000,length:17,global:true});
 console.log(JSON.stringify({scope:'Unchanged JS per-loop/global history and restored committed PCM; host/hardware/browser envelopes excluded',cases,historySnapshots:metadataChecks,comparedFrames:frames,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true}));
})().catch(error=>{console.error(error);process.exitCode=1;});
