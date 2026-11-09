// SPDX-License-Identifier: AGPL-3.0-only
// Real Engine capture/playback/control output against UNCHANGED EQProcessor,
// with its original AudioParam target equation established by the browser gate.
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const ref=loadReference(),descriptors=ref.create(48000).Processor.parameterDescriptors;
const run=spawnSync(process.argv[2],['--reference'],{encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(run.status,0,run.stderr);
const cases=run.stdout.split(/\r?\n/).filter(l=>l.startsWith('{')).map(l=>JSON.parse(l));assert.equal(cases.length,2);let maximum=0,frames=0;
for(const test of cases){
    const count=test.dry.length/2,{processor}=ref.create(48000),initial=descriptors.map(d=>Math.fround(ref.presets.Radio[d.name]??d.defaultValue));
    const values=initial.map(v=>({start:v,target:v,at:0}));
    const events=[{at:2048,patch:{p1Freq:900,p1Gain:-6}},{at:4096,patch:Object.fromEntries(descriptors.map(d=>[d.name,ref.presets.Telephone[d.name]??d.defaultValue]))}];let event=0,offset=0;
    const at=(v,t)=>v.target+(v.start-v.target)*Math.exp(-(t-v.at)/(48000*.05));
    while(offset<count){
        while(event<events.length&&events[event].at<=offset){const e=events[event++];for(const [name,value]of Object.entries(e.patch)){const v=values[descriptors.findIndex(d=>d.name===name)];v.start=Math.fround(at(v,Math.max(v.at,e.at-1)));v.target=Math.fround(value);v.at=e.at;}}
        const n=Math.min(128-(test.origin+offset)%128,count-offset);
        const input=[0,1].map(ch=>Float32Array.from({length:n},(_,i)=>test.dry[(offset+i)*2+ch])),output=[new Float32Array(n),new Float32Array(n)];
        const parameters=Object.fromEntries(descriptors.map((d,i)=>[d.name,Float32Array.of(at(values[i],offset))]));
        processor.process([input],[output],parameters);
        for(let i=0;i<n;++i){const age=offset+i,fade=age<960?1-age/960:age<1920?(age-960)/960:1;for(let ch=0;ch<2;++ch){const error=Math.abs(test.wet[age*2+ch]-Math.fround(output[ch][i]*fade));maximum=Math.max(error,maximum);assert(error<=1e-6,`${test.block}/${age}/${ch}: ${error}`);}}
        offset+=n;
    }frames+=count;
}
console.log(`PASS connected Engine -> unchanged EQ worklet: ${cases.length} stereo 64/128 cases, ${frames} frames, XY/preset smoothing/global quantum/fade peak ${maximum}, limit 1e-6`);
