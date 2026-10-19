// Execute the complete unchanged scheduleCountIn method. The original delegates
// triangle generation to WebAudio: compare native PCM to its band-limited Fourier
// definition (4e-5 absolute interpolation tolerance), not a mocked JS oscillator.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
const start=source.indexOf('    static scheduleCountIn('),end=source.indexOf('\n    }',start)+6;
assert(start>=0&&end>start);
let events=[],timers=[],state={};const master={style:{}};
const destination={},ctx={destination,currentTime:0,createOscillator(){const event={frequency:{value:0},type:'sine',connect(gain){assert(gain.isGain);},start(time){this.startTime=time;},stop(time){this.endTime=time;events.push(this);}};return event;},createGain(){return {isGain:true,gain:{value:0},connect(target){assert.equal(target,destination);}};}};
// Record the actual gain assignment too, without replacing scheduler logic.
ctx.createGain=()=>{const gain={value:0};return {isGain:true,gain,connect(target){assert.equal(target,destination);Object.defineProperty(gain,'value',{get(){return 0.5;},set(value){assert.equal(value,0.5);}});}};};
const context={get state(){return state;},document:{getElementById:()=>master},setTimeout(fn,delay){timers.push({fn,delay});return timers.length;}};vm.createContext(context);vm.runInContext('class Original {\n'+source.slice(start,end)+'\n};globalThis.schedule=delay=>Original.scheduleCountIn(delay);',context);
function oracle(now,target,bpm,num,sound,visual){events=[];timers=[];ctx.currentTime=now;state={audioContext:ctx,bpm,timeSig:{num},countIn:{audio:sound,visual}};context.state=state;context.schedule(target-now);for(const e of events){assert.equal(e.type,'triangle');assert([440,880].includes(e.frequency.value));assert(Math.abs(e.endTime-e.startTime-0.1)<1e-12);}return {events:events.map(e=>({start:e.startTime,end:e.endTime,freq:e.frequency.value})),visual:timers.slice().map(t=>{t.fn();return {start:now+t.delay/1000,color:master.style.borderColor==='#f00'?2:1};})};}
const harmonics=new Map();
function triangle(elapsed,freq,rate){const key=rate+':'+freq;let hs=harmonics.get(key);if(!hs){
 const size=rate<=24000?2048:rate<=88200?4096:16384,pitch=Math.fround(1+Math.fround(Math.fround(Math.log2(Math.fround(freq/(rate/size))))*3));
 const band=Math.floor(pitch),blend=pitch-band,high=Math.floor(size/2*2**(-band/3)),low=Math.floor(size/2*2**(-(band+1)/3));
 hs=[];let total=0;for(let h=1;h<size/2;h+=2){total+=1/(h*h);if(h<=high)hs.push([h,(h%4===1?1:-1)/(h*h)*(h<=low?1:1-blend)]);}hs.total=total;harmonics.set(key,hs);
 }let sum=0;const phase=elapsed*freq*2*Math.PI;for(const [h,gain] of hs)sum+=gain*Math.sin(phase*h);return 0.5*sum/hs.total;}
function pcm(events,frame,rate){const now=frame/rate;let sum=0;for(const e of events)if(now>=e.start&&now<e.end)sum+=triangle(now-e.start,e.freq,rate);return Math.fround(sum);}
function flash(now,plan){let latest=-1,color=0;for(const cue of plan.visual)if(now>=cue.start&&now<cue.start+0.15&&cue.start>latest){latest=cue.start;color=cue.color;}return color;}
const run=commands=>{const result=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:64*1024*1024});assert.equal(result.status,0,result.stderr);return result.stdout.trim().split(/\r?\n/);};
let cases=0,samples=0,maximum=0;
for(const rate of [8000,44100,48000,96000,384000])for(const spec of [
 {now:0,target:2,bpm:120,num:4,sound:true,visual:true},
 {now:0.013,target:2.1,bpm:999,num:32,sound:true,visual:true},
 {now:10.001,target:10.55,bpm:120,num:7,sound:true,visual:false},
 {now:0,target:0.55,bpm:120,num:4,sound:true,visual:true},
 {now:0,target:0.1,bpm:600,num:1,sound:true,visual:true},
 {now:0,target:1.1,bpm:137.3,num:7,sound:false,visual:true}
]){
 const {now,target,bpm,num,sound,visual}=spec,expected=oracle(now,target,bpm,num,sound,visual),first=Math.floor(Math.max(now,target-0.14)*rate),n=Math.ceil(0.3*rate);
 const line=run([`prepare ${rate}`,`schedule ${now} ${target} ${bpm} ${num} ${+sound} ${+visual}`,`kernel ${first} ${n}`])[0].split(' ').filter(Boolean).map(Number);assert.equal(line.length,n*2);
 for(let i=0;i<n;i++){const value=pcm(expected.events,first+i,rate),error=Math.abs(line[i*2]-value);maximum=Math.max(maximum,error);assert(error<=4e-5,`${rate}/${bpm} frame ${first+i} native ${line[i*2]} reference ${value} error ${error}`);assert.equal(line[i*2+1],flash((first+i)/rate,expected),'Original cue flash timing');}
 samples+=n;cases++;
}
// Connected Engine: arm using production LoopBank, actual stereo output and meter
// routing. Existing js_native_sync independently validates the grid/extractor.
for(const rate of [8000,48000])for(const block of [64,128])for(const spec of [
 {bpm:120,num:4,visual:false,sound:true},
 {bpm:999,num:32,visual:true,sound:true},
 {bpm:137.3,num:7,visual:true,sound:false},
 {bpm:240,num:4,visual:false,sound:true,arm:0.9}
]){
 const {bpm,num,visual,sound}=spec,arm=spec.arm??0.013,pre=Math.round(arm*rate),now=pre/rate,bar=60/bpm*num;
 let target=Math.ceil((now+0.03)/bar)*bar;if((visual||sound)&&target-now<bar*0.75)target+=bar;
 const expected=oracle(now,target,bpm,num,sound,visual),n=Math.ceil((target-now+0.2)*rate);
 const lines=run([`prepare ${rate}`,`configure 1 ${bpm} 1 ${num} 4 ${+visual} ${+sound}`,`render ${pre} ${block}`,'action 0 0',`render ${n} ${block}`]);
 let heard=false;
 for(const line of lines){const [frame,deadline,status,master,peak,color,count,...actual]=line.split(' ').map(Number);assert.equal(actual.length,count);assert.equal(master,0,'SOUND excludes MASTER while MASTER muted');if(frame>=pre&&status===8)assert.equal(deadline,Math.ceil(target*rate-1e-9),'Audio/visual lead-in matches original target');
  for(let i=0;i<count;i++){const value=frame+i>=pre?Math.max(-1,Math.min(1,pcm(expected.events,frame+i,rate))):0,error=Math.abs(actual[i]-value);maximum=Math.max(maximum,error);assert(error<=4e-5,`Connected ${rate}/${block}/${bpm} frame ${frame+i} error ${error}`);heard ||= Math.abs(actual[i])>0.01;}
  assert.equal(color,frame>=pre?flash((frame+count)/rate,expected):0,'Connected native flash');samples+=count;
 }
 assert.equal(heard,sound,'Actual output reflects original SOUND checkbox');cases++;
}
console.log(JSON.stringify({result:'PASS',cases,samples,maximumAbsoluteError:maximum,tolerance:4e-5,oracle:'unchanged scheduleCountIn + independently evaluated pitch-blended triangle; WebAudio implementation PCM is not bit-identical'}));
