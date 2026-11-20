// SPDX-License-Identifier: AGPL-3.0-only
// Test-owned CDP document identity. Never accept an old page's ready controls.
class DriveDocument {
 constructor(frame){this.frame=frame;this.current=null;}
 event(message){
  const p=message.params;
  if(message.method==='Runtime.executionContextsCleared')this.current=null;
  if(message.method==='Runtime.executionContextDestroyed'&&this.current&&
     (p.executionContextUniqueId? p.executionContextUniqueId===this.current.uniqueId:p.executionContextId===this.current.id))this.current=null;
  if(message.method==='Runtime.executionContextCreated'){
   const c=p.context;
   if(c.auxData?.frameId===this.frame&&c.auxData.isDefault&&c.origin==='https://juce.backend'){
    if(typeof c.uniqueId!=='string'||!c.uniqueId)throw Error('CDP unique execution context identity is required');
    this.current={id:c.id,uniqueId:c.uniqueId};
   }
  }
 }
 next(previous){return this.current?.uniqueId!==previous?this.current?.uniqueId:undefined;}
}
module.exports=DriveDocument;
