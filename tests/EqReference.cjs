// Test-only loader: execute the unchanged first worklet declarations, never the
// product bootstrap. The preserved resource and independent reference stay intact.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
function loadReference(){
 const scripts=[],context={document:{createElement:()=>({}),head:{appendChild:element=>scripts.push(element.textContent)}}};vm.createContext(context);
 for(const file of ['audioEngine.js','effects.js']){
  const source=fs.readFileSync(path.join(root,'resources/ui/Modules',file),'utf8'),end=source.indexOf('})();');assert(end>0);
  vm.runInContext(source.slice(0,end+5),context);
 }
 assert.equal(scripts.length,2);assert(scripts[0].includes('class SVF'));assert(scripts[1].includes('class EQProcessor'));
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
 const start=source.indexOf('const EQ_PRESETS = {'),end=source.indexOf('\n};',start);assert(start>0&&end>start);
 const presets=vm.runInNewContext(source.slice(start,end+3)+'\nEQ_PRESETS');
 return {source:scripts.join('\n'),presets,
  create(rate){let Processor;const worklet={sampleRate:rate,AudioWorkletProcessor:class{},registerProcessor:(name,p)=>{assert.equal(name,'eq-processor');Processor=p;}};
   vm.createContext(worklet);vm.runInContext(scripts.join('\n'),worklet);return {Processor,processor:new Processor()};}
 };
}
module.exports={loadReference};
