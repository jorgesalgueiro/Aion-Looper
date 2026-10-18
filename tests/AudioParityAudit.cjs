// Native capture -> worker finalization -> committed playback, with unchanged JS oracle.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const ctx={document:{createElement:()=>({}),head:{appendChild(){}}},window:{},setTimeout,clearTimeout,console};vm.createContext(ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8'),match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(source);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=source.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<source.length);if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:{state:'running'}};globalThis.dc=finalBuffer=>{${source.slice(start,end)}};globalThis.fade=(b,s)=>AudioEngine.seamlessLoopCrossfade(b,s);globalThis.mix=(a,b,o,f)=>AudioEngine.mixBuffersCircular(a,b,o,f);`,ctx);
 const buffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]};};
 let compared=0,maximum=0,cases=0;
 for(const mono of [false,true])for(const rate of [8000,48000,96000])for(const length of [1,4,17,512,1025,4097,48001])for(const mode of ['record','overdub','substitute']){
   const takeLength=mode==='record'?0:Math.max(1,Math.floor(length*1.7)),offset=length>4?Math.floor(length/3):0;
   const feedback=[0,0.8,1][Math.floor(cases/3)%3];
   const original=[[],[]],take=[[],[]];
   for(let i=0;i<length;i++){original[0].push(Math.fround(0.15+0.4*Math.sin(i*0.11)));original[1].push(Math.fround(-0.08+0.3*Math.cos(i*0.073)));}
   for(let i=0;i<takeLength;i++){take[0].push(Math.fround(0.1+0.6*Math.cos(i*0.071)));take[1].push(Math.fround(-0.2+0.5*Math.sin(i*0.13)));}
   const base=buffer(mono?[original[0]]:original,rate);ctx.fade(base,0.015);ctx.dc(base);
   if(mode!=='record'){const added=buffer(mono?[take[0]]:take,rate);ctx.dc(added);ctx.fade(added,0.008);await ctx.mix(base,added,offset/rate,mode==='substitute'?0:feedback);}
   const commands=[`prepare 81920 ${rate}`,'record 0 0',...original[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+original[1][i]}`),'finish 0 0','wait 0 0','retrigger 0 0'];
   if(mode!=='record')commands.push(`feedback 0 ${feedback}`,...Array(offset).fill('tick 0 0'),`${mode} 0 0`,...take[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+take[1][i]}`),'finish 0 0','wait 0 0','retrigger 0 0');
   commands.push(...Array(length).fill('tick 0 0'));
   const run=spawnSync(process.argv[2],['--audio'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);
   const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
   rows.forEach((r,i)=>assert.equal(Number(r.split(' ')[0]),1,`Native command rejected: ${commands[i]}`));
   const rendered=rows.slice(-length);
   for(let i=0;i<length;i++){const values=rendered[i].split(' ').map(Number),expected=(base.getChannelData(0)[i]+base.getChannelData(mono?0:1)[i])*0.5*Math.SQRT1_2;const error=Math.abs(values[1]-expected);maximum=Math.max(maximum,error);compared++;assert(error<=1e-6,`${mode} length ${length}, sample ${i}: ${values[1]} vs JS ${expected}`);assert(Math.abs(values[2]-expected)<=1e-6);}
   cases++;
 }
 console.log(JSON.stringify({scope:'Committed capture/overdub/substitute PCM through center pan; playback envelopes, browser resampling, latency compensation and sync excluded',cases,comparedSamples:compared,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true},null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
