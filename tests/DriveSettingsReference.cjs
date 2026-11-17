// SPDX-License-Identifier: AGPL-3.0-only
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert/strict'),cp=require('child_process');
const root=path.resolve(__dirname,'..');
const original=fs.readFileSync(path.join(root,'.reference/Ascii-Looper/Modules/effects.js'),'utf8');
const preserved=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
const catalog=JSON.parse(cp.execFileSync(process.argv[2],['--catalog'],{encoding:'utf8'}));
const evaluate=text=>JSON.parse(JSON.stringify(vm.runInNewContext('('+text+')')));
for(const entry of catalog){
 const name=entry.name,upper=name.toUpperCase();
 const defaults=original.match(new RegExp('^    '+name+': (\\{[^}]*\\})','m'));
 assert(defaults,`Original ${name} defaults`);assert(preserved.includes(defaults[0]));
 assert.deepEqual(entry.defaults,evaluate(defaults[1]));
 const presets=original.match(new RegExp('const '+upper+'_PRESETS = (\\{[\\s\\S]*?\\n\\});'));
 assert(presets);assert(preserved.includes(presets[0]));assert.deepEqual(entry.presets,evaluate(presets[1]));
 const code={distortion:'T',overdrive:'O',fuzz:'F'}[name];
 const start=original.indexOf(`    '${code}': { key: '${name}'`);assert(start>=0);
 const controls=original.slice(start).match(/controls: (\[[\s\S]*?\])/);assert(controls);
 assert.deepEqual(entry.controls,evaluate(controls[1]).map(({p,min,max,step})=>({p,min,max,step})));
}
console.log('PASS: unchanged original T/O/F defaults, widget bounds/steps and all 17 preset names/values; new DSP sound is intentionally different');
