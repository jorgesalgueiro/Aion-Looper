const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
assert(/loopAnalyser\.fftSize\s*=\s*2048/.test(source),'Unchanged original loop analyser window size');
let cases=0,samples=0;
for(const channels of [1,2])for(const block of [64,128])for(const frames of [0,1,17,64,128,2047,2048,2176,8193]){
 const rows=Array.from({length:frames},(_,i)=>Array.from({length:channels},(_,c)=>Math.fround(Math.sin(i*0.031+c)*0.75)));
 const run=spawnSync(process.argv[2],[],{input:`${channels} ${frames} ${block}\n`+rows.map(v=>v.join(' ')).join('\n')+'\n',encoding:'utf8'});assert.equal(run.status,0,run.stderr);
 const actual=run.stdout.trim().split(/\s+/).map(Number);assert.equal(actual.length,2048);
 for(let i=0;i<2048;++i){const at=frames-2048+i,row=at<0?null:rows[at];const expected=!row?0:channels===1?row[0]:Math.fround(Math.fround(row[0]*0.5)+Math.fround(row[1]*0.5));assert.equal(actual[i],expected,JSON.stringify({channels,frames,block,i}));++samples;}
 ++cases;
}
console.log(JSON.stringify({scope:'Original 2048-loop-analyser size; exact native chronological zero-prefixed rolling mono/stereo PCM. Actual browser downmix is a separate gate.',cases,comparedSamples:samples,parity:true},null,2));
