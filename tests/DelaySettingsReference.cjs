// SPDX-License-Identifier: AGPL-3.0-only
// Original controls/presets only. The new delay model is intentionally not a
// claim of WebAudio feedback-filter or transition-sound identity.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),probe=process.argv[2];assert(probe);
const read=p=>fs.readFileSync(p,'utf8').replace(/\r\n/g,'\n');
const effects=read(path.join(root,'resources/ui/Modules/effects.js'));
const extract=s=>({
 defaults:s.match(/    delay: (\{[\s\S]*?\n    \}),/)[1],
 presets:s.match(/const DELAY_PRESETS = (\{[\s\S]*?\n\});/)[1],
 markup:s.slice(s.indexOf("    'D': { key: 'delay'"),s.indexOf("    'T': { key: 'distortion'"))
});
const source=extract(effects),reference=path.join(root,'.reference/Ascii-Looper/Modules/effects.js');
assert(fs.existsSync(reference),'Independent original reference required');
assert.deepEqual(source,extract(read(reference)),'Preserved settings must match independent reference');
const defaults=vm.runInNewContext('('+source.defaults+')'),presets=vm.runInNewContext('('+source.presets+')');
const markup=vm.runInNewContext('({'+source.markup+'}).D');
const result=spawnSync(probe,['catalog'],{encoding:'utf8'});assert.equal(result.status,0,result.stderr);
const native=JSON.parse(result.stdout),names=native.controls.map(c=>c.name);
assert.deepEqual(names,['time','repetitions','damp','mix','panSpeed','panDepth','sync']);
for(const c of native.controls){assert.equal(c.initial,Number(defaults[c.name]));
 if(c.name==='sync'){assert.equal(c.min,0);assert.equal(c.max,1);continue;}
 const widget=markup.controls.find(w=>w.p===c.name);assert(widget,c.name);
 assert.equal(c.min,widget.min);assert.equal(c.max,widget.max);assert.equal(c.step,widget.step);
}
assert.deepEqual(native.presets.map(p=>p.name),Object.keys(presets));
for(const p of native.presets)assert.deepEqual(p.values,names.map(n=>Number(presets[p.name][n])));
console.log('PASS unchanged original D defaults, six sliders, sync flag and nine named presets; not audio-model parity');
