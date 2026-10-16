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
        assert(!document.getElementById('btnSaveProj').disabled&&document.getElementById('loop-pan-slider-0').disabled,'Native save is enabled while unmigrated pan remains disabled');
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
        await api.command('stop',1);await until(s=>s.tracks[1].state==='stopped'&&!action.disabled);
        const punchButton=name=>[...document.querySelectorAll('#loop-1 button')].find(b=>b.textContent===name);
        assert(!punchButton('UNDO').disabled,'Finished native overdub enables original UNDO');
        const overdubHistory=(await api.status()).tracks[1].undo;
        await punchButton('UNDO').onclick();await until(s=>s.tracks[1].redo===1&&s.tracks[1].undo===overdubHistory-1);
        await punchButton('REDO').onclick();await until(s=>s.tracks[1].redo===0&&s.tracks[1].undo===overdubHistory);
        checks.push('Original UNDO and REDO restore a native overdub pass');
        await api.command('play',1);await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        mode.value='substitute';mode.dispatchEvent(new Event('change'));action.click();await until(s=>s.tracks[1].state==='substituting'&&!action.disabled);
        action.click();await until(s=>s.tracks[1].state==='playing'&&!action.disabled);
        mode.value='normal';mode.dispatchEvent(new Event('change'));
        checks.push('Original loop action drives record, finish/play, stop, overdub and substitute in Rust');
        document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'2',bubbles:true}));await until(s=>s.tracks[1].state==='stopped'&&!action.disabled);
        const substituteHistory=(await api.status()).tracks[1].undo;
        await punchButton('UNDO').onclick();await until(s=>s.tracks[1].redo===1&&s.tracks[1].undo===substituteHistory-1);
        await punchButton('REDO').onclick();await until(s=>s.tracks[1].redo===0&&s.tracks[1].undo===substituteHistory);
        checks.push('Original UNDO and REDO restore a native substitute pass');
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
        await api.command('record',1);const parallel=await until(s=>s.tracks[1].state==='recording');
        const untracked=await api.command('overdub',0);await until(s=>s.tracks[0].state==='overdubbing');await wait(40);
        assert(untracked.undo_recorded===false&&document.getElementById('error').textContent.includes('without undo'),'Concurrent capture explicitly reports unavailable punch undo in the original interface');
        const continued=await api.status();assert(continued.tracks[1].state==='recording'&&continued.tracks[1].length>parallel.tracks[1].length,'Another loop keeps capturing during a punch without history');
        await api.command('finish',0);await until(s=>s.tracks[0].state==='stopped');await api.command('finish',1);await until(s=>s.tracks[1].state==='stopped');await api.command('clear',1);await until(s=>s.tracks[1].state==='empty');
        await api.command('play',0);await until(s=>s.tracks[0].state==='playing');
        await api.command('overdub',0);await until(s=>s.tracks[0].state==='overdubbing');await wait(60);await api.command('finish',0);await until(s=>s.tracks[0].state==='stopped');checks.push('Native overdub completes');
        await api.command('play',0);await until(s=>s.tracks[0].state==='playing');await api.command('substitute',0);await until(s=>s.tracks[0].state==='substituting');await wait(60);await api.command('finish',0);await until(s=>s.tracks[0].state==='stopped');checks.push('Native substitute completes');
        if(window.AionNativeImportFile){
            await api.command('play',0);const before=await until(s=>s.tracks[0].state==='playing');
            const imported=await api.importWave(1,window.AionNativeImportFile);const after=await until(s=>s.tracks[1].state==='stopped'&&s.tracks[1].length===imported.frames);
            assert(after.tracks[0].state==='playing'&&after.frames>before.frames,'Prepared WAV commits while the requested audio device continues playback');
            if(imported.resampled){assert(imported.source_sample_rate!==imported.sample_rate,'Different-rate WAV is converted by the Rust worker');assert(document.getElementById('project-status').textContent.includes(`${imported.source_sample_rate} → ${imported.sample_rate} Hz`),'Original project panel reports native sample-rate conversion');}
            await api.command('stop',0);await until(s=>s.tracks[0].state==='stopped');
        }
        finalStatus=await api.status();
        assert(finalStatus.tracks[0].length===captured.tracks[0].length,'Punch operations retain the loop length');
        assert(finalStatus.output_peak===0,'Test master output stays muted');
        assert(!finalStatus.underruns&&!finalStatus.discontinuities&&!finalStatus.stream_errors&&!finalStatus.command_errors&&!finalStatus.capture.dropped_frames&&!finalStatus.capture.errors&&!finalStatus.capture.nonfinite_samples&&!finalStatus.capture.malformed_blocks,'Native duplex reports no input gaps or stream errors');
        let savedProject=null,importedFrames=0;
        if(!window.AionNativeTestPhysical){
            document.getElementById('projectName').value='AionTest';document.querySelector('#loop-0 .track-name').value='Synthetic voice';document.querySelector('#loop-0 .track-name').dispatchEvent(new Event('input'));
            const wave=await api.exportWave(0);assert(wave.exported&&wave.file.endsWith('.wav'),'Original loop SAVE exports a native stereo WAV');
            const wavePath=window.AionNativeImportFile||`${wave.directory}/${wave.file}`;
            await api.command('play',0);const beforeImport=await until(s=>s.tracks[0].state==='playing');
            let imported;
            if(innerWidth>0&&innerHeight>0){
                const importCard=document.getElementById('loop-1');importCard.scrollIntoView({block:'center',behavior:'instant'});await wait(50);const rect=importCard.getBoundingClientRect();
                let dropPoint=null;for(let y=Math.max(1,rect.top+2);y<Math.min(innerHeight-1,rect.bottom);y+=8){for(let x=Math.max(1,rect.left+2);x<Math.min(innerWidth-1,rect.right);x+=32){if(document.elementFromPoint(x,y)?.closest('.loop')===importCard){dropPoint={x,y};break;}}if(dropPoint)break;}
                if(!dropPoint)throw new Error(`No visible loop drop target: ${JSON.stringify({rect:rect.toJSON(),width:innerWidth,height:innerHeight})}`);
                imported=await api.dropFile([wavePath],dropPoint.x,dropPoint.y);checks.push('Visible original loop drop target routes the WAV to Rust');
            }else{
                // Hidden WebKitGTK has a zero-sized viewport. Exercise its actual
                // path controls rather than pretending to hit an invisible card.
                document.getElementById('import-path').value=wavePath;document.getElementById('import-track').value='1';imported=await document.getElementById('import-wave').onclick();checks.push('Native WAV path controls import without browser audio decoding');
            }
            importedFrames=imported.frames;
            const afterImport=await until(s=>s.tracks[1].state==='stopped'&&s.tracks[1].length===importedFrames);
            assert(imported.imported&&afterImport.tracks[0].state==='playing'&&afterImport.frames>beforeImport.frames,'WAV import runs through Rust while another loop keeps playing');
            assert(document.getElementById('loop-action-1').textContent==='PLAY','Original imported loop action shows PLAY');
            let rejected=false;try{await api.request('engine_project_import',{token:opened.token,track:0,path:wavePath});}catch(_){rejected=true;}assert(rejected,'Import into a playing loop is rejected');
            await api.command('stop',0);await until(s=>s.tracks[0].state==='stopped');
            const editButton=name=>[...document.querySelectorAll('#loop-1 button')].find(b=>b.textContent===name);
            assert(!editButton('NORM').disabled&&editButton('UNDO').disabled,'Original NORM is enabled for stopped takes and empty history disables UNDO');
            await editButton('NORM').onclick();await until(s=>s.tracks[1].undo===1);
            assert(!editButton('UNDO').disabled,'Rust normalization enables original UNDO');
            await editButton('UNDO').onclick();await until(s=>s.tracks[1].redo===1&&s.tracks[1].undo===0);
            await editButton('REDO').onclick();await until(s=>s.tracks[1].undo===1&&s.tracks[1].redo===0);
            checks.push('Original NORM, UNDO and REDO controls edit native PCM without WebAudio');
            const globalUndo=document.getElementById('btnGlobalUndo'),globalRedo=document.getElementById('btnGlobalRedo');
            assert(!globalUndo.disabled&&globalUndo.textContent==='UNDO L2','Original master UNDO names the latest native loop');
            const localCount=(await api.status()).tracks[1].undo;
            await globalUndo.onclick();await until(s=>s.tracks[1].undo===localCount-1&&s.history.redo_track===1);
            assert(globalRedo.textContent==='REDO L2'&&!globalRedo.disabled,'Global undo updates the original REDO target');
            await globalRedo.onclick();await until(s=>s.tracks[1].undo===localCount&&s.history.undo_track===1);
            document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'z',ctrlKey:true,bubbles:true,cancelable:true}));await until(s=>s.tracks[1].undo===localCount-1&&!globalRedo.disabled);
            document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'z',metaKey:true,shiftKey:true,bubbles:true,cancelable:true}));await until(s=>s.tracks[1].undo===localCount&&!globalUndo.disabled);
            checks.push('Ctrl/Cmd+Z and Shift+Z share Rust global undo and redo');
            const textUndo=new KeyboardEvent('keydown',{key:'z',ctrlKey:true,bubbles:true,cancelable:true});document.getElementById('projectName').dispatchEvent(textUndo);
            assert(!textUndo.defaultPrevented&&(await api.status()).tracks[1].undo===localCount,'Text-field undo stays in the text field');
            await api.command('play',1);await until(s=>s.tracks[1].state==='playing');assert(globalUndo.disabled&&globalUndo.textContent==='UNDO L2','Playing global target is identified but disabled until stopped');
            await api.command('stop',1);await until(s=>s.tracks[1].state==='stopped'&&!globalUndo.disabled);

            savedProject=await api.saveProject();assert(savedProject.saved&&savedProject.project.endsWith('.aion'),'Original project save writes a native Rust project');
            const list=await api.projects();assert(list.projects.includes(savedProject.project),'Native saved project is listed for reopening');
        }
        await api.command('clear',0);await until(s=>s.tracks[0].state==='empty');checks.push('Native clear releases the take');
        await api.close();checks.push('Native close releases both streams');
        const reopened=await api.open(configuration);assert(reopened.token!==opened.token,'Reopened native audio uses a fresh owner token');
        const fresh=await until(s=>s.frames>0);assert(fresh.tracks.every(track=>track.length===0),'Both streams reopen with a fresh native session');
        assert(document.getElementById('btnGlobalUndo').disabled&&document.getElementById('btnGlobalRedo').disabled,'Closing audio clears both original global history controls');
        await api.close();
        if(savedProject){
            await api.loadProject(savedProject.project);const loaded=await api.open(configuration);const restored=await until(s=>s.frames>0);
            assert(loaded.names[0]==='Synthetic voice'&&document.querySelector('#loop-0 .track-name').value==='Synthetic voice','Native project restores original loop names');
            assert(restored.tracks[0].length===captured.tracks[0].length&&restored.tracks[0].state==='stopped'&&restored.tracks[0].muted,'Native project restores takes stopped with saved mute');
            assert(document.getElementById('loop-vol-slider-0').value==='0.5'&&document.getElementById('loop-fbk-slider-0').value==='0.6','Native project restores gain and feedback controls');
            assert(restored.tracks[1].length===importedFrames&&restored.tracks[1].state==='stopped','Imported WAV persists in native project save/reopen');
            assert(document.getElementById('monitor').textContent==='MON OFF','Restored project starts with monitoring off');await api.close();
        }
        result={kind:'self_test',ok:true,checks,opened,status:finalStatus,audioSaved:false,syntheticProjectSaved:Boolean(savedProject),uiBlockMs:250,idleSeconds};
    }catch(error){result={kind:'self_test',ok:false,checks,error:String(error),stack:error.stack,status:finalStatus};}
    finally{window.AionNative.setPolling(true);try{await window.AionNative.close();}catch(_){}window.ipc.postMessage(JSON.stringify(result));}
});
