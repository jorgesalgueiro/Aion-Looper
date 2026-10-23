// Run only against aion_mouse_host, never against a hardware Standalone session.
// No snapshot fixtures: all PCM, state transitions and acknowledgements are real.
const assert = require('node:assert/strict');
const delay = ms => new Promise(r => setTimeout(r, ms));
let socket;
(async () => {
    let target;
    for (let n=0;n<100;++n) {
        try { target=(await (await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend')); } catch {}
        if(target)break;await delay(150);
    }
    assert(target,'Start the synthetic mouse host on CDP port 9235');
    const ws=socket=new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
    let seq=0,acceptClear=false;const pending=new Map(),runtimeErrors=[],clearDialogs=[];
    ws.addEventListener('message',e=>{const reply=JSON.parse(e.data);if(reply.method==='Runtime.exceptionThrown')runtimeErrors.push(reply.params.exceptionDetails);if(reply.method==='Page.javascriptDialogOpening'){clearDialogs.push(reply.params.message);call('Page.handleJavaScriptDialog',{accept:acceptClear}).catch(error=>runtimeErrors.push(error.message));}pending.get(reply.id)?.(reply);});
    const call=(method,params={})=>new Promise((resolve,reject)=>{
        const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout: '+method));},10000);
        pending.set(id,reply=>{clearTimeout(timer);pending.delete(id);reply.error?reject(Error(JSON.stringify(reply.error))):resolve(reply.result);});
        ws.send(JSON.stringify({id,method,params}));
    });
    const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
    const snapshot=()=>evaluate(`AionNative.request({operation:'snapshot'})`);
    const pcmDirectory=require('node:path').resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]||64}`);
    const readPcm=()=>{try{return JSON.parse(require('node:fs').readFileSync(require('node:path').join(pcmDirectory,'pcm-evidence.json'),'utf8'));}catch{return null;}};
    const testRun=process.env.AION_MOUSE_TEST_RUN;assert(testRun,'Run through tools/TestMouseWorkflow.ps1 for owned-host evidence');
    const readNavigation=()=>{let value;try{value=JSON.parse(require('node:fs').readFileSync(require('node:path').join(pcmDirectory,'navigation-evidence.json'),'utf8'));}catch{return null;}assert.equal(value.testRun,testRun,'Navigation evidence belongs to this test host');return value;};
    const waitNavigation=async(predicate,label)=>{const end=Date.now()+7000;let evidence;while(Date.now()<end){evidence=readNavigation();if(evidence&&predicate(evidence))return evidence;await delay(10);}assert.fail(label+': '+JSON.stringify(evidence));};
    const waitPcm=async(predicate,label)=>{const end=Date.now()+7000;let evidence;while(Date.now()<end){evidence=readPcm();if(evidence&&predicate(evidence))return evidence;await delay(5);}assert.fail(label+': '+JSON.stringify(evidence));};
    const wait=async (predicate,label,timeout=7000)=>{const end=Date.now()+timeout;let value;while(Date.now()<end){value=await snapshot();if(predicate(value)){await delay(100);return value;}await delay(35);}assert.fail(label+': '+JSON.stringify(value));};
    const state=(wanted,id=0)=>wait(s=>s.tracks[id].state===wanted,`Loop ${id+1} -> ${wanted}`);
    const click=async (selector,{outside=false,hold=20}={})=>{
        const rect=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(!e)throw Error('Missing target');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,disabled:!!e.disabled,hit:e.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2))};})()`);
        assert(!rect.disabled&&rect.hit,'Mouse target blocked: '+selector+' '+JSON.stringify(rect));
        // Queue ordered input before waiting for CDP acknowledgements: response
        // latency must not turn a requested short click into a >=600ms hold.
        const input=[call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1})];
        await delay(hold);
        if(outside)input.push(call('Input.dispatchMouseEvent',{type:'mouseMoved',x:1190,y:790,button:'left',buttons:1}));
        input.push(call('Input.dispatchMouseEvent',{type:'mouseReleased',x:outside?1190:rect.x,y:outside?790:rect.y,button:'left',clickCount:1}));
        await Promise.all(input);
    };
    const pauseDispatch=()=>evaluate(`window.savedHoldPump=AionCommandQueue.prototype.pump;window.pauseHoldQueue=true;AionCommandQueue.prototype.pump=function(){window.pausedHoldQueue=this;if(window.pauseHoldQueue)return;return window.savedHoldPump.call(this);};`);
    const resumeDispatch=()=>evaluate(`(async()=>{const queue=window.pausedHoldQueue,pump=window.savedHoldPump;AionCommandQueue.prototype.pump=pump;delete window.pauseHoldQueue;delete window.pausedHoldQueue;delete window.savedHoldPump;await pump.call(queue);})()`);
    const parameter=async (selector,value)=>{
        await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});e.value=${JSON.stringify(value)};e.dispatchEvent(new Event('change',{bubbles:true}));})()`);
    };
    const mode=async value=>{await click(`label:has(input[name=syncMode][value=${value}])`);await wait(s=>s.syncMode===['off','static','dynamic'].indexOf(value),'sync mode '+value);};
    const exactlyOne=async (gesture,label,paired=false)=>{const before=await snapshot();await gesture();const after=await wait(s=>s.lastRequestId>before.lastRequestId,label);assert.equal(after.lastRequestId,before.lastRequestId+1,label);assert(!after.rejected,label+' rejected');if(!paired){await delay(160);assert.equal((await snapshot()).lastRequestId,after.lastRequestId,label+' duplicated');}return after;};
    await call('Runtime.enable');await call('Page.enable');
    await call('Emulation.setFocusEmulationEnabled',{enabled:true});
    for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
    await wait(s=>s.audioReady&&s.inputAvailable&&s.inputPeak>0.1,'Synthetic native input active');
    await evaluate(`window.mouseErrors=[];window.addEventListener('error',e=>window.mouseErrors.push(e.message));document.documentElement.style.scrollBehavior='auto'`);
    // Real trusted wheel events, not DOM dispatchEvent: original capture guard.
    await evaluate(`window.wheelEvidence=[];window.addEventListener('wheel',e=>window.wheelEvidence?.push({id:e.target.id,shift:e.shiftKey,prevented:e.defaultPrevented,trusted:e.isTrusted,focused:document.activeElement===e.target}),{capture:true,passive:false});`);
    for(const selector of ['#native-master','#loop-vol-slider-0','#bpmInput','#loop-name-0']){
        // Track-name ID follows the original markup; obtain it semantically below.
        const actual=selector==='#loop-name-0'?'#loop-0 .track-name':selector;
        for(const shifted of [false,true]){
            const before=await snapshot();
            const rect=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(actual)});e.scrollIntoView({block:'center',behavior:'instant'});e.focus();window.wheelEvidence=[];const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,value:e.value,id:e.id,protected:e.type==='range'||e.type==='number'};})()`);
            await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x,y:rect.y});
            await call('Input.dispatchMouseEvent',{type:'mouseWheel',x:rect.x,y:rect.y,deltaX:0,deltaY:120,modifiers:shifted?8:0});
            let evidence;for(let n=0;n<100;++n){evidence=await evaluate('window.wheelEvidence');if(evidence.length)break;await delay(10);}
            assert.equal(evidence.length,1,'One real wheel event reaches window capture');
            const event=evidence[0],blocked=rect.protected&&!shifted;
            assert.equal(event.id,rect.id,'Wheel targets original control');assert(event.trusted,'Use real browser input');assert.equal(event.shift,shifted);
            assert.equal(event.prevented,blocked,'Original Shift bypass / protected target');assert.equal(event.focused,!blocked,'Only protected non-Shift wheel blurs');
            if(blocked){
                assert.equal(await evaluate(`document.querySelector(${JSON.stringify(actual)}).value`),rect.value,'Blocked wheel preserves DOM value');
                const after=await snapshot();assert.equal(after.masterGain,before.masterGain);assert.equal(after.syncBpm,before.syncBpm);assert.equal(after.tracks[0].volume,before.tracks[0].volume);assert.equal(after.lastRequestId,before.lastRequestId,'Blocked wheel issues no loop command');
            }
            // Restore any platform default Shift-wheel edit through the real controls.
            await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(actual)});if(e.value!==${JSON.stringify(rect.value)}){e.value=${JSON.stringify(rect.value)};e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));}e.blur();})()`);
            await delay(50);
        }
    }
    await evaluate('window.wheelEvidence=null');
    console.log('PASS: trusted native wheel protection on MASTER, loop volume and BPM; Shift bypass/text focus; blocked values and audio controls unchanged');
    await parameter('#bpmInput',120);await parameter('#numBars',1);await parameter('#timeSigNum',4);
    await click('#countInVisual');
    await mode('off');
    // The original click bypasses MASTER, but never loop capture. No device is open.
    const metroBefore=await snapshot();assert.equal(metroBefore.masterGain,0);
    await evaluate(`document.getElementById('metronomeVol').value='0.35';document.getElementById('metronomeVol').dispatchEvent(new Event('input',{bubbles:true}));`);
    await click('#metronomeBtn');
    const metroSound=await wait(s=>s.metronomeEnabled&&Math.abs(s.metronomeVolume-0.35)<0.001&&s.outputPeak>0.005,'Native metronome audible with MASTER muted');
    assert.equal(metroSound.masterPeak,0,'Native MASTER meter excludes the monitor click');
    for(let n=0;n<25;++n){assert((await evaluate(`document.getElementById('master-ascii-vu').textContent`)).includes('-inf'),'Original MASTER display excludes the audible click');await delay(25);}

    assert.equal((await snapshot()).lastRequestId,metroBefore.lastRequestId,'Metronome is independent of loop commands');
    await click('#native-stop-all');await wait(s=>s.metronomeEnabled&&s.outputPeak>0.005,'STOP ALL preserves the original metronome');
    await evaluate(`document.activeElement?.blur()`);
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'KeyN',key:'n',windowsVirtualKeyCode:78});
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'KeyN',key:'n',windowsVirtualKeyCode:78});
    await wait(s=>!s.metronomeEnabled&&s.outputPeak===0,'N stops native metronome without a click tail');
    assert.equal((await snapshot()).lastRequestId,metroBefore.lastRequestId+1,'Only STOP ALL dispatches an audio transport command');
    await evaluate(`document.getElementById('metronomeVol').value='0.5';document.getElementById('metronomeVol').dispatchEvent(new Event('input',{bubbles:true}));`);
    // Actual original SOUND checkbox and real output, with no physical device.
    await parameter('#bpmInput',240);await mode('static');
    await click('#countInAudio');await wait(s=>s.countInAudio&&!s.countInVisual,'SOUND-only lead-in');
    for(const cancel of ['#loop-action-0','#native-stop-all']){
        const previousEpoch=readPcm().epoch;require('node:fs').writeFileSync(require('node:path').join(pcmDirectory,'pcm-reset'),'');
        await waitPcm(e=>e.epoch>previousEpoch&&!e.heard,'Reset actual output evidence');
        await evaluate(`(()=>{window.soundArmedSeen=false;window.soundArmedObserver?.disconnect();const row=document.getElementById('loop-0');window.soundArmedObserver=new MutationObserver(()=>{if(row.classList.contains('armed')&&document.getElementById('loop-action-0').textContent==='CANCEL'){window.soundArmedSeen=true;window.soundArmedObserver.disconnect();}});window.soundArmedObserver.observe(row,{attributes:true,attributeFilter:['class']});})()`);
        await click('#loop-action-0');
        // Read rendered callback samples, not an occasionally sampled telemetry
        // proxy. The old exact block-meter gate remains in CountInTests.cpp.
        const pcm=await waitPcm(e=>e.heard&&e.outputPeak>0.01&&e.frame<e.scheduledFrame,'Actual callback PCM while armed');
        assert.equal(pcm.masterPeak,0,'SOUND excluded from MASTER meter');assert.equal(pcm.countInFlash,0,'SOUND does not enable VISUAL');
        await click(cancel);await state('empty');
        assert(await evaluate('window.soundArmedSeen'),'Original CANCEL state was displayed while armed');
        const lastSound=readPcm().lastAudibleFrame;
        await delay(150);for(let n=0;n<20;++n){const stopped=await snapshot();assert.equal(stopped.outputPeak,0,'Cancel leaves no delayed SOUND');assert.equal(stopped.countInFlash,0);assert.equal(stopped.tracks[0].state,'empty');await delay(50);}
        assert.equal(readPcm().lastAudibleFrame,lastSound,'Actual PCM remains silent after cancellation');
    }
    require('node:fs').writeFileSync(require('node:path').join(pcmDirectory,'pcm-stop'),'');
    await click('#countInAudio');await wait(s=>!s.countInAudio,'SOUND disabled');
    await parameter('#bpmInput',120);await mode('off');
    // Header and button are interchangeable, including release outside the button.
    await exactlyOne(()=>click('#loop-state-text-0'),'Header starts actual recording');
    await state('recording');await delay(220);
    await exactlyOne(()=>click('#loop-action-0',{outside:true}),'Dragged release finishes once');
    let playing=await state('playing');assert(playing.tracks[0].length>4800,'Real captured PCM committed');
    await evaluate(`(()=>{const e=document.getElementById('native-master');e.value=1;e.dispatchEvent(new Event('input',{bubbles:true}));})()`);
    await wait(s=>s.outputPeak>0.02,'Captured PCM renders through production processor');
    // Original double-click resets must move a focused slider as well as DSP.
    await evaluate(`window.resetSliderEvents=[];window.addEventListener('dblclick',e=>{if(e.target.id?.match(/^loop-(speed|fbk)-slider-/))window.resetSliderEvents.push({id:e.target.id,trusted:e.isTrusted,focused:document.activeElement===e.target});},true);`);
    for(const [kind,field,initial,value] of [['speed','playbackRate',0.63,1],['fbk','feedback',0.23,0.8]]){
        const selector=`#loop-${kind}-slider-0`;
        await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});e.focus();e.value=${initial};e.dispatchEvent(new Event('input',{bubbles:true}));})()`);
        await wait(s=>Math.abs(s.tracks[0][field]-initial)<1e-6,'Set pre-reset '+kind);
        const before=await snapshot();
        const rect=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
        for(const clickCount of [1,2]){
            await call('Input.dispatchMouseEvent',{type:'mousePressed',...rect,button:'left',clickCount});
            await call('Input.dispatchMouseEvent',{type:'mouseReleased',...rect,button:'left',clickCount});
        }
        const after=await wait(s=>s.lastRequestId>before.lastRequestId&&Math.abs(s.tracks[0][field]-value)<1e-6,'Actual double-click native reset '+kind);
        assert.equal(await evaluate(`Number(document.querySelector(${JSON.stringify(selector)}).value)`),value,'Focused '+kind+' slider must visibly reset like original JS');
        assert(await evaluate(`document.activeElement===document.querySelector(${JSON.stringify(selector)})`),'Reset stays focused');
        const evidence=await evaluate('window.resetSliderEvents');assert(evidence.some(e=>e.id===selector.slice(1)&&e.trusted&&e.focused),'Actual trusted focused double click');
        assert.equal(after.tracks[0].state,'playing');assert.equal(after.tracks[0].length,before.tracks[0].length);assert.equal(after.tracks[0].waveformId,before.tracks[0].waveformId);assert.equal(after.tracks[0].undoCount,before.tracks[0].undoCount);
        await wait(s=>s.outputPeak>0.02,'Captured PCM survives '+kind+' reset');
        await evaluate('document.activeElement.blur()');
    }
    console.log('PASS: trusted double-click resets focused Speed/Feedback position and native values; captured nonzero PCM and history retained');
    // Original synchronized range edits snap both DSP rate and the focused thumb.
    for(const sync of ['static','dynamic']){
        await mode(sync);
        for(const [initial,expected] of [[0.36,0.25],[0.37,0.5],[0.73,0.5],[0.74,1],[1.48,1],[1.49,2]]){
            await evaluate(`(()=>{const e=document.getElementById('loop-speed-slider-0');e.scrollIntoView({block:'center',behavior:'instant'});e.focus();e.value=${initial};})()`);
            const before=await snapshot();
            await call('Input.dispatchKeyEvent',{type:'rawKeyDown',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39,nativeVirtualKeyCode:39});
            await call('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowRight',code:'ArrowRight',windowsVirtualKeyCode:39,nativeVirtualKeyCode:39});
            const after=await wait(s=>s.lastRequestId>before.lastRequestId,'Synchronized speed input acknowledged');
            assert.equal(after.tracks[0].playbackRate,expected,'Original '+sync+' speed quantization after '+initial);
            assert.equal(await evaluate(`Number(document.getElementById('loop-speed-slider-0').value)`),expected,'Focused synchronized thumb snaps');
            assert(await evaluate(`document.activeElement===document.getElementById('loop-speed-slider-0')`),'Speed edit retains focus');
            for(const field of ['state','length','waveformId','undoCount'])assert.equal(after.tracks[0][field],before.tracks[0][field],'Speed preserves '+field);
            await wait(s=>s.outputPeak>0.02,'Speed edit preserves captured audio');
        }
    }
    await mode('off');
    await evaluate(`(()=>{const e=document.getElementById('loop-speed-slider-0');e.value=1;e.dispatchEvent(new Event('input',{bubbles:true}));e.blur();})()`);
    await wait(s=>s.tracks[0].playbackRate===1,'Restore manual speed');
    console.log('PASS: original static/dynamic speed quantization with focused trusted range-key input and nonzero PCM');
    const waveform=await wait(s=>s.tracks[0].waveformId>0&&s.tracks[0].wavePeaks?.length===720,'Worker waveform reaches native editor');
    assert(waveform.tracks[0].wavePeaks.some(p=>p>0.01),'Real committed PCM yields waveform peaks');
    await delay(200);
    const canvas=await evaluate(`(()=>{const c=document.querySelector('#loop-0 canvas'),ctx=c.getContext('2d'),w=c.width,h=c.height;let shaped=0;for(const x of [Math.floor(w/4),Math.floor(w/2),Math.floor(w*3/4)]){const column=ctx.getImageData(x,0,1,h).data,mid=Math.floor(h/2)*4;if(column[mid+1]>0&&column[3]===0&&column[(h-1)*4+3]===0)shaped++;}return {shaped,w,h};})()`);
    assert(canvas.shaped>=2,'Canvas shows original centered waveform bars rather than full-height progress fill: '+JSON.stringify(canvas));
    const known=await evaluate(`AionNative.request({operation:'snapshot',knownGeneration:${waveform.generation},knownWaveforms:${JSON.stringify(waveform.tracks.map(t=>t.waveformId))}})`);
    assert(!Object.hasOwn(known.tracks[0],'wavePeaks'),'Unchanged waveform payload omitted on acknowledged polls');
    const screenshot=await call('Page.captureScreenshot',{format:'png'});require('node:fs').writeFileSync(require('node:path').resolve(__dirname,'../build/native-mouse-waveform.png'),Buffer.from(screenshot.data,'base64'));
    // Drag the original Start slider using actual pointer input, then release.
    const beforePhase=await snapshot();
    const phaseRect=await evaluate(`(()=>{const e=document.getElementById('loop-start-slider-0');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x,y:r.y+r.height/2,w:r.width};})()`);
    await call('Input.dispatchMouseEvent',{type:'mousePressed',x:phaseRect.x+phaseRect.w*0.2,y:phaseRect.y,button:'left',clickCount:1});
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:phaseRect.x+phaseRect.w*0.7,y:phaseRect.y,buttons:1,button:'left'});
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:phaseRect.x+phaseRect.w*0.7,y:phaseRect.y,button:'left',clickCount:1});
    const phased=await wait(s=>s.tracks[0].startDelay>0.5&&s.tracks[0].phaseApplyFrame===0,'Mouse Start phase applied');
    assert.equal(phased.tracks[0].state,'playing');assert.equal(phased.tracks[0].length,beforePhase.tracks[0].length);assert(!phased.rejected);
    assert.equal(await evaluate(`document.getElementById('loop-delay-display-0').textContent`),Math.floor(phased.tracks[0].startDelay*(phased.tracks[0].length/phased.sampleRate*1000))+'ms');
    await wait(s=>s.outputPeak>0.02,'Phase-shifted PCM keeps playing');
    const divisions=Math.round((phased.tracks[0].length/phased.sampleRate)/(60/phased.syncBpm/4));
    const snapped=divisions>0?Math.round(phased.tracks[0].startDelay*divisions)/divisions:phased.tracks[0].startDelay;
    await exactlyOne(()=>click('#loop-0 .btn-purple[title="Quantize Start to 16th Note"]'),'Original Q button sends exactly one command');
    const quantized=await wait(s=>Math.abs(s.tracks[0].startDelay-snapped)<1e-14&&s.tracks[0].phaseApplyFrame===0,'Mouse Q phase applied');
    assert.equal(quantized.tracks[0].length,phased.tracks[0].length);assert.equal(quantized.tracks[0].state,'playing');assert(!quantized.rejected);
    await wait(s=>s.outputPeak>0.02,'Quantized PCM keeps playing');
    for(const direction of [-1,1]){
        const before=await snapshot();let expected=before.tracks[0].startDelay+direction*((60/before.syncBpm/16)/(before.tracks[0].length/before.sampleRate));
        if(expected<0)expected+=1;if(expected>1)expected-=1;if(expected<0||expected>1)expected=((expected%1)+1)%1;
        await exactlyOne(()=>click(`#loop-0 [aria-label="Nudge loop start ${direction<0?'left':'right'}"]`),'Original nudge sends one command');
        const nudged=await wait(s=>Math.abs(s.tracks[0].startDelay-expected)<1e-14&&s.tracks[0].phaseApplyFrame===0,'Mouse nudge applied');
        assert.equal(nudged.tracks[0].state,'playing');assert.equal(nudged.tracks[0].length,before.tracks[0].length);assert(!nudged.rejected);
        await wait(s=>s.outputPeak>0.02,'Nudged PCM keeps playing');
    }
    await exactlyOne(()=>click('#loop-0 button[title="RTRG"]'),'Original RTRG sends exactly one command');
    await wait(s=>s.tracks[0].startDelay===0&&s.tracks[0].phaseApplyFrame===0&&s.tracks[0].state==='playing','RTRG resets Start and replaces source');
    await wait(s=>s.outputPeak>0.02,'Retriggered PCM keeps playing');
    assert.equal(await evaluate(`document.getElementById('loop-delay-display-0').textContent`),'0ms');
    await click('#native-stop-all');await state('stopped');
    await exactlyOne(()=>click('#loop-0 button[title="RTRG"]'),'Stopped RTRG starts playback');
    await state('playing');await wait(s=>s.outputPeak>0.02,'Stopped RTRG produces PCM');

    // Original STUT is momentary: actual pointer hold, release outside and blur.
    const stutRect=()=>evaluate(`(()=>{const e=[...document.querySelectorAll('#loop-0 button')].find(e=>e.textContent.trim()==='STUT');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,disabled:e.disabled};})()`);
    const stutDown=async()=>{const r=await stutRect();assert(!r.disabled);await call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x,y:r.y,button:'left',clickCount:1});};
    await evaluate(`window.stutDraws=[];window.originalStutDraw=AionDrawWaveform;window.AionDrawWaveform=(...args)=>{if(args[0].id==='loop-wave-0')window.stutDraws.push(args[2]);return window.originalStutDraw(...args);};`);
    const beforeStut=await snapshot();await stutDown();
    const fragment=await wait(s=>s.tracks[0].stuttering&&s.outputPeak>0.02,'Held STUT repeats native PCM');
    assert.equal(fragment.tracks[0].state,'playing');assert.equal(fragment.tracks[0].length,beforeStut.tracks[0].length);assert(fragment.tracks[0].stutterEnd>fragment.tracks[0].stutterStart);
    assert(fragment.tracks[0].stutterClock,'Original visual clock enabled');await delay(300);await evaluate('window.stutDraws=[]');await delay(600);
    const drawn=await evaluate('window.stutDraws'),from=fragment.tracks[0].stutterStart/fragment.tracks[0].length,to=fragment.tracks[0].stutterEnd/fragment.tracks[0].length;
    assert(drawn.some(p=>p<from-0.02||p>to+0.02),'Original playhead continues outside the repeated fragment: '+JSON.stringify({drawn,from,to}));
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:1190,y:790,button:'left',buttons:1});await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});
    await wait(s=>!s.tracks[0].stuttering&&s.tracks[0].stutterClock&&s.tracks[0].state==='playing'&&s.outputPeak>0.02,'Release outside STUT resumes full PCM');
    await stutDown();await wait(s=>s.tracks[0].stuttering,'STUT before blur');await evaluate(`window.dispatchEvent(new Event('blur'))`);await wait(s=>!s.tracks[0].stuttering,'Blur releases STUT');
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});
    // Suppress unload/blur release RPCs, and navigate to a page with no bootstrap.
    // The native editor must release the held fragment without any JS cleanup.
    await evaluate(`(()=>{const backend=window.__JUCE__.backend,emit=backend.emitEvent;backend.emitEvent=function(type,data){if(data.params?.[0]?.operation==='releaseStutter')return;return emit.call(this,type,data);};})()`);
    require('node:fs').writeFileSync(require('node:path').join(pcmDirectory,'navigation-probe-start'),'1');
    await stutDown();await wait(s=>s.tracks[0].stuttering,'STUT before navigation with lost unload RPC');
    const heldNavigation=await waitNavigation(e=>e.stuttering&&e.playing&&e.outputPeak>0.02,'Audio-owned held fragment before blank navigation');
    assert.deepEqual(await evaluate('window.mouseErrors'),[]);await call('Page.navigate',{url:'about:blank'});
    let blank=false;for(let n=0;n<100&&!blank;++n){try{blank=await evaluate(`location.href==='about:blank'`);}catch(error){if(!/context|navigat/i.test(error.message))throw error;}if(!blank)await delay(40);}assert(blank,'Navigation reaches blank document with no Aion frontend');
    const releasedNavigation=await waitNavigation(e=>e.frame>heldNavigation.frame&&!e.stuttering&&e.playing&&e.outputPeak>0.02,'Native blank navigation releases STUT despite lost unload RPC and absent bootstrap');
    assert.equal(releasedNavigation.length,heldNavigation.length);assert.equal(releasedNavigation.waveformId,heldNavigation.waveformId,'Native navigation preserves stored PCM');
    require('node:fs').writeFileSync(require('node:path').join(pcmDirectory,'navigation-probe-stop'),'1');await call('Page.navigate',{url:target.url});
    let restored=false;for(let n=0;n<100&&!restored;++n){try{restored=await evaluate(`location.href===${JSON.stringify(target.url)}&&document.readyState==='complete'&&typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`);}catch(error){if(!/context|navigat/i.test(error.message))throw error;}if(!restored)await delay(40);}assert(restored,'Original editor returns after blank-navigation regression');
    await wait(s=>!s.tracks[0].stuttering&&s.tracks[0].state==='playing','Native release remains after editor returns');
    await evaluate(`window.mouseErrors=[];window.addEventListener('error',e=>window.mouseErrors.push(e.message));document.documentElement.style.scrollBehavior='auto'`);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});
    await stutDown();await wait(s=>s.tracks[0].stuttering,'STUT before editor reload');assert.deepEqual(await evaluate('window.mouseErrors'),[]);await call('Page.reload');await delay(400);
    for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
    await wait(s=>!s.tracks[0].stuttering&&s.tracks[0].state==='playing','Reload releases held STUT without stopping the loop');
    await evaluate(`window.mouseErrors=[];window.addEventListener('error',e=>window.mouseErrors.push(e.message));document.documentElement.style.scrollBehavior='auto'`);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});
    // A held start behind a paused queue must never fire after its release.
    await pauseDispatch();await stutDown();await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});await resumeDispatch();await delay(150);assert(!(await snapshot()).tracks[0].stuttering,'Released queued STUT cannot latch later');

    const namedPcm=await snapshot();
    await click('#loop-0 .track-name');await call('Input.insertText',{text:'Guitarra Á 1 + 😀'});
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Enter',key:'Enter',windowsVirtualKeyCode:13});
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Enter',key:'Enter',windowsVirtualKeyCode:13});
    const renamed=await wait(s=>s.tracks[0].name==='Guitarra Á 1 + 😀','Rename real playing PCM');
    assert.equal(renamed.lastRequestId,namedPcm.lastRequestId,'Name input cannot trigger loop keyboard shortcuts');
    assert.equal(renamed.tracks[0].length,namedPcm.tracks[0].length);assert.equal(renamed.tracks[0].state,'playing');
    await wait(s=>s.outputPeak>0.02,'Renamed PCM keeps playing');

    // Pause only frontend dispatch, preserving real core PCM and audio callbacks.
    // A deletion queued at 4s must be discarded if focus is then lost.
    const beforeBlur=await snapshot();
    await pauseDispatch();
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await delay(4200);
    await evaluate(`window.dispatchEvent(new Event('blur'))`);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    await resumeDispatch();
    // The short STOP has expired after the queue's existing 3s limit. Retain
    // this case: the uncancelled deletion used to erase the still-playing clip.
    const afterBlur=await state('playing');assert.equal(afterBlur.tracks[0].length,beforeBlur.tracks[0].length,'Blur invalidates queued deletion, preserving actual PCM');
    await delay(200);assert.equal((await snapshot()).lastRequestId,beforeBlur.lastRequestId,'Expired short STOP and cancelled hold send no commands');
    await wait(s=>s.outputPeak>0.02,'PCM retained after cancelling queued deletion');
    // Also cover an acknowledged STOP: only hold commands wait behind dispatch.
    const beforeAckBlur=await snapshot();
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await pauseDispatch();await delay(4200);await evaluate(`window.dispatchEvent(new Event('blur'))`);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await resumeDispatch();
    const acknowledged=await state('stopped');assert.equal(acknowledged.tracks[0].length,beforeAckBlur.tracks[0].length,'Cancelled queued delete preserves an already-stopped clip');
    assert.equal(acknowledged.lastRequestId,beforeAckBlur.lastRequestId+1,'Acknowledged short STOP remains the only command');
    await click('#loop-action-0');await state('playing');await wait(s=>s.outputPeak>0.02,'Stopped PCM still replays after cancellation');
    // Clear/re-record while the physical key remains held: an old STOP cannot
    // delete the replacement take when its original 4s deadline arrives.
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    assert.equal((await snapshot()).tracks[0].name,'','DEL clears the name with the real loop');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');
    const replacement=await state('playing');await delay(4200);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    const protectedTake=await state('playing');assert.equal(protectedTake.tracks[0].length,replacement.tracks[0].length,'DEL cancels stale holds before a replacement take');
    await wait(s=>s.outputPeak>0.02,'Replacement PCM survives the previous gesture deadline');
    // Seed a finalization wait in the actual frontend FIFO, without changing
    // snapshots or PCM. DEL must cancel unsent actions and clear immediately.
    await evaluate(`window.savedClearPump=AionCommandQueue.prototype.pump;AionCommandQueue.prototype.pump=function(){window.clearTestQueue=this;return window.savedClearPump.call(this);};`);
    for(let n=0;n<60;++n){if(await evaluate(`!!window.clearTestQueue`))break;await delay(25);}
    await evaluate(`AionCommandQueue.prototype.pump=window.savedClearPump;window.clearTestQueue.enqueue(()=>AionCommandQueue.wait,{track:0});window.clearTestQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});delete window.savedClearPump;delete window.clearTestQueue;`);
    const beforeDeferredClear=await snapshot();await click('#loop-0 .loop-track-delete');
    const immediatelyCleared=await wait(s=>s.tracks[0].state==='empty','DEL bypasses finalization wait',1200);
    assert.equal(immediatelyCleared.lastRequestId,beforeDeferredClear.lastRequestId+1,'Only explicit DEL dispatches');
    await delay(3200);assert.equal((await snapshot()).tracks[0].state,'empty','Cancelled deferred record cannot resurrect the deleted loop');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');await state('playing');
    // The 4s STOP gesture must clear just like DEL, even behind another track's
    // finalization wait, and discard an unsent record belonging to this track.
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await pauseDispatch();await delay(2800);
    await evaluate(`window.pausedHoldQueue.enqueue(()=>AionCommandQueue.wait,{track:1});window.pausedHoldQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});`);
    await resumeDispatch();await delay(1400);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    await wait(s=>s.tracks[0].state==='empty','Held STOP bypasses another track finalization wait',1200);
    await delay(3200);assert.equal((await snapshot()).tracks[0].state,'empty','Held STOP discards a queued replacement recording');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');await state('playing');
    const beforeHold=await snapshot();
    await click('#loop-action-0',{hold:3800});
    const kept=await state('stopped');assert.equal(kept.tracks[0].length,beforeHold.tracks[0].length,'Releasing STOP before 4s keeps PCM');
    await delay(400);assert.equal((await snapshot()).tracks[0].length,kept.tracks[0].length,'Released hold cannot delete later');
    await click('#loop-action-0');await state('playing');
    await click('#loop-action-0',{hold:4200});
    const heldStop=await state('empty');
    assert.equal(heldStop.tracks[0].length,0,'Holding STOP intentionally deletes a bad loop');
    assert.equal(heldStop.tracks[0].undoCount,0,'Quick delete clears local undo history');
    await click('#loop-action-0');await state('recording');await delay(220);
    await click('#loop-action-0');await state('playing');
    await exactlyOne(()=>click('#loop-state-symbol-0'),'Header stops playback');await state('stopped');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse replays committed loop');await state('playing');
    // Clicking nested controls/waveform must never also toggle transport.
    const beforeWave=await snapshot();await click('#loop-wave-0');await delay(250);
    assert.equal((await snapshot()).lastRequestId,beforeWave.lastRequestId,'Waveform never toggles transport');
    const choice=await wait(s=>s.exportStatus==='Choose a WAV destination','Waveform picker ready');
    require('node:fs').writeFileSync(require('node:path').resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]||64}`,choice.exportFilename+'.choose'),'');
    await wait(s=>!s.exportBusy,'Waveform export finishes independently');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Nested control sends only its own action');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Unmute without toggling transport');
    await exactlyOne(()=>click('#native-stop-all'),'Stop all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // FINISH may commit and auto-play before 600ms; the same hold must not stop it.
    await click('#loop-action-0');await state('recording');await delay(180);
    await click('#loop-action-0',{hold:4200});playing=await state('playing');
    const finishedLength=playing.tracks[0].length;assert(finishedLength>4800,'Held FINISH commits PCM');
    await wait(s=>s.outputPeak>0.02,'Held FINISH preserves auto-play');
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await delay(4200);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    const heldKeyStop=await state('empty');assert.equal(heldKeyStop.tracks[0].length,0,'Held keyboard STOP also quick-deletes the bad loop');
    // STATIC: arm cancellation, automatic capture, exact length, queued play cancellation.
    await mode('static');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early static grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse arms static capture');await state('armed');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse cancels arm');await state('empty');
    await exactlyOne(()=>click('#loop-state-text-0'),'Header arms static capture');
    await state('recording');playing=await state('playing');
    assert.equal(playing.tracks[0].length,96000,'Static capture has exact two-second grid PCM');
    await wait(s=>s.outputPeak>0.02,'Static PCM playback');
    // A queued stop is still playing audio; show the original pending symbol,
    // then the stopped symbol only after an immediate second STOP.
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early pending-stop display phase');
    const statusBefore=await snapshot();
    await exactlyOne(()=>click('#loop-action-0'),'Queue static stop');
    const pendingStop=await state('stopping');
    assert.equal(await evaluate(`document.getElementById('loop-state-symbol-0').textContent`),'[.]','Original pending STOP symbol differs from stopped');
    assert.equal(await evaluate(`document.getElementById('loop-state-text-0').textContent`),'STOPPING');
    assert.equal(pendingStop.tracks[0].waveformId,statusBefore.tracks[0].waveformId);
    await wait(s=>s.outputPeak>0.02,'Queued stop still produces captured PCM');
    await exactlyOne(()=>click('#loop-action-0'),'Immediate second STOP');
    const statusStopped=await state('stopped');
    assert.equal(await evaluate(`document.getElementById('loop-state-symbol-0').textContent`),'[S]','Only completed STOP shows stopped');
    assert.equal(await evaluate(`document.getElementById('loop-extra-info-0').textContent`),' (2.0s)','Original duration spacing');
    assert.equal(statusStopped.tracks[0].length,statusBefore.tracks[0].length);
    assert.equal(statusStopped.tracks[0].undoCount,statusBefore.tracks[0].undoCount);
    console.log('PASS: real queued STOP [.] / completed STOP [S], original duration text, nonzero PCM and clip/history retained');
    await exactlyOne(()=>click('#native-stop-all'),'Stop static playback');await state('stopped');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early queued-play grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Queue static play');await state('queued');
    await exactlyOne(()=>click('#loop-state-text-0'),'Header cancels queued play');await state('stopped');
    await delay(2100);assert.equal((await snapshot()).tracks[0].state,'stopped','Cancelled play stays cancelled across grid');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early held queued-cancel phase');
    await click('#loop-action-0');await state('queued');
    await click('#loop-action-0',{hold:4200});
    const heldCancel=await state('stopped');assert.equal(heldCancel.tracks[0].length,96000,'Holding queued CANCEL preserves PCM');
    await exactlyOne(()=>click('#loop-action-0'),'Queue playback again');await state('playing');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early double-stop grid phase');
    // Assert both acknowledgements, then check late duplicates after the pair.
    // Extra CDP/state settling between taps can otherwise cross the bar and
    // correctly turn the second tap into PLAY instead of STOP NOW.
    const queuedStop=await exactlyOne(()=>click('#loop-action-0'),'Queue quantized stop',true);assert.equal(queuedStop.tracks[0].state,'stopping');
    const cutStop=await exactlyOne(()=>click('#loop-action-0'),'Second mouse stop cuts now');await state('stopped');
    assert.equal(cutStop.lastRequestId,queuedStop.lastRequestId+1,'Exactly two commands across the quantized-stop pair');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // DYNAMIC: starts on mouse down, first finish schedules, second finishes immediately.
    await mode('dynamic');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early dynamic grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Dynamic mouse starts recording');await state('recording');
    const live=await wait(s=>s.liveInput?.track===0&&s.liveInput.samples.length===512&&s.liveInput.samples.some(v=>Math.abs(v)>0.02),'Native DYNAMIC input trace');
    assert.equal(live.liveInput.startFrame,live.tracks[0].captureStartFrame);
    await delay(200);
    const trace=await evaluate(`(()=>{const c=document.querySelector('#loop-0 canvas'),p=c.getContext('2d').getImageData(0,0,c.width,c.height).data;let red=0,yellow=0;for(let i=0;i<p.length;i+=4){if(p[i]>0&&p[i+1]===0&&p[i+2]===0)red++;if(p[i]>0&&p[i+1]>0&&p[i+2]===0)yellow++;}return {red,yellow};})()`);
    assert(trace.red>20&&trace.yellow>0,'Original red live trace and dynamic bar markers visible: '+JSON.stringify(trace));
    const liveImage=await call('Page.captureScreenshot',{format:'png'});require('node:fs').writeFileSync(require('node:path').resolve(__dirname,'../build/native-live-input.png'),Buffer.from(liveImage.data,'base64'));
    await mode('off');await delay(150);
    const off=await snapshot(),expectedProgress=Math.min(1,(off.frame-off.tracks[0].captureStartFrame)/off.sampleRate/10);
    const actualProgress=await evaluate(`(()=>{const c=document.querySelector('#loop-0 canvas'),ctx=c.getContext('2d'),p=ctx.getImageData(0,0,c.width,c.height).data;for(let x=0;x<c.width;x++)if(p[x*4]===255&&p[x*4+1]===255&&p[x*4+2]===255)return x/c.width;return -1;})()`);
    assert(Math.abs(actualProgress-expectedProgress)<0.06,'Switching sync off restores original ten-second recording playhead');
    await mode('dynamic');


    // Screenshot/mode setup can consume the remaining bar. Require a fresh
    // early phase so this case exercises queued finish, not the original 50ms
    // immediate-finish boundary. Keep stopPending mandatory below.
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early observed dynamic finish phase');
    await delay(200);await click('#loop-action-0');
    await wait(s=>s.tracks[0].stopPending,'Dynamic finish deadline');
    playing=await state('playing');assert(playing.tracks[0].length>0&&playing.tracks[0].dynamicRecorded,'Dynamic PCM committed');
    assert(!playing.liveInput,'Finished capture stops live input delivery');
    await wait(s=>s.outputPeak>0.02,'Dynamic PCM playback');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early dynamic second-tap phase');
    await click('#loop-action-0');await state('recording');await delay(150);
    const nextLive=await wait(s=>s.liveInput?.track===0&&s.liveInput.startFrame===s.tracks[0].captureStartFrame,'New take owns its live trace');assert.notEqual(nextLive.liveInput.startFrame,live.liveInput.startFrame,'Old trace cannot bleed into a later take');
    await click('#loop-action-0');await wait(s=>s.tracks[0].stopPending,'Dynamic scheduled finish before second tap');
    await click('#loop-action-0');playing=await state('playing');
    assert(playing.tracks[0].length<96000,'Second finish mouse tap commits before the bar deadline');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // A short fixed take can finish automatically while RECORD is still held.
    await parameter('#bpmInput',600);await parameter('#timeSigNum',1);
    await mode('static');await wait(s=>s.syncBpm===600&&s.syncNum===1,'Fast static grid');
    await click('#loop-action-0',{hold:4200});
    const heldRecord=await state('playing');assert.equal(heldRecord.tracks[0].length,4800,'Held RECORD does not stop an auto-completed take');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    await parameter('#bpmInput',120);await parameter('#timeSigNum',4);
    await mode('off');
    await click('#autoPlayAfterRecord');await wait(s=>!s.autoPlayAfterRecord,'Auto-play off');
    await click('#loop-action-0');await state('recording');await delay(150);await click('#loop-action-0');await state('stopped');
    await click('#loop-action-0');await state('playing');
    await wait(s=>s.outputPeak>0.02,'Explicit play with auto-play disabled');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    const rapidBefore=await snapshot();
    await click('#loop-action-0');await click('#loop-action-0');await click('#loop-action-0');
    const rapidAfter=await state('playing');
    assert.equal(rapidAfter.lastRequestId,rapidBefore.lastRequestId+3,'Rapid record/finish/play resolves against acknowledged native state');
    assert(rapidAfter.tracks[0].length>0&&!rapidAfter.rejected,'Rapid mouse sequence commits real audio');
    await delay(750);assert.equal((await snapshot()).lastRequestId,rapidAfter.lastRequestId,'Released mouse gestures leave no delayed hold');
    // Original X and DEL ALL share the real native command and confirmation.
    await click('#loop-action-19');await state('recording',19);await delay(150);await click('#loop-action-19');await state('stopped',19);
    const bankBefore=await snapshot();assert(bankBefore.tracks[0].length>0&&bankBefore.tracks[19].length>0,'Two real recorded loops before DEL ALL');
    const x=async()=>{await call('Input.dispatchKeyEvent',{type:'keyDown',code:'KeyX',key:'x',windowsVirtualKeyCode:88});await call('Input.dispatchKeyEvent',{type:'keyUp',code:'KeyX',key:'x',windowsVirtualKeyCode:88});};
    await click('#loop-0 .track-name');await x();await delay(150);assert.equal(clearDialogs.length,0,'X in a loop name cannot prompt or delete');await evaluate('document.activeElement.blur()');
    await x();assert.deepEqual(clearDialogs,['Clear ALL loops and master recordings?'],'Original confirmation text');
    await delay(150);let cancelled=await snapshot();assert.equal(cancelled.lastRequestId,bankBefore.lastRequestId,'Cancelled X emits no audio command');
    for(const id of [0,19]){assert.equal(cancelled.tracks[id].waveformId,bankBefore.tracks[id].waveformId);assert.equal(cancelled.tracks[id].length,bankBefore.tracks[id].length);assert.equal(cancelled.tracks[id].state,bankBefore.tracks[id].state);}
    await wait(s=>s.outputPeak>0.02,'Cancelled X preserves actual loop playback');
    await stutDown();await wait(s=>s.tracks[0].stuttering,'STUT held before DEL ALL');await pauseDispatch();
    for(const [code,key,vk] of [['Digit1','1',49],['Digit2','2',50]])await call('Input.dispatchKeyEvent',{type:'keyDown',code,key,windowsVirtualKeyCode:vk});
    const beforeClear=await snapshot();acceptClear=true;await x();assert.equal(clearDialogs.length,2);
    await resumeDispatch();const cleared=await wait(s=>s.tracks.every(t=>t.state==='empty'&&t.length===0&&!t.stuttering)&&s.outputPeak===0,'Confirmed X clears all real PCM and held STUT');
    assert.equal(cleared.lastRequestId,beforeClear.lastRequestId+1,'Only clearAll survives paused older play/record gestures');
    await delay(4300);assert.equal((await snapshot()).lastRequestId,cleared.lastRequestId,'Cancelled held timers cannot revive or delete after X');
    for(const [code,key,vk] of [['Digit1','1',49],['Digit2','2',50]])await call('Input.dispatchKeyEvent',{type:'keyUp',code,key,windowsVirtualKeyCode:vk});
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:1190,y:790,button:'left',clickCount:1});
    acceptClear=false;const beforeButton=await snapshot();await click('#mod-menu button[data-i18n="CLEAR_ALL"]');assert.equal(clearDialogs.length,3);assert.equal((await snapshot()).lastRequestId,beforeButton.lastRequestId,'Original DEL ALL button still cancels without a command');
    acceptClear=true;await exactlyOne(()=>click('#mod-menu button[data-i18n="CLEAR_ALL"]'),'Original DEL ALL button still clears through shared handler');
    assert(clearDialogs.every(message=>message==='Clear ALL loops and master recordings?'));
    console.log('PASS: original X/DEL ALL native confirmation cancel/accept, text focus, real stored PCM, held STUT and paused key/timer cancellation');
    // Real keypad input: Shift does not choose the Digit-row bank; navigation
    // cannot start/stop audio, and key-up remains physical across NumLock changes.
    await evaluate(`document.activeElement.blur();window.padEvents=[];document.addEventListener('keydown',e=>{if(e.code.startsWith('Numpad'))window.padEvents.push({code:e.code,key:e.key,shift:e.shiftKey});},true);`);
    const keypad=(type,digit,key,shift=false,virtualKey=96+Number(digit))=>call('Input.dispatchKeyEvent',{type,code:'Numpad'+digit,key,modifiers:shift?8:0,windowsVirtualKeyCode:virtualKey,isKeypad:true});
    for(const [digit,id,nav,virtualKey] of [['1',0,'End',35],['0',9,'Insert',45]]){
        await keypad('keyDown',digit,digit,true);const padRecord=await state('recording',id);assert.equal(padRecord.tracks[id+10].state,'empty','Shift+keypad keeps the original lower bank');
        await keypad('keyUp',digit,nav,false,virtualKey);await delay(180);
        await keypad('keyDown',digit,digit,true);await keypad('keyUp',digit,nav,false,virtualKey);const padTake=await state('stopped',id);assert(padTake.tracks[id].length>0,'Keypad commits actual captured audio');
        await keypad('keyDown',digit,digit,true);await keypad('keyUp',digit,nav,false,virtualKey);
        const padPlay=await wait(s=>s.tracks[id].state==='playing'&&s.outputPeak>0.02,'Keypad take produces native PCM');
        const navKeys=['Insert','End','ArrowDown','PageDown','ArrowLeft','Clear','ArrowRight','Home','ArrowUp','PageUp'],navVk=[45,35,40,34,37,12,39,36,38,33];
        for(let n=0;n<10;++n)for(const shift of [false,true]){await keypad('keyDown',String(n),navKeys[n],shift,navVk[n]);await keypad('keyUp',String(n),navKeys[n],shift,navVk[n]);}
        await delay(4300);const afterPad=await snapshot();assert.equal(afterPad.lastRequestId,padPlay.lastRequestId,'Keypad navigation and released timers issue no audio command');assert.equal(afterPad.tracks[id].waveformId,padTake.tracks[id].waveformId);assert.equal(afterPad.tracks[id].length,padTake.tracks[id].length);assert.equal(afterPad.tracks[id].state,'playing');assert(afterPad.outputPeak>0.02,'Navigation preserves real playback');
        await keypad('keyDown',digit,digit,true);await keypad('keyUp',digit,nav,false,virtualKey);await state('stopped',id);
        await exactlyOne(()=>click(`#loop-${id} .loop-track-delete`),'Clean up keypad take');
    }
    const padEvents=await evaluate('window.padEvents');assert(padEvents.some(e=>e.code==='Numpad1'&&e.key==='1'&&e.shift),'Browser receives shifted keypad character');assert(padEvents.some(e=>e.code==='Numpad0'&&e.key==='Insert'),'Browser receives NumLock navigation');
    console.log('PASS: actual Shift+keypad record/finish/play/stop on original bank, all navigation keys ignored, NumLock-changing release and >4s PCM preservation');
    // Missed key-up: a long RECORD is intentional until a fresh >10s press.
    await evaluate('document.activeElement.blur()');
    const keyOne=async(type,autoRepeat=false)=>call('Input.dispatchKeyEvent',{type,code:'Digit1',key:'1',windowsVirtualKeyCode:49,autoRepeat});
    await keyOne('keyDown');const lostRelease=await state('recording');
    await delay(10100);let longRecording=await snapshot();assert.equal(longRecording.tracks[0].state,'recording','A long held RECORD is never automatically expired');assert.equal(longRecording.lastRequestId,lostRelease.lastRequestId,'Long hold cannot invent finish/cut/delete');
    await keyOne('keyDown',true);await delay(120);assert.equal((await snapshot()).lastRequestId,lostRelease.lastRequestId,'OS repeat after ten seconds remains ignored');
    await keyOne('keyDown');const recovered=await state('stopped');
    assert.equal(recovered.lastRequestId,lostRelease.lastRequestId+1,'Fresh key recovers lost release and finishes exactly once');assert(recovered.tracks[0].length>=480000,'Recovered keyboard recording retains over ten seconds of real input PCM');
    await keyOne('keyUp');await keyOne('keyDown');await keyOne('keyUp');
    const recoveredPlay=await wait(s=>s.tracks[0].state==='playing'&&s.outputPeak>0.02,'Recovered take produces nonzero native playback');assert.equal(recoveredPlay.tracks[0].waveformId,recovered.tracks[0].waveformId,'Recovery/playback preserves committed PCM');
    await delay(750);assert.equal((await snapshot()).lastRequestId,recoveredPlay.lastRequestId,'Old/released keyboard timers cannot touch recovered PCM');
    await exactlyOne(()=>click('#mod-menu button[data-i18n="CLEAR_ALL"]'),'Clean up recovered keyboard take');
    console.log('PASS: actual missed key-up, >10s RECORD remains held, OS-repeat ignored, fresh-key finish/play preserves nonzero native PCM and old timers stay cancelled');
    assert.deepEqual(await evaluate('window.mouseErrors'),[],'No browser gesture exceptions');assert.deepEqual(runtimeErrors,[],'No runtime exceptions across editor reload');
    console.log('PASS: real mouse/header arm, cancel, record, native PCM playback, Start drag/Q/nudges, STUT hold/outside-release/blur/queued-cancel, nested-control isolation, STATIC queued play/stop and DYNAMIC finish/second tap, auto-play off, rapid record/finish/play, held STOP quick-deletes; FINISH/CANCEL/RECORD preserve PCM; no device opened');
    ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
