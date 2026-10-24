// Execute the complete unchanged AudioEngine.readMeter with real Float32 windows.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8');
const start=source.indexOf('    static readMeter('),end=source.indexOf('\n    }',start)+6;assert(start>=0&&end>start);
const state={audioContext:null},oracle={state};vm.createContext(oracle);
vm.runInContext(`class AudioEngine{${source.slice(start,end)}};globalThis.read=(a,d,m,p,n)=>AudioEngine.readMeter(a,d,m,p,n);`,oracle);
const contexts=new Map(),analysers=new Map(),arrays=new Map(),meter={},commands=[],expected=[];
let samples=Float32Array.of(0),now=0,settings={context:1,analyser:1,data:1,running:true};
const token=v=>Number.isNaN(v)?'nan':v===Infinity?'inf':v===-Infinity?'-inf':String(v);
const step=(n,p,values,changes={})=>{
 Object.assign(settings,changes);now=n;samples=Float32Array.from(values);
 const {context,analyser,data,running}=settings;
 if(context&&!contexts.has(context))contexts.set(context,{});
 state.audioContext=context?contexts.get(context):null;if(state.audioContext)state.audioContext.state=running?'running':'suspended';
 if(analyser&&!analysers.has(analyser))analysers.set(analyser,{getFloatTimeDomainData(array){array.set(samples);}});
 const key=`${data}:${samples.length}`;if(data&&!arrays.has(key))arrays.set(key,new Float32Array(samples.length));
 // Each identity in a trace has one size, just like the analyser's real data array.
 const array=data?arrays.get(key):null;
 oracle.read(analyser?analysers.get(analyser):null,array,meter,p,n);
 expected.push([meter.rms,meter.peak,meter.linearPeak,meter.peakDb,meter.rmsPercent,meter.peakPercent,Number(meter.clipped)]);
 commands.push([context,analyser,data,running?1:0,token(n),token(p),samples.length,...Array.from(samples,token)].join(' '));
};
// Every actual array replacement carries a fresh identity; transitions back to
// an existing array are also tested. The native component receives only spans.
let identity=1;
for(const size of [0,1,64,128,256,2048,32768]){
 const data=++identity;
 step(0,0,new Array(size).fill(0),{context:++identity,analyser:++identity,data,running:true});
 for(const level of [0,0.000999,0.001,0.001001,0.5,0.999999,1,1.25,0]){
  const window=Array.from({length:size},(_,i)=>i%3===0?-level:level);
  step(now+0.04,now+0.04,window);
  for(const dt of [0,0.179,0.8,0.000001,0.399999,0.02])step(now+dt,now+dt,new Array(size).fill(0));
 }
}
// Stalled audible clocks, backwards latency jitter, retained disconnected tail,
// ring wrap/eviction and context/source/data/running resets.
step(10,9.8,[1,0.5,-0.25,NaN,Infinity,-Infinity],{context:99,analyser:99,data:99,running:true});
for(let i=0;i<280;++i){const n=10+i/100;step(n,i<12?9.8:i%9===0?n-0.1:n,[i%13===0?1.1:0.15,0,-0.2,NaN,Infinity,-Infinity]);}
step(now+0.1,now+0.1,[0,0,0,0,0,0],{analyser:0,data:0});
step(now+0.9,now+0.9,[],{running:false});
step(now+0.1,now+0.1,[0.5],{analyser:101,data:101,running:true});
step(now+0.1,now+0.1,[0.75],{data:102});
step(now+0.1,now+0.1,[0.25],{analyser:102});
step(0,0,[0.1]);step(-1,Infinity,[0.2]);step(NaN,NaN,[0.3]);step(Infinity,-Infinity,[0.4]);
step(1,1,[1],{context:0});step(2,2,[0.4],{context:200});
// A full history with audible time older than every retained observation.
for(let i=0;i<260;++i)step(3+i/20,0,[i%2?0.25:0.75]);
const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);
const actual=run.stdout.trim().split('\n').map(line=>line.trim().split(/\s+/).map(Number));assert.equal(actual.length,expected.length);
let maximum=0;
for(let i=0;i<expected.length;++i)for(let j=0;j<7;++j){const error=Math.abs(actual[i][j]-expected[i][j]);maximum=Math.max(maximum,error);assert(Number.isFinite(actual[i][j])&&error<=1e-12*Math.max(1,Math.abs(expected[i][j])),JSON.stringify({step:i,field:j,actual:actual[i],expected:expected[i]}));}
console.log(JSON.stringify({scope:'Complete unchanged readMeter math on supplied Float32 analyser windows; PCM collection/IPC/UI/device latency excluded',observations:expected.length,maximumAbsoluteError:maximum,relativeTolerance:1e-12,parity:true},null,2));
