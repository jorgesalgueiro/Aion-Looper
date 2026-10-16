window.addEventListener('aion-native-ready',async()=>{
    const checks=[],wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));let result,finalStatus;
    function assert(ok,label){if(!ok)throw new Error(label);checks.push(label);}
    async function until(predicate){for(let i=0;i<200;i++){const s=await window.AionNative.status();if(s&&predicate(s))return s;await wait(10);}throw new Error('Native state transition timed out.');}
    try{
        const api=window.AionNative;
        assert(document.querySelectorAll('.loop').length===10,'Ten native loop controls are present');
        assert(!document.querySelector('script[src="native-input.js"]'),'Compatibility PCM transport is not loaded');
        document.getElementById('master').value='0';
        if(window.AionNativeTestPhysical)await api.devices();
        const testSeconds=window.AionInputTestSeconds||1.5,idleSeconds=window.AionNativeIdleSeconds||0;
        const configuration={synthetic:!window.AionNativeTestPhysical,gain:0,tracks:2,seconds:Math.max(10,Math.ceil(testSeconds+idleSeconds+2))};
        const opened=await api.open(configuration);
        assert(opened.web_audio===false&&opened.transport==='native-duplex','Native audio opens without a browser audio graph');
        if(window.AionNativeTestPhysical)assert(opened.info.host==='ASIO','Requested ASIO duplex driver is active');
        assert(document.querySelector('#header .rainbow-text')&&document.querySelector('#startOverlay .rainbow-text'),'Original ASCII logos are preserved');
        assert(getComputedStyle(document.body).fontFamily.includes('Courier New'),'Original terminal stylesheet is active');
        assert(!window.AudioEngine&&!window.LoopManager,'Original browser audio runtime is absent');
        assert(![...document.querySelectorAll('*')].some(el=>[...el.attributes].some(a=>a.name.startsWith('on'))),'Original inline audio handlers are removed');
        assert(document.getElementById('btnSaveProj').disabled&&document.getElementById('loop-pan-slider-0').disabled,'Unmigrated project save and pan controls are disabled');
        await until(s=>s.tracks.length===2);
        const action=document.getElementById('loop-action-1');
        action.click();await until(s=>s.tracks[1].state==='recording'&&!action.disabled);await wait(100);
        assert(action.textContent==='FINISH','Original loop action changes to FINISH');
        action.click();await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        assert(action.textContent==='STOP','Original finish button starts native loop playback');
        action.click();await until(s=>s.tracks[1].state==='stopped'&&!action.disabled);
        const mode=document.getElementById('action-mode');mode.value='overdub';mode.dispatchEvent(new Event('change'));
        action.click();await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        action.click();await until(s=>s.tracks[1].state==='overdubbing'&&!action.disabled);
        action.click();await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        mode.value='substitute';mode.dispatchEvent(new Event('change'));action.click();await until(s=>s.tracks[1].state==='substituting'&&!action.disabled);
        action.click();await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        mode.value='normal';mode.dispatchEvent(new Event('change'));
        checks.push('Original loop action drives record, finish/play, stop, overdub and substitute in Rust');
        document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'2',bubbles:true}));await until(s=>s.tracks[1].state==='stopped'&&!action.disabled);
        checks.push('Original number-key loop control uses native transport');
        await api.command('clear',1);await until(s=>s.tracks[1].state==='empty');
        const start=await until(s=>s.frames>0);
        assert(start.callback_frames>0,'Native output callback is running');
        await api.command('monitor',0,1);
        await api.command('record',0);
        const recording=await until(s=>s.tracks[0].state==='recording');
        await wait(200);
        const blockedAt=performance.now();while(performance.now()-blockedAt<250){}
        await wait(Math.max(200,testSeconds*1000-450));
        if(idleSeconds){api.setPolling(false);await wait(idleSeconds*1000);api.setPolling(true);}
        await api.command('finish',0);
        const captured=await until(s=>s.tracks[0].state==='stopped');
        assert(captured.tracks[0].length>opened.info.sample_rate*.5,'Recording continues while the UI thread is blocked');
        assert(captured.frames-recording.frames>opened.info.sample_rate*.5,'Native sample clock advances independently of UI rendering');
        if(idleSeconds)assert(captured.tracks[0].length>opened.info.sample_rate*idleSeconds,'Native transport survives prolonged absence of UI polling');
        await api.command('play',0);await until(s=>s.tracks[0].state==='playing');
        await wait(50);const playing=await api.status();assert(playing.tracks[0].position>0,'Native loop playback advances');
        await api.command('mute',0,1);await until(s=>s.tracks[0].muted);checks.push('Native mute updates without a browser gain node');
        await api.command('gain',0,.5);await api.command('feedback',0,.6);
        await api.command('overdub',0);await until(s=>s.tracks[0].state==='overdubbing');await wait(60);await api.command('finish',0);await until(s=>s.tracks[0].state==='stopped');checks.push('Native overdub completes');
        await api.command('play',0);await until(s=>s.tracks[0].state==='playing');await api.command('substitute',0);await until(s=>s.tracks[0].state==='substituting');await wait(60);await api.command('finish',0);await until(s=>s.tracks[0].state==='stopped');checks.push('Native substitute completes');
        finalStatus=await api.status();
        assert(finalStatus.tracks[0].length===captured.tracks[0].length,'Punch operations retain the loop length');
        assert(finalStatus.output_peak===0,'Test master output stays muted');
        assert(!finalStatus.underruns&&!finalStatus.discontinuities&&!finalStatus.stream_errors&&!finalStatus.command_errors&&!finalStatus.capture.dropped_frames&&!finalStatus.capture.errors&&!finalStatus.capture.nonfinite_samples&&!finalStatus.capture.malformed_blocks,'Native duplex reports no input gaps or stream errors');
        await api.command('clear',0);await until(s=>s.tracks[0].state==='empty');checks.push('Native clear releases the take');
        await api.close();checks.push('Native close releases both streams');
        const reopened=await api.open(configuration);assert(reopened.token!==opened.token,'Reopened native audio uses a fresh owner token');
        const fresh=await until(s=>s.frames>0);assert(fresh.tracks.every(track=>track.length===0),'Both streams reopen with a fresh native session');
        await api.close();
        result={kind:'self_test',ok:true,checks,opened,status:finalStatus,audioSaved:false,uiBlockMs:250,idleSeconds};
    }catch(error){result={kind:'self_test',ok:false,checks,error:String(error),stack:error.stack,status:finalStatus};}
    finally{window.AionNative.setPolling(true);try{await window.AionNative.close();}catch(_){}window.ipc.postMessage(JSON.stringify(result));}
});
