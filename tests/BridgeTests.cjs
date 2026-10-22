const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../resources/native/bridge.js'),'utf8');
const watchdog=setTimeout(()=>{console.error('Bridge lifecycle test did not finish');process.exitCode=1;},5000);
const document=(session,crypto={randomUUID:()=>session})=>{
 const outgoing=[],events=new Map(),timers=new Map(),page=new Map();let serial=0;
 const backend={addEventListener:(type,fn)=>events.set(type,fn),emitEvent:(type,value)=>outgoing.push(value)};
 const context={window:{__JUCE__:{backend},crypto,addEventListener:(type,fn)=>page.set(type,fn)},document:{addEventListener(){},querySelectorAll:()=>[]},setTimeout:(fn,ms)=>{assert.equal(ms,5000);const id=++serial;timers.set(id,()=>{timers.delete(id);fn();});return id;},clearTimeout:id=>timers.delete(id)};
 vm.runInNewContext(source,context);
 return {api:context.window.AionNative,backend,outgoing,timers,close:()=>page.get('pagehide')(),complete:message=>events.get('__juce__complete')(message)};
};
(async()=>{
 const failed=document('transport-failure'),send=failed.backend.emitEvent;
 failed.backend.emitEvent=()=>{throw Error('transport unavailable');};
 await assert.rejects(failed.api.request({operation:'snapshot'}),/transport unavailable/);
 assert.equal(failed.timers.size,0,'Failed native send must immediately release its timeout and pending slot');
 // More failures than the entire queue must not block a later valid request.
 for(let n=0;n<256;++n)await assert.rejects(failed.api.request({operation:'snapshot'}),/transport unavailable/);
 assert.equal(failed.timers.size,0);
 failed.backend.emitEvent=(type,value)=>{JSON.stringify(value);send(type,value);};
 const circular={operation:'snapshot'};circular.self=circular;await assert.rejects(failed.api.request(circular),/circular/i);assert.equal(failed.timers.size,0,'Serialization failure releases its slot');
 const getter={operation:'hello',get bad(){throw Error('payload getter failed');}};await assert.rejects(failed.api.request(getter),/payload getter failed/);assert.equal(failed.timers.size,0);
 failed.backend.emitEvent=send;
 const recovered=[];for(let n=0;n<128;++n)recovered.push(failed.api.request({operation:'hello'}));assert.equal(failed.timers.size,128,'All 128 slots remain available after failed sends');
 for(const request of failed.outgoing)failed.complete({promiseId:request.resultId,result:{ok:true,_aionSession:request.params[0]._aionSession}});
 assert((await Promise.all(recovered)).every(reply=>reply.ok));assert.equal(failed.timers.size,0);
 console.log('PASS: failed transport/serialization/getter cleanup and full pending-capacity recovery without waiting for timeout');
 const old=document('old-document'),current=document('current-document');
 const before=old.api.request({operation:'snapshot'}).catch(error=>error.message),stale=old.outgoing[0];old.close();assert.equal(await before,'Editor closed');assert.equal(old.timers.size,0);
 let value;const after=current.api.request({operation:'snapshot'}).then(reply=>value=reply),active=current.outgoing[0];assert.equal(stale.resultId,active.resultId,'Exercise actual ID reuse after reload');
 current.complete({promiseId:active.resultId,result:{ok:true,value:'old',_aionSession:stale.params[0]._aionSession}});await Promise.resolve();assert.equal(value,undefined,'Old document completion must never resolve a new document request');assert.equal(current.timers.size,1,'Stale reply cannot remove the new timeout');
 current.complete({promiseId:active.resultId,result:{ok:true}});current.complete({promiseId:active.resultId,result:null});await Promise.resolve();assert.equal(value,undefined,'Missing/malformed envelope cannot resolve current request');
 current.complete({promiseId:active.resultId,result:{ok:true,value:'current',_aionSession:active.params[0]._aionSession}});await after;assert.equal(value.value,'current');assert.equal(current.timers.size,0);assert(!('_aionSession' in value),'Correlation envelope is not exposed to UI callers');
 // Same-document responses can arrive out of order; unknown/duplicate IDs do nothing.
 const a=current.api.request(Object.freeze({operation:'hello',_aionSession:'caller-cannot-override'})),b=current.api.request({operation:'not-supported'}),first=current.outgoing.at(-2),second=current.outgoing.at(-1);
 assert.equal(first.params[0]._aionSession,'current-document');
 current.complete({promiseId:99999,result:{_aionSession:'current-document'}});current.complete({promiseId:second.resultId,result:{ok:false,error:'Unknown operation',_aionSession:'current-document'}});current.complete({promiseId:first.resultId,result:{ok:true,_aionSession:'current-document'}});assert.equal((await a).ok,true);assert.equal((await b).error,'Unknown operation');assert.equal(current.timers.size,0);
 current.complete({promiseId:first.resultId,result:{ok:false,_aionSession:'current-document'}});assert.equal(current.timers.size,0);
 for(const payload of [null,undefined,1,'snapshot',[]])await assert.rejects(current.api.request(payload),/Expected one request object/);
 // A stale completion cannot bypass the same 128-request bound or five-second deadline.
 const bounded=document('bounded-document'),waiting=[];for(let n=0;n<128;++n)waiting.push(bounded.api.request({operation:'snapshot'}).catch(error=>error.message));await assert.rejects(bounded.api.request({operation:'snapshot'}),/queue is full/);
 const timed=bounded.outgoing[0];bounded.complete({promiseId:timed.resultId,result:{ok:true,_aionSession:'obsolete-document'}});assert.equal(bounded.timers.size,128);bounded.timers.values().next().value();assert.equal(await waiting[0],'Native request timed out');
 const freed=bounded.api.request({operation:'hello'}),freedPacket=bounded.outgoing.at(-1);bounded.complete({promiseId:freedPacket.resultId,result:{ok:true,_aionSession:'bounded-document'}});assert.equal((await freed).ok,true,'Timeout restores one queue slot');
 bounded.close();assert.equal(bounded.timers.size,0);assert((await Promise.all(waiting)).slice(1).every(message=>message==='Editor closed'));
 const retry=current.api.request({operation:'snapshot'}),last=current.outgoing.at(-1);current.complete({promiseId:last.resultId,result:{ok:true,_aionSession:'current-document'}});assert.equal((await retry).ok,true);
 // Older WebKit paths keep correlation when randomUUID is unavailable.
 for(const crypto of [{getRandomValues:values=>{values.fill(0x1234abcd);return values;}},{}]){
  const fallback=document('fallback',crypto),first=fallback.api.request({operation:'hello'}),packet=fallback.outgoing[0],token=packet.params[0]._aionSession;assert.equal(typeof token,'string');assert(token.length>0&&token.length<=128);fallback.complete({promiseId:packet.resultId,result:{ok:true,_aionSession:token}});assert.equal((await first).ok,true);
  const second=fallback.api.request({operation:'hello'}),next=fallback.outgoing[1];assert.equal(next.params[0]._aionSession,token,'Document token stays stable across requests');fallback.complete({promiseId:next.resultId,result:{ok:true,_aionSession:token}});await second;
 }
 console.log('PASS: reused request ID after reload rejects obsolete completion and accepts current native response');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>clearTimeout(watchdog));
