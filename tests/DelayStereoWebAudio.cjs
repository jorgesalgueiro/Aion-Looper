// SPDX-License-Identifier: AGPL-3.0-only
// First echo / wet-only panning / original final mono-merger subset. This is
// deliberately NOT a comparison of the new feedback filter or time transitions.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const {connect,close}=require('./ArpDelayWebAudio.cjs');
const root=path.resolve(__dirname,'..'),read=p=>fs.readFileSync(p,'utf8').replace(/\r\n/g,'\n');
const original=read(path.join(root,'resources/ui/Modules/loopTracks.js'));
const reference=read(path.join(root,'.reference/Ascii-Looper/Modules/loopTracks.js'));
const method=(source,name)=>{const begin=source.indexOf('    '+name+'('),end=source.indexOf('\n    }',begin)+6;assert(begin>=0&&end>begin,name);return source.slice(begin,end);};
const methods=['_createDelay','_addAutoPanToWet','_addPanControl'].map(name=>{const m=method(original,name);assert.equal(m,method(reference,name),'unchanged reference '+name);return m;}).join('\n');
const integerOnly=process.argv.includes('--integer-only');
const probe=process.argv.slice(2).find(a=>!a.startsWith('--'))||path.join(root,'build/windows-core/Release/aion_delay_tests.exe');
(async()=>{const {evaluate,browser}=await connect();let maximum=0,cases=0,frames=0;
 try{
  for(const rate of [8000,48000])for(const shape of ['left','right','stereo'])for(const pan of [0,2,5,8,10]){
   // Keep the strict fractional failure as the default diagnostic. The explicit
   // integer subset isolates stereo routing/panning without changing tolerance
   // or pretending Chromium's Float32 read-coordinate matches native precision.
   const mix=pan===5?1:.63,speed=pan===0?0:pan===8?3.7:5,depth=pan===8?.7:1,time=integerOnly?.0625:.05017;
   const data=await evaluate(`(async()=>{
    const rate=${rate},count=16384,shape=${JSON.stringify(shape)},pan=${pan};
    const ctx=new OfflineAudioContext(2,count,rate),state={audioContext:ctx,bpm:120};
    const params={time:${time},repetitions:0,damp:3000,mix:${mix},panSpeed:${speed},panDepth:${depth},sync:false};
    const effects={delay:params};class Graph{constructor(){this.loop={pan,params:{delay:params}};} ${methods}}
    const graph=new Graph(),buffer=ctx.createBuffer(2,count,rate),input=[];
    for(let c=0;c<2;++c){const a=buffer.getChannelData(c);for(let n=0;n<count;++n)a[n]=(shape==='left'&&c===1)||(shape==='right'&&c===0)?0:.08*Math.sin(n*(c?.047:.031))+.03*Math.cos(n*(c?.083:.071));input.push(Array.from(a));}
    const source=ctx.createBufferSource();source.buffer=buffer;const delay=graph._createDelay(source),output=graph._addPanControl(delay.output);output.merger.connect(ctx.destination);source.start();
    const rendered=await ctx.startRendering();return {input,pcm:[Array.from(rendered.getChannelData(0)),Array.from(rendered.getChannelData(1))]};
   })()`);
   const input=data.input[0].map((v,n)=>v+' '+data.input[1][n]).join('\n')+'\n';
   for(const exact of [0,1]){
    const result=spawnSync(probe,['stereo',rate,time,mix,speed,depth,exact,pan].map(String),{input,encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(result.status,0,result.stderr);
    const actual=result.stdout.trim().split(/\s+/).map(Number);assert.equal(actual.length,32768);
    let error=0,peak=0;for(let n=0;n<16384;++n)for(let c=0;c<2;++c){const a=actual[2*n+c],b=data.pcm[c][n];assert(Number.isFinite(a)&&Number.isFinite(b));error=Math.max(error,Math.abs(a-b));peak=Math.max(peak,Math.abs(b));}
    assert(peak>.01,'audible browser oracle');
    if(error>2e-6){fs.writeFileSync(path.join(root,'build/delay-stereo-failure.json'),JSON.stringify({rate,shape,pan,exact,time,mix,speed,depth,input:data.input,pcm:data.pcm,actual}));}
    assert(error<=2e-6,JSON.stringify({rate,shape,pan,exact,error}));maximum=Math.max(maximum,error);++cases;frames+=16384;
   }
  }
  console.log(JSON.stringify({result:'PASS',scope:'unchanged Delay first echo / wet-only StereoPanner / original final loop downmix-pan; feedback0 and native exact1 only',integerOnly,cases,stereoFrames:frames,maximumAbsoluteError:maximum,browser:browser.product}));
 }finally{close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
