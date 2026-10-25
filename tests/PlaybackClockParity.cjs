// Complete unchanged playbackTime getter composed with complete readMeter.
// Output-clock and analyser IO are supplied snapshots, not hardware timing proof.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),state={audioContext:null},contexts=new Map(),meter={},data=new Float32Array(2048);
let level=0;
const analyser={getFloatTimeDomainData(array){const pattern=[1,-1,.5,-.5];for(let i=0;i<array.length;++i)array[i]=pattern[i%4]*Math.fround(level);}};
const source=fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8');
function method(signature){const start=source.indexOf('    static '+signature),end=source.indexOf('\n    }',start)+6;assert(start>=0&&end>start);return source.slice(start,end);}
const ctx={state};vm.createContext(ctx);vm.runInContext(`class AudioEngine{${method('get currentTime()')}${method('get playbackTime()')}${method('readMeter(')}}`,ctx);
vm.runInContext('globalThis.observe=(analyser,data,meter)=>{const time=AudioEngine.playbackTime;AudioEngine.readMeter(analyser,data,meter);return time;};',ctx);
const expected=[],commands=[],settings={context:1,running:1,now:10,base:.1,out:.2,mode:1,stamp:9,perf:1000,wall:1025,level:1.25};
const token=v=>Number.isNaN(v)?'nan':v===Infinity?'inf':v===-Infinity?'-inf':String(v);
function step(changes={}){
 Object.assign(settings,changes);const s=settings;
 if(s.context&&!contexts.has(s.context))contexts.set(s.context,{});
 const audio=s.context?contexts.get(s.context):null;state.audioContext=audio;
 if(audio){
  audio.currentTime=s.now;audio.baseLatency=s.base;audio.outputLatency=s.out;audio.state=s.running?'running':'suspended';
  delete audio.getOutputTimestamp;
  if(s.mode!==0)audio.getOutputTimestamp=()=>{if(s.mode===2)throw Error('device timestamp unavailable');if(s.mode===3)return null;if(s.mode===6)return undefined;return {contextTime:s.stamp,performanceTime:s.perf};};
 }
 ctx.performance=s.mode===4?undefined:{now(){if(s.mode===5)throw Error('wall clock unavailable');return s.wall;}};
 level=s.level;const time=ctx.observe(analyser,data,meter);
 expected.push([time,meter.rms,meter.peak,meter.linearPeak,meter.peakDb,meter.rmsPercent,meter.peakPercent,Number(meter.clipped)]);
 commands.push([s.context,s.running,s.now,s.base,s.out,s.mode,s.stamp,s.perf,s.wall,s.level].map(token).join(' '));
}
// Timestamp precedence, extrapolation in milliseconds, caps, fallback and all IO
// unavailable paths (including original try/catch). Never double-subtract latency.
for(const mode of [0,1,2,3,4,5,6])for(const wall of [-10000,0,1000,1025,3000,NaN,Infinity,-Infinity])step({mode,wall});
for(const field of ['now','base','out','stamp','perf'])for(const value of [-Infinity,-1,-0,0,Number.MIN_VALUE,.001,1,10,Number.MAX_VALUE,Infinity,NaN]){
 step({context:1,running:1,now:10,base:.1,out:.2,mode:1,stamp:9,perf:1000,wall:1025,[field]:value});
 step({mode:0});
}
// Finite operands can still overflow during timestamp interpolation.
step({mode:1,now:10,stamp:Number.MAX_VALUE,perf:1,wall:Number.MAX_VALUE,base:.1,out:.2});
step({stamp:1,perf:Number.MAX_VALUE,wall:-Number.MAX_VALUE});
step({mode:0,base:Number.MAX_VALUE,out:Number.MAX_VALUE});
// Delayed selection, meter holds, clip timing, context replacement, suspension,
// jitter and history wrap all use the actual getter, not a hand-built clock.
step({context:2,mode:0,base:.125,out:.125,now:1,level:1.25});
step({now:1.25,level:0});assert.equal(meter.peak,1.25);assert.equal(meter.clipped,true);
for(let i=0;i<300;++i){const now=2+i/50;step({now,mode:i%4===0?0:1,stamp:now-.15,perf:1000,wall:i%9===0?980:1025,level:i%17===0?1.1:.1});}
step({context:0,now:NaN});step({context:3,now:0,stamp:0,perf:0});
step({now:1,running:0});step({now:1.1,running:1});step({now:.5});
for(let i=0;i<140;++i)step({now:10+i/50,stamp:.25,perf:1000,wall:1000,level:i%2?.2:.8});
const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);
const actual=run.stdout.trim().split('\n').map(line=>line.trim().split(/\s+/).map(Number));assert.equal(actual.length,expected.length);let maximum=0;
for(let i=0;i<expected.length;++i){assert.equal(actual[i].length,8);for(let j=0;j<8;++j){const error=Math.abs(actual[i][j]-expected[i][j]);maximum=Math.max(maximum,error);assert(Number.isFinite(actual[i][j])&&error<=1e-12*Math.max(1,Math.abs(expected[i][j])),JSON.stringify({step:i,input:commands[i],field:j,actual:actual[i],expected:expected[i]}));}}
console.log(JSON.stringify({parity:true,observations:expected.length,maximumAbsoluteError:maximum,relativeTolerance:1e-12,scope:'Complete unchanged playbackTime getter plus complete readMeter on supplied Float32 windows. Timestamp/latency IO is stubbed; native device/host clock acquisition, transport, graph binding and renderer are not certified.'},null,2));
