// SPDX-License-Identifier: AGPL-3.0-only
const assert=require('node:assert/strict'),DriveDocument=require('./DriveDocument.cjs');
const page=new DriveDocument('main');
const created=(id,uniqueId,frameId='main',origin='https://juce.backend',isDefault=true)=>({method:'Runtime.executionContextCreated',params:{context:{id,uniqueId,origin,auxData:{frameId,isDefault}}}});
assert.equal(page.next(),undefined);
page.event(created(1,'old'));assert.equal(page.next(),'old');
// A reload acknowledgement may precede navigation. Even ready controls from
// the old document must not qualify, and no evaluate may be sent there.
assert.equal(page.next('old'),undefined);
for(const c of [created(2,'iframe','child'),created(3,'isolated','main','https://juce.backend',false),created(4,'blank','main','')]){
 page.event(c);assert.equal(page.next('old'),undefined);
}
page.event({method:'Runtime.executionContextsCleared'});assert.equal(page.next('old'),undefined);
// Numeric IDs may be reused after navigation; unique identity must advance.
page.event(created(1,'new'));assert.equal(page.next('old'),'new');
page.event({method:'Runtime.executionContextDestroyed',params:{executionContextId:1,executionContextUniqueId:'old'}});
assert.equal(page.next('old'),'new','late old-world destruction cannot retire its replacement');
page.event({method:'Runtime.executionContextDestroyed',params:{executionContextId:1,executionContextUniqueId:'new'}});
assert.equal(page.next('old'),undefined);
page.event(created(7,'third'));
page.event({method:'Runtime.executionContextDestroyed',params:{executionContextId:7}});assert.equal(page.next(),undefined);
assert.throws(()=>page.event(created(8,'')),/unique execution context/);
console.log('PASS drive document readiness: old ready page excluded, frame/world/origin filtering, recycled numeric IDs, late destruction and missing identity');
