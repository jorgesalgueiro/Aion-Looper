// SPDX-License-Identifier: AGPL-3.0-only
// Kernel only. Actual WebAudio smoothing/quantum/graph acceptance is independent.
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const reference=loadReference(),{Processor}=reference.create(48000),descriptors=Processor.parameterDescriptors;
const defaults=Object.fromEntries(descriptors.map(p=>[p.name,Math.fround(p.defaultValue)]));
const names=descriptors.map(p=>p.name),values=p=>names.map(name=>Math.fround(p[name])).join(' ');
const native=process.argv[2];assert(native,'Provide aion_eq_probe');
let cases=0,frames=0,maximum=0,tailCases=0;
function runCase(rate,block,label,initial,steps){
 const {processor}=reference.create(rate),commands=[`prepare ${rate} ${values(initial)}`],expected=['ok'];let clock=0;
 let current={...initial};
 for(const step of steps){
  if(step.reset){commands.push('reset');expected.push('ok');const fresh=reference.create(rate).processor;Object.assign(processor,fresh);}
  if(step.params){current={...current,...step.params};commands.push(`configure ${values(current)}`);expected.push('ok');}
  const count=step.count??512,channels=step.channels??2;
  const input=Array.from({length:channels},(_,ch)=>Float32Array.from({length:count},(_,i)=>{
   const n=clock+i;
   if(step.silence)return 0;
   if(step.impulse)return i===0?(ch===0?(step.amplitude??.5):-(step.amplitude??.5)*.5):0;
   // Deterministic signed tones + nonperiodic integer noise, independent channels.
   const random=((((n+1)*(ch?7919:104729))%65521)/32760.5-1)*.015;
   return .11*Math.sin(n*(ch?.271:.137))+.04*Math.cos(n*(ch?.071:.019))+random;
  }));
  const output=[new Float32Array(count),new Float32Array(count)];
  // Worklet calls correspond to explicit coefficient boundaries; subdividing
  // native rendering at 64/128/irregular sizes may never reset its state.
  // Original unmodified worklet with approved effective-frequency inputs. Keep
  // raw-defect reproduction below separate from this deliberate safety repair.
  const params=Object.fromEntries(names.map(name=>[name,Float32Array.of(name.endsWith('Freq')?Math.min(current[name],rate*.5*(1-1e-6)):current[name])]));
  assert.equal(processor.process([input],[output],params),true);
  commands.push(`render ${count} ${channels} ${block} `+Array.from({length:count},(_,i)=>input.map(ch=>ch[i]).join(' ')).join(' '));
  expected.push(output);clock+=count;
  if(step.silence&&output.some(ch=>ch.some(x=>x!==0)))tailCases++;
 }
 const run=spawnSync(native,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length,`${label} response count`);
 for(let line=0;line<lines.length;++line){
  if(typeof expected[line]==='string'){assert.equal(lines[line],expected[line],`${label} setting acceptance`);continue;}
  const [kind,...actual]=lines[line].split(' ');assert.equal(kind,'pcm');const output=expected[line];assert.equal(actual.length,output[0].length*2);
  for(let i=0;i<output[0].length;++i)for(let ch=0;ch<2;++ch){
   const expectedSample=output[ch][i],sample=Number(actual[i*2+ch]);assert(Number.isFinite(expectedSample)&&Number.isFinite(sample),`${label} finite PCM`);
   const error=Math.abs(sample-expectedSample);maximum=Math.max(maximum,error);
   assert(error<=1e-6,`${rate}/${block}/${label} frame ${i} channel ${ch}: ${sample} vs ${expectedSample} error ${error}`);
  }
  frames+=output[0].length;
 }
 cases++;
}
const patterns=[{impulse:true,count:129},{count:511},{silence:true,channels:0,count:1031},{channels:1,count:257},{silence:true,channels:0,count:4096}];
// Preserve the exact unit-impulse case that disproved an ideal-transfer DFT
// assertion: tiny-state flushing changes its spectrum, but original PCM must
// still match strictly. The actual curve uses steady-tone response acceptance.
runCase(8000,64,'unit impulse + full tiny-state flush tail',defaults,[{impulse:true,amplitude:1,count:65536,channels:1}]);
for(const rate of [44100,48000,96000,192000])for(const block of [64,128,37]){
 runCase(rate,block,'defaults',defaults,patterns);
 for(const [name,preset]of Object.entries(reference.presets))runCase(rate,block,`preset ${name}`,{...defaults,...preset},patterns);
 // Each of eight shelf/peak filters independently, both gain extremes, Q limits,
 // very low and highest in-range frequencies, with tail/reset and missing channel.
 for(const [freq,gain,q]of [['lsFreq','lsGain',null],...Array.from({length:6},(_,i)=>[`p${i+1}Freq`,`p${i+1}Gain`,`p${i+1}Q`]),['hsFreq','hsGain',null]]){
  for(const sign of [-1,1]){
   const p={...defaults,[freq]:sign<0?10:22000,[gain]:sign*24};if(q)p[q]=sign<0?.1:10;
   runCase(rate,block,`${gain} ${sign}`,p,[...patterns,{reset:true,impulse:true,count:257}]);
  }
 }
 // All bands active at once and rapid changes across the exact bypass boundary:
 // float32(0.01) is BELOW mathematical 0.01; do not use a float threshold.
 const hot={...defaults,lcFreq:500,hcFreq:17000,lsGain:12,hsGain:-12};
 for(let i=1;i<=6;i++)Object.assign(hot,{[`p${i}Gain`]:i%2?9:-9,[`p${i}Q`]:i%2?.1:10});
 runCase(rate,block,'all bands, rapid bypass/reset',hot,[
  {count:257},...[.010001,.01,-.009999,-.010001,0,24,0,-24,0,12].map(g=>({params:{p1Gain:g,lsGain:g,hsGain:-g},count:129})),
  {params:{p1Gain:0,p2Gain:0,p3Gain:0,p4Gain:0,p5Gain:0,p6Gain:0,lsGain:0,hsGain:0},silence:true,channels:0,count:513},
  {params:{p1Gain:24},count:17},{reset:true,count:257},...patterns
 ]);
}
// Low sample rates with explicitly safe settings, including all active filters.
for(const rate of [8000,16000,22050,32000])for(const block of [64,128,37]){
 runCase(rate,block,'Nyquist-limited defaults',defaults,patterns);
 for(const [name,preset]of Object.entries(reference.presets))runCase(rate,block,`Nyquist-limited preset ${name}`,{...defaults,...preset},patterns);
 const p={...defaults,hcFreq:rate*.4,lsGain:3,hsGain:-3,hsFreq:rate*.3};
 for(let i=1;i<=6;i++)Object.assign(p,{[`p${i}Freq`]:rate*(.02+i*.04),[`p${i}Gain`]:i%2?4:-4,[`p${i}Q`]:i%2?.1:10});
 runCase(rate,block,'safe low-rate settings',p,patterns);
}
// Preserve the original failing cases as evidence instead of silently clamping
// or discarding them. Product policy must be decided before low-rate integration.
const defect=[];
for(const rate of [22050,32000]){
 const {processor}=reference.create(rate),params=Object.fromEntries(names.map(name=>[name,Float32Array.of(defaults[name])])),input=[new Float32Array(128),new Float32Array(128)];
 input[0][0]=input[1][0]=.1;let first=-1;
 for(let quantum=0;quantum<64;++quantum){const output=[new Float32Array(128),new Float32Array(128)];processor.process([input],[output],params);
  for(let i=0;i<128;i++)if(first<0&&!Number.isFinite(output[0][i]))first=quantum*128+i;input[0].fill(0);input[1].fill(0);
 }
 assert.equal(first,rate===22050?224:167,'Retain unchanged low-rate instability reproduction');defect.push({rate,firstNonfiniteFrame:first});
}
assert(tailCases>0,'Oracle must exercise nonzero filter tails');
console.log(JSON.stringify({scope:'Unchanged original EQProcessor/SVF kernel only; all original presets, band boundaries, bypass/re-entry/reset, stereo/absent channels/tails; no graph or AudioParam automation claim',cases,stereoFrames:frames,nonzeroTailSegments:tailCases,maximumAbsoluteError:maximum,tolerance:1e-6,referenceDefects:defect}));
