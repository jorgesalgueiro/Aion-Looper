// Opt-in physical microphone test. Audio stays in memory and master output is muted.
document.addEventListener('DOMContentLoaded', () => setTimeout(async () => {
    const checks = [];
    function assert(condition,message) { if (!condition) throw new Error(message); checks.push(message); }
    const wait = ms => new Promise(resolve => setTimeout(resolve,ms));
    let result;
    const diagnostics = {audioSaved:false};
    let heartbeatAt = performance.now(), maxUiGap = 0;
    const heartbeat = setInterval(()=>{const now=performance.now();maxUiGap=Math.max(maxUiGap,now-heartbeatAt);heartbeatAt=now;},10);
    try {
        window.alert = message => { throw new Error(message); };
        const addInput = InputManager.addInputTrack;
        InputManager.addInputTrack = async () => {};
        document.getElementById('btn-start').click();
        for (let i=0;i<600 && document.getElementById('startOverlay').style.display !== 'none';i++) await wait(10);
        InputManager.addInputTrack = addInput;
        assert(state.audioContext?.state === 'running','Audio context running');
        state.masterGain.gain.cancelScheduledValues(0);
        state.masterGain.gain.setValueAtTime(0,state.audioContext.currentTime);
        state.syncEnabled = false;
        await InputManager.addInputTrack();
        const input = state.inputs[0], track = input?.stream?.getAudioTracks()[0];
        assert(track?.readyState === 'live',`Physical microphone opened: ${InputManager.lastInputError || ''}`);
        assert(!input.monitor,'MON starts off');
        const settings = track.getSettings();
        diagnostics.settings = settings;
        const devices = await window.RustLooper.input.devices();
        assert(devices.some(device => device.kind === 'audioinput' && device.deviceId === settings.deviceId),'Selected microphone appears in device selector');
        InputManager.toggleMonitor(0);
        await wait(600); // Let initial worklet prefill and its status report settle.
        const beforeRecording = {...input.stream.nativeStatus};
        diagnostics.beforeRecording = beforeRecording;
        assert(track.readyState === 'live' && beforeRecording.frames > 0 && !beforeRecording.error,'Native capture is flowing before recording');
        await LoopManager.startRecording(0);
        await wait(1500);
        diagnostics.recordingEnd = {...input.stream.nativeStatus};
        diagnostics.stopTime = state.audioContext.currentTime;
        diagnostics.beforeStopUiGap = maxUiGap;
        LoopManager.stopRecording(true);
        for (let i=0;i<600 && (state.isRecording || !state.loops[0].audioBuffer);i++) await wait(10);
        const buffer = state.loops[0].audioBuffer;
        assert(buffer?.length > 0,'Physical input recorded through original loop controls');
        let peak = 0;
        for (let c=0;c<buffer.numberOfChannels;c++) for (const sample of buffer.getChannelData(c)) {
            if (!Number.isFinite(sample)) throw new Error('Non-finite captured audio');
            peak = Math.max(peak,Math.abs(sample));
        }
        assert(true,'Captured PCM is finite');
        await wait(550); // Include the final worklet counter report.
        const capture = {...input.stream.nativeStatus};
        Object.assign(diagnostics,{capture,peak,frames:buffer.length,maxUiGap,trackState:track.readyState,inputError:InputManager.lastInputError});
        assert(capture.frames > 0 && !capture.error && track.readyState === 'live','Rust capture remains live after recording');
        const quality = {reportedUnderruns:capture.underruns-beforeRecording.underruns,reportedOverruns:capture.overruns-beforeRecording.overruns};
        diagnostics.quality = quality;
        state.loops[0].clear();
        await InputManager.changeDevice(0,settings.deviceId);
        const replacement = state.inputs[0].stream.getAudioTracks()[0];
        assert(track.readyState === 'ended' && replacement.readyState === 'live','Changing device releases old capture and opens selected microphone');
        InputManager.removeInputTrack(0);
        assert(replacement.readyState === 'ended','Removing input releases microphone');
        assert(quality.reportedUnderruns === 0 && quality.reportedOverruns === 0,'No new reported transport gaps during the recording test');
        result = {kind:'self_test',ok:true,checks,...diagnostics,quality,device:capture.device,capture,settings,peak,frames:buffer.length,audioSaved:false};
    } catch(error) {
        result = {kind:'self_test',ok:false,checks,...diagnostics,inputs:window.RustLooper.input.status(),inputError:InputManager.lastInputError,error:String(error),stack:error.stack};
    } finally {
        clearInterval(heartbeat);
        for (const input of [...state.inputs]) for (const track of input.stream?.getTracks() || []) track.stop();
        window.ipc.postMessage(JSON.stringify(result));
    }
},100));
