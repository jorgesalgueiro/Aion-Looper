// Execute unchanged UIManager.setLoopStartDelay, Loop.restart, AudioGraph and buffer DSP.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]};};
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){},start(time,offset){this.started={time,offset};},stop(){},gain:param(),playbackRate:param()});
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:node,createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(loops,context);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],customEffects:{}};
 UIManager.updateLoop=()=>{};
 globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};
 globalThis.setup=b=>{const loop=new Loop(0);loop.audioBuffer=b;loop.duration=b.duration;loop.state='playing';state.loops=[loop];loop.graph=new AudioGraph(loop);if(!loop.graph.build())throw Error('Reference graph failed');};
 globalThis.toggle=()=>state.loops[0].toggleEffect('reverse');
 globalThis.restart=()=>state.loops[0].graph.retrigger(0,state.loops[0]);
 globalThis.stored=()=>state.loops[0].audioBuffer;
 globalThis.source=()=>state.loops[0].graph.source.buffer;
 globalThis.normalize=()=>state.loops[0].normalize();
 globalThis.edit=async(take,offset,feedback)=>{const finalBuffer=take;${loops.slice(start,end)};AudioEngine.seamlessLoopCrossfade(take,0.008);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,offset,feedback);};`,context);

 vm.runInContext('globalThis.peaks=b=>UIManager.generateWaveformPeaks(b);',context);
 let cases=0,compared=0,maximum=0;
 for(const mono of [true,false])for(const sampleRate of [8000,48000,96000])for(const length of [1,17,512,720,721,4097,20003]){
  const raw=[Array.from({length},(_,i)=>Math.fround(0.1+0.4*Math.sin(i*0.17))),Array.from({length},(_,i)=>Math.fround(-0.05+0.7*Math.cos(i*0.093)))];
  const buffer=makeBuffer(mono?[raw[0]]:raw,sampleRate);context.condition(buffer);const expected=context.peaks(buffer);
  const commands=[`prepare 81920 ${sampleRate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0','peaks 0 0'];
  const result=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(result.status,0,result.stderr);
  const actual=result.stdout.trim().split(/\r?\n/).at(-1).split(' ').map(Number);assert.equal(actual.shift(),1);assert.equal(actual.length,720);
  for(let i=0;i<720;++i){const error=Math.abs(actual[i]-expected[i]);maximum=Math.max(maximum,error);assert(error<=1e-6,`Waveform ${mono}/${sampleRate}/${length}/${i}: ${actual[i]} != ${expected[i]}`);compared++;}cases++;
 }
 console.log(JSON.stringify({result:'PASS',cases,compared,maximumAbsoluteError:maximum,tolerance:1e-6,oracle:'Unchanged generateWaveformPeaks on reference-conditioned mono/stereo PCM, 720 bins including ceil partition/padded tail'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
