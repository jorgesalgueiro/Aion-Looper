// Deliberately strict diagnostic: exits 1 while native overdub differs from reference DSP.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..'),length=512,rate=48000;
 const ctx={document:{createElement:()=>({}),head:{appendChild(){}}},window:{},setTimeout,clearTimeout,console};vm.createContext(ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 vm.runInContext(`const state={audioContext:{state:'running'}};globalThis.mix=(a,b)=>AudioEngine.mixBuffersCircular(a,b,0,0.8);`,ctx);
 const buffer=value=>{const channels=[new Float32Array(length).fill(value),new Float32Array(length).fill(value)];return {length,sampleRate:rate,numberOfChannels:2,getChannelData:c=>channels[c]};};
 const base=buffer(0.2),added=buffer(0.1);await ctx.mix(base,added);
 const commands=['record 0 0',...Array(length).fill('tick 0 0.2'),'finish 0 0','overdub 0 0',...Array(length).fill('tick 0 0.1'),'finish 0 0','retrigger 0 0',...Array(length).fill('tick 0 0')];
 const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/).slice(-length);let maxError=0,mismatched=0;
 lines.forEach((line,i)=>{const values=line.split(' ').map(Number),actual=values[values.length-2],expected=base.getChannelData(0)[i]*Math.SQRT1_2;const error=Math.abs(actual-expected);maxError=Math.max(maxError,error);if(error>1e-6)mismatched++;});
 console.log(JSON.stringify({scope:'Core overdub boundary mix only; capture DC removal, capture fades, resampling and browser rendering excluded',samples:length,mismatched,maxAbsoluteError:maxError,parity:mismatched===0},null,2));
 if(mismatched)process.exitCode=1;
})().catch(error=>{console.error(error);process.exitCode=2;});
