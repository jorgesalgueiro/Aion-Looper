// SPDX-License-Identifier: AGPL-3.0-only
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const ref=loadReference(),names=ref.create(48000).Processor.parameterDescriptors,run=spawnSync(process.argv[2],[],{input:'presets\n',encoding:'utf8'});
assert.equal(run.status,0,run.stderr);const presets=JSON.parse(run.stdout);assert.equal(presets.length,10);
for(const [i,[name,values]]of Object.entries(ref.presets).entries()){
    assert.equal(presets[i].name,name);
    names.forEach((p,n)=>assert(Math.abs(presets[i].values[n]-Math.fround(values[p.name]??p.defaultValue))<=1e-6,`${name}/${p.name}`));
}
console.log('PASS ten native preset names and all 240 Float32 settings against unchanged EQ_PRESETS');
// Execute the unchanged original cancellation method, not a hand-written model.
// Browser IO is inert; the observable EQ configuration must survive CANCEL.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.join(__dirname,'../resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
const start=source.indexOf('    static stopRecording('),end=source.indexOf('\n    }',start)+6;assert(start>0&&end>start);const method=source.slice(start,end);
let cancellations=0;
for(const force of [false,true])for(const [preset,params]of [['',{}],...Object.entries(ref.presets)]){
    const loop={id:0,state:'armed',graph:{},effects:{eq:true},params:{eq:{...params}},activeFxPresets:{eq:preset}};
    const before=JSON.stringify([loop.effects,loop.params,loop.activeFxPresets]);
    const state={loops:[loop],isRecording:true,isFinishingRecording:false,recordingLoopId:0,syncMode:'static',recordingStartTimeout:1,recordingTimeout:2,loopRecorder:{context:{},disconnect(){}},liveRecordAnalyser:{disconnect(){}}};
    vm.runInNewContext(`class Original{${method}};Original.stopRecording(${force});`,{state,clearTimeout(){},InputManager:{getRecordingNode:()=>({disconnect(){}})},UIManager:{updateLoop(){},updateStatus(){}},Log:{debug(){}}});
    assert.equal(loop.state,'empty');assert.equal(state.isRecording,false);assert.equal(state.recordingLoopId,-1);assert.equal(loop.graph,null);
    assert.equal(JSON.stringify([loop.effects,loop.params,loop.activeFxPresets]),before,'Original CANCEL preserves EQ fields');++cancellations;
}
console.log(`PASS ${cancellations} unchanged original stopRecording CANCEL EQ-preservation cases`);
