// Current-feature work only: compare offline finalization kernels with unchanged JS.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const ctx={document:{createElement:()=>({}),head:{appendChild(){}}},window:{},setTimeout,clearTimeout,console};vm.createContext(ctx);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),ctx);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
 const dcBlock=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(dcBlock,'Original capture DC block must exist');
 const start=dcBlock.index+dcBlock[0].indexOf('for (');
 const brace=loops.indexOf('{',start);let depth=1,end=brace+1;
 for(;depth&&end<loops.length;end++){if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 assert.equal(depth,0);
 vm.runInContext(`const state={audioContext:{state:'running'}};
 globalThis.fade=(buffer,seconds)=>AudioEngine.seamlessLoopCrossfade(buffer,seconds);
 globalThis.mix=(base,take,offset,feedback)=>AudioEngine.mixBuffersCircular(base,take,offset,feedback);
 globalThis.dc=finalBuffer=>{${loops.slice(start,end)}};`,ctx);
 const cases=[];let randomSeed=17;
 const random=()=>{randomSeed=(Math.imul(randomSeed,1664525)+1013904223)>>>0;return randomSeed/0x100000000;};
 const samples=(count,bias=0)=>Array.from({length:count},(_,i)=>Math.fround(bias+0.7*Math.sin(i*0.071)+0.3*(random()-0.5)));
 const buffer=(values,rate)=>{const data=Float32Array.from(values);return {length:data.length,sampleRate:rate,numberOfChannels:1,getChannelData:()=>data};};
 for(const length of [0,1,2,3,4,8,9,10,16,39,40,511,512,1024,12345]){
   for(const rate of [8000,44100,48000,96000,192000]){
     const base=samples(length,0.13),dc=buffer(base,rate);ctx.dc(dc);
     cases.push({action:'dc',base,take:[],rate,offset:0,feedback:0.8,fade:0.015,expected:[...dc.getChannelData(0)]});
     for(const fade of [0,0.008,0.015,0.1]){const data=buffer(base,rate);ctx.fade(data,fade);cases.push({action:'fade',base,take:[],rate,offset:0,feedback:0.8,fade,expected:[...data.getChannelData(0)]});}
   }
 }
 for(let n=0;n<96;n++){
   const rate=[8000,44100,48000,96000][n%4],length=[1,4,17,511,512,997][n%6],takeLength=[0,1,2,7,19,512,1025,48001][n%8];
   const base=n%3===0?Array(length).fill(n%2?-0.9:0.9):samples(length),take=n%3===0?Array(takeLength).fill(n%2?-0.9:0.9):samples(takeLength,0.1);
   const offset=[0,-0.03,0.007,1.25][n%4],feedback=[0,0.8,1][n%3],data=buffer(base,rate);
   await ctx.mix(data,buffer(take,rate),offset,feedback);
   cases.push({action:'mix',base,take,rate,offset,feedback,fade:0.015,expected:[...data.getChannelData(0)]});
 }
 // Retain the original 512-frame raw-mixer regression when broadening the live audit.
 {const base=Array(512).fill(0.2),take=Array(512).fill(0.1),data=buffer(base,48000);await ctx.mix(data,buffer(take,48000),0,0.8);cases.push({action:'mix',base,take,rate:48000,offset:0,feedback:0.8,fade:0.015,expected:[...data.getChannelData(0)]});}
 const input=cases.map(c=>[c.action,c.base.length,c.take.length,c.rate,c.offset,c.feedback,c.fade,...c.base,...c.take].join(' ')).join('\n')+'\n';
 const result=spawnSync(process.argv[2],[],{input,encoding:'utf8',maxBuffer:64*1024*1024});assert.equal(result.status,0,result.stderr);
 const lines=result.stdout.trim().split(/\r?\n/);assert.equal(lines.length,cases.length);
 let maximum=0,compared=0;
 for(let n=0;n<cases.length;n++){const output=lines[n].split(' ').map(Number);assert.equal(output.shift(),1);const test=cases[n];assert.equal(output.length,test.expected.length);for(let i=0;i<output.length;i++){const error=Math.abs(output[i]-test.expected[i]);maximum=Math.max(maximum,error);compared++;assert(error<=1e-6,`${test.action} fixture ${n} sample ${i}: native ${output[i]}, JS ${test.expected[i]}`);}}
 console.log(JSON.stringify({scope:'Offline kernels only; live capture integration remains gated',cases:cases.length,comparedSamples:compared,maximumAbsoluteError:maximum,tolerance:1e-6,parity:true}));
})().catch(error=>{console.error(error);process.exitCode=1;});
