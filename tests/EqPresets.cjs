// SPDX-License-Identifier: AGPL-3.0-only
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const ref=loadReference(),names=ref.create(48000).Processor.parameterDescriptors,run=spawnSync(process.argv[2],[],{input:'presets\n',encoding:'utf8'});
assert.equal(run.status,0,run.stderr);const presets=JSON.parse(run.stdout);assert.equal(presets.length,10);
for(const [i,[name,values]]of Object.entries(ref.presets).entries()){
    assert.equal(presets[i].name,name);
    names.forEach((p,n)=>assert(Math.abs(presets[i].values[n]-Math.fround(values[p.name]??p.defaultValue))<=1e-6,`${name}/${p.name}`));
}
console.log('PASS ten native preset names and all 240 Float32 settings against unchanged EQ_PRESETS');
