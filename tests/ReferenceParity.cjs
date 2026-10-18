// Executes the unchanged JS classes as the state oracle; stubs only browser/audio IO.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const context={window:{},document:{createElement:()=>({}),head:{appendChild(){}},getElementById:()=>null,activeElement:null},effects:{},FACTORY_EFFECTS:{},console,setTimeout,clearTimeout};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8'),context);
vm.runInContext(`
const state={loops:[],syncEnabled:false,playingSources:{},undoStack:[],redoStack:[],isRecording:false,recordingLoopId:-1,soloState:{active:false,loopId:-1,previousStates:{}}};
const AudioEngine={currentTime:1,scheduledFade(){}};
UIManager.updateLoop=()=>{};UIManager.updateStatus=()=>{};
state.loops=Array.from({length:20},(_,i)=>new Loop(i));
globalThis.perform=(operation,id,value)=>{
 const loop=state.loops[id];
 if(operation==='seed'){loop.audioBuffer={};loop.duration=8/48000;loop.state='playing';loop.playbackRate=1;}
 if(operation==='volume')UIManager.setLoopVolume(id,value);
 if(operation==='feedback')UIManager.setLoopFeedback(id,value);
 if(operation==='pan')UIManager.setLoopPan(id,value);
 if(operation==='speed')UIManager.setLoopSpeed(id,value);
 if(operation==='mute'){if(loop.muted!==!!value)LoopManager.toggleMute(id);}
 if(operation==='solo')SoloManager.toggleSolo(id);
 if(operation==='clear')loop.clear();
 return state.loops.map(l=>({volume:l.volume,feedback:l.feedback,pan:l.pan,speed:l.playbackRate,muted:l.muted,solo:state.soloState.active&&state.soloState.loopId===l.id,effective:l.effectiveVolume}));
};`,context);
const commands=[['seed',0,0],['mute',0,1],['clear',0,0],['seed',1,0],['solo',1,0],['clear',2,0],['solo',1,0],['pan',3,3.9]];
let seed=4711;const random=n=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed%n;};
for(let i=0;i<400;++i){const action=['volume','feedback','pan','speed','mute','solo','clear'][random(7)],id=random(20);const value=action==='pan'?random(11):action==='speed'?[0.25,0.5,1,2][random(4)]:action==='volume'?random(201)/100:action==='feedback'?random(101)/100:random(2);commands.push([action,id,value]);}
const expected=commands.map(c=>context.perform(...c));
const run=spawnSync(process.argv[2],[],{input:commands.map(c=>c.join(' ')).join('\n')+'\n',encoding:'utf8',maxBuffer:4*1024*1024});
assert.equal(run.status,0,run.stderr);const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,commands.length);
for(let n=0;n<commands.length;++n){const values=lines[n].split(' ').map(Number);assert.equal(values[0],1,'rejected '+commands[n]);const soloActive=expected[n].some(t=>t.solo);for(let i=0;i<20;++i){const actual=values.slice(1+i*6,7+i*6),oracle=expected[n][i],want=[oracle.volume,oracle.feedback,oracle.pan,oracle.speed,+oracle.muted,+oracle.solo];for(let f=0;f<6;++f)assert(Math.abs(actual[f]-want[f])<1e-6,`JS/native mismatch after ${n} ${commands[n]} track ${i} field ${f}: ${actual[f]} vs ${want[f]}`);const effective=actual[4]||(soloActive&&!actual[5])?0:actual[0];assert(Math.abs(effective-oracle.effective)<1e-6,'effective mute/solo volume');}}
console.log(`PASS: ${commands.length} traces / ${commands.length*20} loop snapshots match unchanged JS setters, mute, clear and solo. Browser rendering/audio capture are outside this oracle.`);
