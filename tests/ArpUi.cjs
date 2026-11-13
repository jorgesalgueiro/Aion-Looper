// SPDX-License-Identifier: AGPL-3.0-only
// Real editor, CDP input and synthetic production audio. No physical-device claim.
const assert=require('node:assert/strict');const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 assert(process.env.AION_MOUSE_TEST_RUN,'Use tools/TestArpUi.ps1');let target;
 for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(target)break;await wait(100);}
 assert(target);socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
 let serial=0;const replies=new Map(),errors=[];
 socket.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);replies.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{replies.delete(id);reject(Error(method+' timeout'));},12000);replies.set(id,r=>{clearTimeout(timer);replies.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
 const until=async(predicate,label)=>{let s;for(let n=0;n<160;++n){s=await snapshot();if(predicate(s))return s;await wait(50);}assert.fail(label+': '+JSON.stringify({arp:s?.tracks[0]?.arp,pending:s?.tracks[0]?.arpPending,errors}));};
 const rect=async selector=>{await evaluate(`document.querySelector(${JSON.stringify(selector)}).scrollIntoView({block:'center',behavior:'instant'})`);await wait(150);return evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,disabled:e.disabled,hit:e.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2))};})()`);};
 const click=async selector=>{const r=await rect(selector);assert(!r.disabled&&r.hit,selector+' blocked');const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x,y:r.y,button:'left',clickCount:1});await wait(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x,y:r.y,button:'left',clickCount:1});await Promise.all([down,up]);};
 await call('Runtime.enable');
 for(let n=0;n<100;++n){if(await evaluate(`!!document.querySelector('#native-arp-enabled')&&typeof AionNative?.request==='function'`))break;await wait(100);}
 await until(s=>s.audioReady&&s.inputAvailable,'audio active');
 for(const [id,value] of [['masterGain',1],['inputMonitor',0],['syncMode',0],['autoPlayAfterRecord',0],['metronomeVolume',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
 await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','record');await wait(300);await click('#loop-action-0');await until(s=>s.tracks[0].state==='stopped','capture');
 await click('#loop-action-0');await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.01,'nonzero playback');
 await click('#loop-0-arpDelay');await until(s=>s.tracks[0].arp.enabled&&s.tracks[0].arpReady,'async checkbox enable');
 await click('#loop-0-arpDelay + span');assert(await evaluate(`document.querySelector('#native-arp-title').textContent.includes('Loop 1')`));
 await evaluate(`document.querySelector('#sel_arpDelay').value='sync-trip';document.querySelector('#sel_arpDelay').dispatchEvent(new Event('change',{bubbles:true}))`);
 await until(s=>s.tracks[0].arp.preset==='sync-trip'&&s.tracks[0].arpReady,'original preset');
 await click('#arpDelayStay');await until(s=>s.tracks[0].arp.values.stay===1&&s.tracks[0].arpReady,'STAY click');
 await click('#arpDelayHumanize');await call('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39});
 const desired=await evaluate(`Number(document.querySelector('#arpDelayHumanize').value)`);
 let s=await until(s=>s.tracks[0].arp.values.humanize===desired&&!s.tracks[0].arp.preset&&s.tracks[0].arpReady,'last manual slider value clears preset');const before=s.tracks[0].arp;
 // User routing, not a fixed Q/C/A chain. Removing A retains its settings.
 let reply=await rpc({operation:'signalChain',action:'set',track:0,order:'CQ',generation:s.generation,expiresAt:Date.now()+3000});assert(reply.ok);await until(s=>s.tracks[0].signalChain==='CQ','A omitted');assert.deepEqual((await snapshot()).tracks[0].arp,before);
 s=await snapshot();reply=await rpc({operation:'signalChain',action:'set',track:0,order:'ACQ',generation:s.generation,expiresAt:Date.now()+3000});assert(reply.ok);await until(s=>s.tracks[0].signalChain==='ACQ'&&s.outputPeak>.001,'A reinserted with continuing PCM');
 await call('Page.reload');await wait(1000);await until(s=>s.tracks[0].arp.enabled&&s.tracks[0].arpReady,'editor reopen initial sync');
 assert(await evaluate(`document.querySelector('#loop-0-arpDelay').checked`));
 await click('#loop-0-arpDelay');await until(s=>!s.tracks[0].arp.enabled&&s.tracks[0].arpReady,'bypass exact receipt');
 assert.deepEqual(errors,[]);console.log('PASS Arp actual WebView2: record/play PCM, enable/preset/STAY/slider, arbitrary chain omission/reinsert, reload, bypass / '+process.argv[2]);socket.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
