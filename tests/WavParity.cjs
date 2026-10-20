const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),context={console,window:{},document:{createElement:()=>({}),head:{appendChild(){}}},Log:{warn(){}}};vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
vm.runInContext('globalThis.wav=b=>AudioEngine.bufferToWAV(b)',context);
const buffer=(channels,rate)=>({length:channels[0].length,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]});
let cases=0,bytes=0;
const compare=(actual,expected,label)=>{assert.deepEqual(actual,expected,label);cases++;bytes+=actual.length;};
const raw=[NaN,Infinity,-Infinity,-8,-1,-0.99999,-0.5,-1/32768,-0,0,1/32767,0.5,0.99999,1,8];
for(const rate of [8000,44100,48000,96000,192000,48000.75,NaN])for(const channelCount of [1,2])for(const length of [0,1,17,4097]){
 const data=Array.from({length:channelCount},(_,c)=>Float32Array.from({length},(_,i)=>i<raw.length?raw[(i+c*3)%raw.length]:1.2*Math.sin(i*0.13+c)));
 const samples=Array.from({length},(_,i)=>data.map(c=>c[i]).join(' ')).join('\n');
 const result=spawnSync(process.argv[2],[],{input:`${channelCount} ${length} ${rate}\n${samples}\n`,encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(result.status,0,result.stderr);
 compare(Buffer.from(result.stdout.trim(),'hex'),Buffer.from(context.wav(buffer(data,rate))),`original encoder ${rate}/${channelCount}/${length}`);
}
// Connected capture/finalization, followed by export: use the unchanged original
// extraction conditioning, not mixer output or a duplicate native encoder model.
const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
vm.runInContext(`globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};`,context);
for(const rate of [8000,48000,96000])for(const mono of [false,true])for(const length of [1,17,512,4097,20003]){
 const data=[Float32Array.from({length},(_,i)=>0.1+0.4*Math.sin(i*0.17)),Float32Array.from({length},(_,i)=>-0.05+0.7*Math.cos(i*0.093))];if(mono)data[1]=data[0].slice();
 const expected=buffer(data.map(c=>c.slice()),rate);context.condition(expected);
 const input=[`prepare 81920 ${rate}`,'record 0 0',...Array.from({length},(_,i)=>`tick 0 ${data[0][i]} ${data[1][i]}`),'finish 0 0','wait 0 0','export 0 0',
  // Export ignores playback-only controls, including an active held STUT region.
  'reverse 0 1','speed 0 0.37','delay 0 0.6','apply 0 0','tick 0 0','stut 0 1','export 0 0'];
 const result=spawnSync(process.argv[3],[],{input:input.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(result.status,0,result.stderr);
 const lines=result.stdout.trim().split(/\r?\n/),native=lines.at(-1),plain=lines.at(-8);
 compare(Buffer.from(native,'hex'),Buffer.from(context.wav(expected)),`connected original PCM ${rate}/${mono}/${length}`);
 assert.equal(native,plain,'Playback-only changes must not alter exported bytes');
}
// Original exportLoop filename, fallback and exact JS UTF-16 replacement semantics.
vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/projectManager.js'),'utf8'),context);
vm.runInContext(`const state={loops:[]};globalThis.nameOracle=(name,id)=>{state.loops[id]={name,id,audioBuffer:{}};AudioEngine.bufferToWAV=()=>new ArrayBuffer(0);ProjectManager.downloadWAV=(_,filename)=>globalThis.filename=filename;ProjectManager.exportLoop(id);return filename;};Date.now=()=>1234567890;`,context);
const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
for(const name of ['',null,'Bass 1','ação / loop','🎵','../bad:take?','A_B-C'])for(const id of [0,19])assert.equal(native.window.AionExportName(name,id,1234567890),context.nameOracle(name,id));
console.log(JSON.stringify({result:'PASS',cases,bytes,maximumByteDifference:0,oracle:'Unchanged AudioEngine.bufferToWAV/encodeWAV, original capture conditioning and ProjectManager.exportLoop filename; raw stored PCM ignores playback controls'}));
