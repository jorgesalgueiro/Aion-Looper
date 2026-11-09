// SPDX-License-Identifier: AGPL-3.0-only
// Analytical AudioParam targets + UNCHANGED worklet; actual browser is a second gate.
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const reference=loadReference(),descriptors=reference.create(48000).Processor.parameterDescriptors,names=descriptors.map(p=>p.name),defaults=descriptors.map(p=>Math.fround(p.defaultValue));
const events=[
 [0,'p1Gain',12],[1,'p1Freq',22000],[63,'lsGain',-12],[64,'p1Gain',-24],[127,'p1Q',10],[128,'p2Gain',9],
 [129,'p1Gain',0],[255,'p1Gain',12],[256,'p1Gain',-12],[257,'p1Freq',40],[511,'p1Q',.1],[513,'hsGain',12],
 [1024,'p3Gain',6],[1024,'p3Freq',6200],[2048,'p1Gain',0],[3073,'hcFreq',22000],[4096,'p4Gain',-9],
 [6144,'p5Gain',4],[6144,'p6Gain',-4],[8192,'lcFreq',200]
];
let maximum=0,cases=0,frames=0;
for(const rate of [8000,22050,44100,48000,96000])for(const block of [64,128,37]){
 const count=12289,inputs=Array.from({length:2},(_,ch)=>Float32Array.from({length:count},(_,i)=>.13*Math.sin(i*(ch?.071:.113))+.04*Math.cos(i*(ch?.173:.291))));
 const commands=[`smooth ${rate} ${defaults.join(' ')}`],expected=[null];let position=0;
 function render(end){if(end<=position)return;commands.push(`render ${end-position} 2 ${block} `+Array.from({length:end-position},(_,j)=>`${inputs[0][position+j]} ${inputs[1][position+j]}`).join(' '));expected.push([position,end]);position=end;}
 for(const [frame,name,value]of events){render(frame);commands.push(`target ${names.indexOf(name)} ${Math.fround(value)}`);expected.push(null);}render(count);
 const values=defaults.map(v=>({start:v,target:v,at:0})),{processor}=reference.create(rate),outputs=[new Float32Array(count),new Float32Array(count)];let event=0;
 const at=(v,frame)=>v.start===v.target?v.target:v.target+(v.start-v.target)*Math.exp(-(frame-v.at)/(rate*.05));
 for(let offset=0;offset<count;offset+=128){
  // AudioParam.value is the last rendered sample. This was established by the
  // actual-browser failing-then-fixed retarget gate, not inferred from exp(t).
  while(event<events.length&&events[event][0]<=offset){const [frame,name,value]=events[event++],v=values[names.indexOf(name)];v.start=Math.fround(at(v,Math.max(v.at,frame-1)));v.target=Math.fround(value);v.at=frame;}
  const parameters=Object.fromEntries(names.map((name,index)=>{let value=Math.fround(at(values[index],offset));if(name.endsWith('Freq'))value=Math.min(value,rate*.5*(1-1e-6));return [name,Float32Array.of(value)];}));
  const n=Math.min(128,count-offset);processor.process([inputs.map(ch=>ch.subarray(offset,offset+n))],[outputs.map(ch=>ch.subarray(offset,offset+n))],parameters);
 }
 const run=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);
 for(let line=0;line<lines.length;++line){if(!expected[line]){assert.equal(lines[line],'ok');continue;}const [start,end]=expected[line],pcm=lines[line].split(' ').slice(1).map(Number);assert.equal(pcm.length,(end-start)*2);
  for(let i=start;i<end;++i)for(let ch=0;ch<2;++ch){const error=Math.abs(pcm[(i-start)*2+ch]-outputs[ch][i]);maximum=Math.max(maximum,error);assert(error<=1e-6,`Smoothing ${rate}/${block} frame ${i} ch${ch}: error ${error}`);}
 }
 cases++;frames+=count;
}
console.log(JSON.stringify({scope:'Analytical 50ms AudioParam target/cancel anchors into unchanged original worklet, 128-frame coefficient clock, sample-offset rapid targets; actual WebAudio checked separately',cases,stereoFrames:frames,maximumAbsoluteError:maximum,tolerance:1e-6}));
