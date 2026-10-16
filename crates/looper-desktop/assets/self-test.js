// Real WebView integration: no microphone access and no audible output.
document.addEventListener('DOMContentLoaded', () => setTimeout(async () => {
    const checks = [];
    function assert(condition, message) { if (!condition) throw new Error(message); checks.push(message); }
    function snapshot(buffer) {
        return Array.from({ length: buffer.numberOfChannels }, (_, channel) => buffer.getChannelData(channel).slice());
    }
    function matches(buffer, samples) {
        return buffer.numberOfChannels === samples.length && samples.every((channel, index) =>
            buffer.length === channel.length && channel.every((sample, frame) => Object.is(sample, buffer.getChannelData(index)[frame])));
    }
    function checkHistory(loop, before, operation) {
        const after = snapshot(loop.audioBuffer);
        loop.undo();
        assert(matches(loop.audioBuffer, before), `${operation} undo restores every sample`);
        loop.redo();
        assert(matches(loop.audioBuffer, after), `${operation} redo restores every sample`);
    }
    try {
        window.alert = message => checks.push(`UI notification: ${message}`);
        assert(!!document.getElementById('btn-start'), 'Original start control loaded');
        assert(getComputedStyle(document.body).backgroundColor === 'rgb(5, 5, 5)', 'Original stylesheet applied');
        assert(window.isSecureContext, 'Secure context for AudioWorklet and device APIs');
        assert(typeof window.SoundTouch === 'function', 'Offline SoundTouch module loaded');
        window.RustLooper.nativeMidiTestMode=true;
        let midiGuarded=false;
        try {await window.RustLooper.request('midi_open',{synthetic:true});}catch(_){midiGuarded=true;}
        assert(midiGuarded,'Native MIDI input requires workstation authorization');
        const inputStartup = InputManager.addInputTrack;
        InputManager.addInputTrack = async () => {};
        document.getElementById('btn-start').click();
        for (let i = 0; i < 600 && document.getElementById('startOverlay').style.display !== 'none'; i++) {
            await new Promise(resolve => setTimeout(resolve, 10));
        }
        InputManager.addInputTrack = inputStartup;
        assert(document.getElementById('startOverlay').style.display === 'none', 'Actual start button opens the workstation');
        assert(!!state.audioContext && state.audioContext.state === 'running', 'Audio engine started');
        state.masterGain.gain.cancelScheduledValues(0);
        state.masterGain.gain.setValueAtTime(0, state.audioContext.currentTime);
        assert(state.loops.length === 10, 'Ten original loop controls initialized');
        assert(!!document.getElementById('loop-0'), 'Loop UI rendered');
        assert(typeof state.audioContext.audioWorklet.addModule === 'function', 'AudioWorklet available');
        state.syncEnabled = false;
        const buffer = state.audioContext.createBuffer(2, 4410, state.audioContext.sampleRate);
        buffer.getChannelData(0).fill(0.125);
        buffer.getChannelData(1).fill(-0.25);
        const file = new File([AudioEngine.bufferToWAV(buffer)], 'test.wav', { type: 'audio/wav' });
        const browserDecoded = await state.audioContext.decodeAudioData(await file.arrayBuffer(), () => {});
        const nativeDecoded = await state.audioContext.decodeAudioData(await file.arrayBuffer());
        assert(matches(nativeDecoded, snapshot(browserDecoded)), 'Native PCM16 import matches browser decoding sample for sample');
        for (const [bits,tag,channels,pattern] of [[24,1,1,[8388607,-8388608,1048577,-1048577,0,1,-1]],[32,3,2,[1.5,-1.25,.125,-.25,0,-0]]]) {
            const samples = Array.from({length: pattern.length * 256}, (_, i) => pattern[i % pattern.length]);
            const size = samples.length * bits / 8;
            const bytes = new Uint8Array(44 + size + size % 2), view = new DataView(bytes.buffer);
            function text(offset, value) { for (let i = 0; i < value.length; i++) bytes[offset+i] = value.charCodeAt(i); }
            text(0,'RIFF'); view.setUint32(4,bytes.length-8,true); text(8,'WAVEfmt '); view.setUint32(16,16,true);
            view.setUint16(20,tag,true); view.setUint16(22,channels,true); view.setUint32(24,state.audioContext.sampleRate,true);
            view.setUint32(28,state.audioContext.sampleRate*channels*bits/8,true); view.setUint16(32,channels*bits/8,true);
            view.setUint16(34,bits,true); text(36,'data'); view.setUint32(40,size,true);
            samples.forEach((sample,index) => {
                if (tag === 3) view.setFloat32(44 + index*4,sample,true);
                else for (let byte = 0; byte < 3; byte++) bytes[44 + index*3 + byte] = (sample >> (byte*8)) & 255;
            });
            const browser = await state.audioContext.decodeAudioData(bytes.buffer.slice(0), () => {});
            const native = await state.audioContext.decodeAudioData(bytes.buffer);
            assert(matches(native,snapshot(browser)), `Native ${tag === 3 ? 'float32' : 'PCM24'} import matches browser decoding sample for sample${matches(native,snapshot(browser)) ? '' : ' expected=' + JSON.stringify(snapshot(browser).map(c=>Array.from(c))) + ' actual=' + JSON.stringify(snapshot(native).map(c=>Array.from(c)))}`);
        }
        for (const [bits,valid,tag,channels] of [[16,16,1,1],[24,24,1,2],[32,24,1,2],[32,16,1,1],[32,32,3,2]]) {
            const pattern=tag===3?[1.5,-1.25,0,-0]:[2**(valid-1)-1,-(2**(valid-1)),valid===16?8784:1048577,-1];
            const samples=Array.from({length:pattern.length*256},(_,i)=>pattern[i%pattern.length]);
            const size=samples.length*bits/8,bytes=new Uint8Array(68+size),view=new DataView(bytes.buffer);
            function text(offset,value) { for(let i=0;i<value.length;i++) bytes[offset+i]=value.charCodeAt(i); }
            text(0,'RIFF');view.setUint32(4,bytes.length-8,true);text(8,'WAVEfmt ');view.setUint32(16,40,true);
            view.setUint16(20,0xfffe,true);view.setUint16(22,channels,true);view.setUint32(24,state.audioContext.sampleRate,true);
            view.setUint32(28,state.audioContext.sampleRate*channels*bits/8,true);view.setUint16(32,channels*bits/8,true);
            view.setUint16(34,bits,true);view.setUint16(36,22,true);view.setUint16(38,valid,true);view.setUint32(40,channels===1?4:3,true);
            bytes.set([tag,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71],44);
            text(60,'data');view.setUint32(64,size,true);
            samples.forEach((sample,index)=>{
                if(tag===3) view.setFloat32(68+index*4,sample,true);
                else for(let byte=0;byte<bits/8;byte++) bytes[68+index*bits/8+byte]=((sample*2**(bits-valid))>>>(byte*8))&255;
            });
            let browser;
            try {
                browser=await state.audioContext.decodeAudioData(bytes.buffer.slice(0),()=>{});
            } catch(error) {
                if (!['EncodingError','NotSupportedError'].includes(error.name)) throw error;
                // Some WebView2 decoders reject valid precision-in-container WAVs.
                // Unknown calibration must preserve that browser fallback result.
                let rejected=false;
                try { await state.audioContext.decodeAudioData(bytes.buffer); }
                catch(nativeError) { rejected=nativeError.name===error.name; }
                assert(rejected,`Browser-unsupported extensible ${valid}/${bits} tag ${tag} preserves decoder rejection`);
                continue;
            }
            window.RustLooper.lastNativeOperation=null;
            const native=await state.audioContext.decodeAudioData(bytes.buffer);
            const importedNatively=window.RustLooper.lastNativeOperation==='decode_wav';
            if(bits===valid) assert(importedNatively,`Extensible ${valid}/${bits} tag ${tag} imports through Rust`);
            else checks.push(`Extensible ${valid}/${bits} tag ${tag} uses ${importedNatively?'Rust':'browser calibration fallback'}`);
            assert(matches(native,snapshot(browser)),`Extensible ${valid}/${bits} tag ${tag} matches every browser sample`);
        }
        await LoopManager.loadAudioToLoop(0, file);
        const loop = state.loops[0];
        assert(!!loop.audioBuffer && loop.audioBuffer.length > 0, 'WAV import populated a loop');
        assert(window.RustLooper.lastNativeOperation === 'decode_wav', 'WAV import executes native Rust decoding');
        await loop.normalize();
        assert(window.RustLooper.lastNativeOperation === 'normalize', 'Normalize button path executed Rust DSP');
        const peak = Math.max(...loop.audioBuffer.getChannelData(1).map(Math.abs));
        assert(Math.abs(peak - 1) < 0.00001, 'Native normalization returned correct PCM');
        const expectedExport = new Uint8Array(AudioEngine.bufferToWAV(loop.audioBuffer));
        const originalDownload = ProjectManager.downloadWAV;
        let exported;
        ProjectManager.downloadWAV = (data, filename) => { exported = { data: new Uint8Array(data), filename }; };
        document.getElementById('loop-wave-0').click();
        for (let i = 0; i < 300 && !exported; i++) {
            await new Promise(resolve => setTimeout(resolve, 10));
        }
        ProjectManager.downloadWAV = originalDownload;
        assert(window.RustLooper.lastNativeOperation === 'encode_wav' && !!exported, 'Waveform export control executes Rust encoding');
        assert(exported.data.length === expectedExport.length && expectedExport.every((byte, index) => byte === exported.data[index]), 'Native loop export matches original WAV bytes exactly');
        assert(exported.filename.endsWith('.wav'), 'Native export retains the WAV filename');
        await loop.play();
        assert(loop.state === 'playing', 'Loop playback control works');
        LoopManager.toggleMute(0);
        assert(loop.muted, 'Loop mute control works');
        loop.stop();
        for (let i = 0; i < 200 && loop.state !== 'stopped'; i++) {
            await new Promise(resolve => setTimeout(resolve, 10));
        }
        assert(loop.state === 'stopped', 'Loop stop control works');
        loop.clear();
        assert(loop.state === 'empty' && !loop.audioBuffer, 'Loop clear control works');
        window.RustLooper.nativeInputTestMode = true;
        await InputManager.addInputTrack();
        assert(state.inputs.length === 1 && state.inputs[0].deviceId.startsWith('native:'), 'Native input attaches through the existing MIC/LINE path');
        assert(!state.inputs[0].monitor, 'Native input keeps MON off by default');
        const nativeDevices = await window.RustLooper.input.devices();
        assert(nativeDevices.length === 1 && nativeDevices[0].deviceId === state.inputs[0].deviceId, 'Native device selector uses host and device identity');
        document.querySelector('.input-track-row button[onclick*="InputManager.toggleMonitor(0)"]').click();
        assert(state.inputs[0].monitor, 'Existing MON control routes native input to the recording bus');
        await LoopManager.startRecording(1);
        await new Promise(resolve => setTimeout(resolve, 200));
        LoopManager.stopRecording(true);
        for (let i = 0; i < 300 && (!state.loops[1].audioBuffer || state.isRecording); i++) {
            await new Promise(resolve => setTimeout(resolve, 10));
        }
        assert(!!state.loops[1].audioBuffer && state.loops[1].audioBuffer.length > 0, 'Recording through the actual AudioWorklet works');
          assert(window.RustLooper.lastNativeOperation === 'finalize_recording', 'First recording extraction, fades and DC removal finalized through Rust DSP');
        function rms(buffer,channel) {
            const samples = buffer.getChannelData(channel);
            return Math.sqrt(samples.reduce((sum,sample)=>sum+sample*sample,0)/samples.length);
        }
        assert(rms(state.loops[1].audioBuffer,0) > .01 && Math.abs(rms(state.loops[1].audioBuffer,1)/rms(state.loops[1].audioBuffer,0)-.5) < .005, 'Native input recording preserves stereo channel balance');
        for (const substitute of [false, true]) {
            const before = snapshot(state.loops[1].audioBuffer);
            window.RustLooper.lastNativeOperation = null;
            await LoopManager.startOverdub(1, substitute);
            assert(state.loops[1].state === (substitute ? 'substituting' : 'overdubbing'), `${substitute ? 'Substitute' : 'Overdub'} control starts`);
            await new Promise(resolve => setTimeout(resolve, 120));
            LoopManager.stopOverdub();
            for (let i = 0; i < 300 && state.isRecording; i++) {
                await new Promise(resolve => setTimeout(resolve, 10));
            }
            assert(window.RustLooper.lastNativeOperation === 'finalize_punch', `${substitute ? 'Substitute' : 'Overdub'} DC removal, latency correction, fades and mix finalized through Rust DSP`);
            checkHistory(state.loops[1], before, substitute ? 'Substitute' : 'Overdub');
        }
        const oldInputTrack = state.inputs[0].stream.getAudioTracks()[0];
        await InputManager.changeDevice(0,nativeDevices[0].deviceId);
        assert(oldInputTrack.readyState === 'ended' && window.RustLooper.input.status().length === 1, 'Native device changes release the previous stream');
        const channelRms = [];
        for (const mode of ['left','right']) {
            InputManager.setChannelMode(0,mode);
            await LoopManager.startRecording(2);
            await new Promise(resolve=>setTimeout(resolve,160));
            LoopManager.stopRecording(true);
            for (let i=0;i<300 && state.isRecording;i++) await new Promise(resolve=>setTimeout(resolve,10));
            channelRms.push(rms(state.loops[2].audioBuffer,0));
            state.loops[2].clear();
        }
        assert(channelRms[0] > .01 && Math.abs(channelRms[1]/channelRms[0]-.5) < .1, 'Existing L/R selectors route the native stereo channels');
        const extended = state.loops[1], beforeAppend = snapshot(extended.audioBuffer), appendLength=extended.audioBuffer.length;
        window.RustLooper.lastNativeOperation = null;
        document.querySelector('#loop-1 button[onclick*=".multiply()"] ').click();
        for (let i=0;i<300 && extended.state !== 'multiplying';i++) await new Promise(resolve=>setTimeout(resolve,10));
        assert(extended.state === 'multiplying','Actual MULT control starts input capture on a playing loop');
        await new Promise(resolve=>setTimeout(resolve,180));
        document.querySelector('#loop-1 button[onclick*=".multiply()"] ').click();
        for (let i=0;i<600 && state.isRecording;i++) await new Promise(resolve=>setTimeout(resolve,10));
        assert(window.RustLooper.lastNativeOperation === 'append_capture' && extended.audioBuffer.length > appendLength,'Interactive MULT appends captured audio through Rust');
        assert(beforeAppend.every((channel,c)=>channel.every((sample,i)=>Object.is(sample,extended.audioBuffer.getChannelData(c)[i]))),'Interactive MULT preserves the original take sample for sample');
        checkHistory(extended,beforeAppend,'Interactive MULT');
        const nativeStream = state.inputs[0].stream;
        InputManager.removeInputTrack(0);
        assert(nativeStream.getAudioTracks()[0].readyState === 'ended' && window.RustLooper.input.status().length === 0, 'Removing native input releases capture and its transport');
        state.loops[1].stop();
        await LoopManager.loadAudioToLoop(0, file);
        const beforeMerge = snapshot(state.loops[0].audioBuffer);
        window.confirm = () => false;
        window.RustLooper.lastNativeOperation = null;
        await LoopManager.mergeLoops(0, '1');
        assert(window.RustLooper.lastNativeOperation === 'mix', 'Loop merge finalized through Rust DSP');
        assert(!matches(state.loops[0].audioBuffer, beforeMerge), 'Loop merge changes destination PCM');
        checkHistory(state.loops[0], beforeMerge, 'Merge');
        const beforeMultiply = snapshot(state.loops[0].audioBuffer);
        const oldLength = state.loops[0].audioBuffer.length;
        window.RustLooper.lastNativeOperation = null;
        document.querySelector('#loop-0 button[onclick*=".multiply()"] ').click();
        for (let i = 0; i < 300 && state.loops[0].audioBuffer.length === oldLength; i++) {
            await new Promise(resolve => setTimeout(resolve, 10));
        }
        assert(window.RustLooper.lastNativeOperation === 'repeat', 'Actual MULT button executes Rust DSP');
        assert(state.loops[0].audioBuffer.length === oldLength * 2, 'Multiply doubles the loop length');
        for (let channel = 0; channel < beforeMultiply.length; channel++) {
            const data = state.loops[0].audioBuffer.getChannelData(channel);
            assert(beforeMultiply[channel].every((sample, frame) => Object.is(sample, data[frame])
                && Object.is(sample, data[frame + oldLength])), `Multiply preserves channel ${channel} in both copies`);
        }
        checkHistory(state.loops[0], beforeMultiply, 'Multiply');
        const samplerBuffer = state.audioContext.createBuffer(2, 4, 44100);
        samplerBuffer.getChannelData(0).set([.125, -.25, .375, -.5]);
        samplerBuffer.getChannelData(1).set([-.5, .375, -.25, .125]);
        state.samplers[0].buffer = samplerBuffer;
        state.samplers[0].state = 'stopped';
        await SamplerManager.reverse(0);
        assert(window.RustLooper.lastNativeOperation === 'reverse', 'Sampler reverse executes Rust DSP');
        assert(matches(state.samplers[0].buffer, snapshot(samplerBuffer).map(channel => channel.reverse())), 'Sampler reverse preserves both channels exactly');
        await SamplerManager.normalize(0);
        assert(window.RustLooper.lastNativeOperation === 'normalize', 'Sampler normalize executes Rust DSP');
        assert(matches(state.samplers[0].buffer, snapshot(samplerBuffer).map(channel => channel.reverse().map(sample => sample * 2))), 'Sampler normalization preserves channel balance');
        assert(state.samplers[0].buffer.sampleRate === 44100, 'Sampler edits preserve the stored sample rate');
        LoopManager.addTracks();
        assert(state.loops.length === 20 && !!document.getElementById('loop-19'), 'Expansion to twenty tracks works');
        for (const tab of ['drone', 'tracker', 'live', 'loops']) {
            document.querySelector(`.main-tab-btn[onclick*="'${tab}'"]`).click();
            assert(document.getElementById(`ws-${tab}`).classList.contains('active'), `Workspace tab ${tab} works`);
        }
        LoopManager.stopAll();
        await new Promise(resolve => setTimeout(resolve,50));
        state.bpm = 127;
        state.loops[0].name = 'Native project loop';
        state.loops[0].feedback = .6;
        state.loops[0].originalBpm = 103;
        state.loops[0].dynamicRecorded = true;
        const projectSampler = state.audioContext.createBuffer(1,8,state.audioContext.sampleRate);
        projectSampler.getChannelData(0).set([.125,-.25,.375,-.5,.625,-.75,.875,-1]);
        Object.assign(state.samplers[0],{buffer:projectSampler,name:'Native one shot',speed:1.25,volume:.6,pan:3,muted:true,isLooping:false});
        const expectedLoopWav = new Uint8Array(AudioEngine.bufferToWAV(state.loops[0].audioBuffer));
        const expectedSamplerWav = new Uint8Array(AudioEngine.bufferToWAV(state.samplers[0].buffer));
        const nativeEncodesBefore = window.RustLooper.nativeWavEncodes;
        const originalCreateURL = URL.createObjectURL, originalAnchorClick = HTMLAnchorElement.prototype.click, originalPrompt = window.prompt;
        let projectBlob, projectFilename;
        URL.createObjectURL = function (blob) {
            if (blob.type === 'application/json') projectBlob = blob;
            return originalCreateURL.call(this,blob);
        };
        HTMLAnchorElement.prototype.click = function () {
            if (this.download.endsWith('.alp')) projectFilename = this.download;
            else originalAnchorClick.call(this);
        };
        window.prompt = () => 'Native round trip';
        document.getElementById('btnSaveProj').click();
        for (let i = 0; i < 1000 && !projectBlob; i++) await new Promise(resolve => setTimeout(resolve,10));
        URL.createObjectURL = originalCreateURL;
        HTMLAnchorElement.prototype.click = originalAnchorClick;
        window.prompt = originalPrompt;
        assert(!!projectBlob && projectFilename === 'Native_round_trip.alp', 'Actual SAVE control produces the original ALP project format');
        assert(window.RustLooper.nativeWavEncodes - nativeEncodesBefore === 3, 'Project save encodes both loops and sampler through Rust');
        const projectText = await projectBlob.text(), savedProject = JSON.parse(projectText);
        const savedLoop = savedProject.loops.find(loop => loop.id === 0), savedSampler = savedProject.samplers.find(sampler => sampler.id === 0);
        function sameWav(base64, expected) {
            const bytes = atob(base64);
            return bytes.length === expected.length && expected.every((byte,index) => byte === bytes.charCodeAt(index));
        }
        assert(sameWav(savedLoop.audioWavBase64,expectedLoopWav) && sameWav(savedSampler.audioWavBase64,expectedSamplerWav), 'Project WAV payloads match the original encoder byte for byte');
        assert(savedProject.loops.length === 20 && savedProject.tracker.loopColumns === 20, 'Project save retains twenty tracks and tracker layout');
        assert(Array.from(document.querySelectorAll('#btnSaveProj')).every(button => !button.disabled && !button.hasAttribute('aria-busy')), 'Project SAVE controls recover after native encoding');
        const expectedRestoredLoop = await state.audioContext.decodeAudioData(expectedLoopWav.buffer.slice(0));
        const expectedRestoredSampler = await state.audioContext.decodeAudioData(expectedSamplerWav.buffer.slice(0));
        state.loops[0].clear();
        state.samplers[0].buffer = null;
        state.bpm = 80;
        const projectInput = {files:[new File([projectText],projectFilename,{type:'application/json'})],value:projectFilename};
        await ProjectManager.load({target:projectInput});
        assert(matches(state.loops[0].audioBuffer,snapshot(expectedRestoredLoop)), 'Project reload restores loop audio without applying another seam fade');
        assert(matches(state.samplers[0].buffer,snapshot(expectedRestoredSampler)), 'Project reload restores mono sampler audio through Rust');
        assert(state.bpm === 127 && state.loops[0].feedback === .6 && state.loops[0].originalBpm === 103 && state.loops[0].dynamicRecorded, 'Project reload restores tempo, feedback and dynamic-recording metadata');
        assert(state.samplers[0].name === 'Native one shot' && state.samplers[0].speed === 1.25 && state.samplers[0].volume === .6 && state.samplers[0].pan === 3 && state.samplers[0].muted && !state.samplers[0].isLooping, 'Project reload restores sampler settings');
        assert(!ProjectManager._loading && projectInput.value === '' && document.body.style.cursor === 'default', 'Project load controls recover after the native round trip');
        // Exercise the actual export controls without starting a live recording
        // or creating downloads. Recorded-performance decoding remains the source path.
        const performance = state.audioContext.createBuffer(2,2400,state.audioContext.sampleRate);
        for (let i=0;i<performance.length;i++) {
            performance.getChannelData(0)[i]=Math.sin(i*.17)*.4;
            performance.getChannelData(1)[i]=Math.cos(i*.13)*.2;
        }
        const performanceWav = AudioEngine.bufferToWAV(performance);
        const allIntegers=AudioEngine.bufferToWAV(state.audioContext.createBuffer(1,65536,state.audioContext.sampleRate));
        const integerView=new DataView(allIntegers);
        for(let i=0;i<65536;i++) integerView.setInt16(44+i*2,i-32768,true);
        const originalIntegers=await state.audioContext.decodeAudioData(allIntegers.slice(0),()=>{});
        const nativeIntegers=await state.audioContext.decodeAudioData(allIntegers.slice(0));
        assert(matches(nativeIntegers,snapshot(originalIntegers)), 'Native PCM16 import matches every signed integer and host rounding exactly');
        const expectedPerformance = await state.audioContext.decodeAudioData(performanceWav.slice(0),()=>{});
        AudioEngine.applyFades(expectedPerformance,.01);
        const fadedPerformanceWav = new Uint8Array(AudioEngine.bufferToWAV(expectedPerformance));
        const performanceBlob = new Blob([performanceWav],{type:'audio/wav'});
        state.masterChunks=[performanceBlob];state.inputChunks=[performanceBlob];
        state.masterMimeType='audio/wav';state.inputMimeType='audio/wav';
        state.loops[0].stemChunks=[performanceBlob];
        const exportDrone = DroneSynth.instances[0];
        if (exportDrone) exportDrone.stemChunks=[performanceBlob];
        const dryExpected = state.loops.filter(loop=>loop.audioBuffer).map(loop=>new Uint8Array(AudioEngine.bufferToWAV(loop.audioBuffer)));
        const collectedExports=[],exportSongs=[];
        const savedDownload=ProjectManager.downloadWAV,savedSong=TrackerManager.saveSong;
        ProjectManager.downloadWAV=(data,name)=>collectedExports.push({bytes:new Uint8Array(data),name});
        TrackerManager.saveSong=name=>exportSongs.push(name);
        const encodesAtExport=window.RustLooper.nativeWavEncodes;
        let exportWork;
        const allExport=ProjectManager.exportAllTracks;
        ProjectManager.exportAllTracks=function(){return exportWork=allExport.call(this);};
        UIManager.updateExportButtons();
        document.getElementById('exportAllBtn').click();
        assert(!!exportWork, 'Actual EXP ALL control starts native export finalization');
        await exportWork;
        ProjectManager.exportAllTracks=allExport;
        const wetCount=3+(exportDrone?1:0),allCount=dryExpected.length+wetCount;
        assert(collectedExports.length===allCount && window.RustLooper.nativeWavEncodes-encodesAtExport===allCount, 'Dry loops, master, input and recorded stems encode through Rust');
        function equalBytes(a,b) {return a.length===b.length&&a.every((value,i)=>value===b[i]);}
        const exportDifferences = collectedExports.map((file,i)=>{
            const expected=i<dryExpected.length?dryExpected[i]:fadedPerformanceWav;
            const offset=file.bytes.findIndex((byte,j)=>byte!==expected[j]);
            return {name:file.name,length:file.bytes.length,expected:expected.length,offset,
                actualByte:file.bytes[offset],expectedByte:expected[offset]};
        }).filter(diff=>diff.offset>=0||diff.length!==diff.expected);
        assert(exportDifferences.length===0, `Bulk export PCM16 bytes and performance fades match the original encoder${exportDifferences.length?': '+JSON.stringify(exportDifferences):''}`);
        assert(!document.getElementById('exportAllBtn').disabled && !document.getElementById('exportAllBtn').textContent.includes('Processing'), 'EXP ALL controls recover after Rust exports');
        for (const [method,buttonId,label] of [['exportMasterOnly','exportMasterBtn','Master_Mix'],['exportInputOnly','exportInputBtn','Input_Only']]) {
            const original=ProjectManager[method];exportWork=null;
            ProjectManager[method]=function(){return exportWork=original.call(this);};
            document.getElementById(buttonId).click();
            assert(!!exportWork, `Actual ${label} control starts Rust WAV export`);
            await exportWork;ProjectManager[method]=original;
            const exported=collectedExports.at(-1);
            assert(exported.name.includes(`_${label}_`) && equalBytes(exported.bytes,fadedPerformanceWav), `${label} export preserves filename and original faded WAV bytes`);
        }
        assert(exportSongs.length===2 && exportSongs.every(name=>name.endsWith('.trk')), 'Master exports preserve tracker performance sidecars');
        ProjectManager.downloadWAV=savedDownload;TrackerManager.saveSong=savedSong;
        const labSource=state.audioContext.createBuffer(2,32,44100);
        for(let i=0;i<labSource.length;i++) {
            labSource.getChannelData(0)[i]=Math.sin(i*.31)*.4;
            labSource.getChannelData(1)[i]=Math.cos(i*.23)*.5;
        }
        for(const [operation,nativeOperation] of [['reverse','reverse'],['normalize','normalize'],['crop','slice']]) {
            SampleLab.rawBuffer=AudioEngine.cloneBuffer(labSource);SampleLab.processedBuffer=null;SampleLab.clearUndo();
            document.getElementById('labStart').value=.25;document.getElementById('labEnd').value=.75;
            document.getElementById('labZC').checked=false;SampleLab.updateDisplays();
            let expected=AudioEngine.cloneBuffer(labSource);
            if(operation==='reverse') expected=AudioEngine.getReversedBuffer(expected);
            if(operation==='normalize') AudioEngine.normalizeBuffer(expected);
            if(operation==='crop') expected=AudioEngine.sliceBuffer(expected,.25,.75);
            let labWork;const original=SampleLab[operation];
            SampleLab[operation]=function(){return labWork=original.call(this);};
            document.querySelector(`button[onclick="SampleLab.${operation}()"]`).click();
            await labWork;SampleLab[operation]=original;
            assert(window.RustLooper.lastNativeOperation===nativeOperation, `Sample Lab ${operation} control executes Rust DSP`);
            assert(matches(SampleLab.rawBuffer,snapshot(expected))&&SampleLab.rawBuffer.sampleRate===44100, `Sample Lab ${operation} preserves original samples and stored rate`);
            if(operation==='normalize') {
                const savedUndo=SampleLab._undoBuffer;await SampleLab.normalize();
                assert(SampleLab._undoBuffer===savedUndo&&document.getElementById('labStatus').textContent==='ALREADY MAXED.', 'Sample Lab no-op normalization preserves prior undo');
            }
            document.getElementById('labUndoBtn').click();
            assert(matches(SampleLab.rawBuffer,snapshot(labSource)), `Sample Lab ${operation} undo restores every sample`);
        }
        const stretchSource=state.audioContext.createBuffer(2,24000,48000);
        for(let i=0;i<stretchSource.length;i++) {
            const sample=Math.sin(i*440*Math.PI*2/48000)*.4;
            stretchSource.getChannelData(0)[i]=sample;stretchSource.getChannelData(1)[i]=sample*.5;
        }
        const stretchBefore=snapshot(stretchSource);
        SampleLab.rawBuffer=stretchSource;SampleLab.processedBuffer=null;
        document.getElementById('labStart').value=0;document.getElementById('labEnd').value=1;
        document.getElementById('labStretch').value=1.5;document.getElementById('labPitch').value=12;
        document.querySelector('button[onclick="SampleLab.process()"]').click();
        assert(SampleLab.isProcessing,'Actual Sample Lab PROCESS control starts asynchronous stretching');
        for(let i=0;i<1500&&SampleLab.isProcessing;i++) await new Promise(resolve=>setTimeout(resolve,10));
        assert(window.RustLooper.lastNativeOperation==='stretch'&&window.RustLooper.lastStretchEngine==='Rubber Band 4.0.0 R3', 'Sample Lab PROCESS executes the native Rubber Band R3 engine');
        const stretched=SampleLab.processedBuffer;
        assert(stretched&&stretched.length===36000&&stretched.sampleRate===48000&&stretched.numberOfChannels===2,'Native stretch preserves stereo and rate with exact requested duration');
        let crossings=0;const from=Math.floor(stretched.length/4),to=Math.floor(stretched.length*3/4);
        for(let i=from+1;i<to;i++) if(stretched.getChannelData(0)[i-1]<=0&&stretched.getChannelData(0)[i]>0) crossings++;
        const measured=crossings*48000/(to-from);
        assert(Math.abs(measured-880)<10,'Native time stretch keeps octave pitch shift independent of duration');
        assert(matches(SampleLab.rawBuffer,stretchBefore)&&!SampleLab.isProcessing,'Native stretching leaves the Lab source intact and releases processing state');
        SampleLab.stopPreview();
        SampleLab.processedBuffer=state.audioContext.createBuffer(1,8,44100);
        SampleLab.processedBuffer.getChannelData(0).set([.125,-.25,.375,-.5,.625,-.75,.875,-1]);
        const expectedLabWav=new Uint8Array(AudioEngine.bufferToWAV(SampleLab.processedBuffer));
        let labExport,labSaveWork;const labSave=SampleLab.save;
        ProjectManager.downloadWAV=(data,name)=>{labExport={bytes:new Uint8Array(data),name};};
        SampleLab.save=function(){return labSaveWork=labSave.call(this);};
        document.querySelector('button[onclick="SampleLab.save()\"]').click();
        await labSaveWork;SampleLab.save=labSave;ProjectManager.downloadWAV=savedDownload;
        assert(window.RustLooper.lastNativeOperation==='encode_wav'&&/^sampler_processed_\d+\.wav$/.test(labExport.name), 'Sample Lab SAVE WAV uses Rust and the original processed filename');
        assert(equalBytes(labExport.bytes,expectedLabWav), 'Sample Lab WAV bytes match the original encoder');
        await App.requestMIDIAccess();
        const midiAccess=state.midiAccess,midiPort=state.midiInputs[0];
        assert(midiAccess===window.RustLooper.midiAccess&&midiPort?.id==='self-test-midi','Existing MIDI setup discovers the native test port');
        assert(document.getElementById('midiStatus').textContent.includes('Synthetic native MIDI'),'Original MIDI status displays the native device');
        assert(window.RustLooper.midiAccess.nativeStatus.transport==='native-push','Native MIDI uses acknowledged push without a JavaScript polling timer');
        const savedMidiMap=state.keyMapping.midi,savedCC=state.midiCCMap,savedLearn=state.midiLearn,savedMidiStorage=localStorage.getItem('ascii_looper_midicc');
        const midiMethods=[App.handleInputPress,App.handleInputRelease,App.toggleGlobalOverdub,App.toggleGlobalSubstitute,App.toggleGlobalSus,
            TrackerManager.togglePlay,TrackerManager.selectPattern,LoopManager.stopAll,DroneSynth.handleMidi,DroneSynth.handlePitchBend];
        const midiActions=[];const savedPlaying=state.tracker.isPlaying,savedVolume=state.loops[0].volume;
        state.keyMapping.midi=new Array(33).fill(null);Object.assign(state.keyMapping.midi,{0:36,30:46,31:47,32:48});
        App.handleInputPress=(id,key)=>midiActions.push(['press',id,key]);App.handleInputRelease=(id,key)=>midiActions.push(['release',id,key]);
        App.toggleGlobalOverdub=()=>midiActions.push(['overdub']);App.toggleGlobalSubstitute=()=>midiActions.push(['substitute']);App.toggleGlobalSus=()=>midiActions.push(['sustain']);
        TrackerManager.togglePlay=()=>{state.tracker.isPlaying=true;midiActions.push(['start']);};
        TrackerManager.selectPattern=id=>midiActions.push(['pattern',id]);
        LoopManager.stopAll=()=>{state.tracker.isPlaying=false;midiActions.push(['stop']);};
        DroneSynth.handleMidi=(...data)=>midiActions.push(['synth',...data]);DroneSynth.handlePitchBend=value=>midiActions.push(['bend',value]);
        state.tracker.isPlaying=false;
        async function midiWait(condition){for(let i=0;i<400&&!condition();i++)await new Promise(resolve=>setTimeout(resolve,5));}
        async function midiSend(messages){await window.RustLooper.request('midi_test',{token:midiAccess.token,messages});}
        await midiSend([[0x92,36,100],[0x92,36,0],[0x90,46,100],[0x90,47,100],[0x90,48,100],
            [0x90,127,90],[0x80,127,0],[0xE0,0,96],[0xC0,2],[0xFA],[0xFC],[0xFB]]);
        await midiWait(()=>midiActions.filter(a=>a[0]==='start').length===2);
        assert(midiActions.some(a=>a[0]==='press'&&a[1]===0)&&midiActions.some(a=>a[0]==='release'&&a[1]===0),'Native MIDI notes and zero-velocity note-off reach existing pad mappings');
        assert(['overdub','substitute','sustain'].every(action=>midiActions.some(a=>a[0]===action)),'Native MIDI retains global overdub, substitute and sustain mappings');
        assert(midiActions.filter(a=>a[0]==='synth').length===2&&midiActions.some(a=>a[0]==='bend'&&a[1]===.5),'Native MIDI preserves unmapped synth notes and pitch bend');
        assert(midiActions.some(a=>a[0]==='pattern'&&a[1]===2)&&midiActions.filter(a=>a[0]==='start').length===2&&midiActions.some(a=>a[0]==='stop'),'Native MIDI retains program changes and start/continue/stop transport');
        state.midiCCMap={};state.midiLearn={active:true,type:'loop_vol',id:0,min:0,max:1};
        await midiSend([[0xB0,119,64]]);await midiWait(()=>!state.midiLearn.active);
        assert(state.midiCCMap[119]?.type==='loop_vol'&&!state.midiLearn.active,'Native CC input completes the original MIDI learn path');
        await midiSend([[0xB0,119,32]]);await midiWait(()=>Math.abs(state.loops[0].volume-32/127)<.000001);
        assert(Math.abs(state.loops[0].volume-32/127)<.000001,'Learned native CC updates the existing loop volume control');
        await midiSend([[0x90,36,100]]);await midiWait(()=>midiActions.filter(a=>a[0]==='press'&&a[1]===0).length===2);
        await window.RustLooper.request('midi_test',{token:midiAccess.token,connected:false});await midiWait(()=>state.midiInputs.length===0);
        assert(state.midiInputs.length===0&&midiPort.state==='disconnected'&&midiActions.filter(a=>a[0]==='release'&&a[1]===0).length===2,'Native MIDI disconnect releases held pad notes and clears the original device list');
        await window.RustLooper.request('midi_test',{token:midiAccess.token,connected:true});await midiWait(()=>state.midiInputs.length===1);
        assert(state.midiInputs[0]!==midiPort&&state.midiInputs[0].id===midiPort.id,'Native MIDI reconnect creates a fresh input with the same device identity');
        App.bindMIDIInputs();App.bindMIDIInputs();await midiSend([[0x90,36,100],[0x80,36,0]]);
        await midiWait(()=>midiActions.filter(a=>a[0]==='release'&&a[1]===0).length===3);
        assert(midiActions.filter(a=>a[0]==='press'&&a[1]===0).length===3&&midiActions.filter(a=>a[0]==='release'&&a[1]===0).length===3,'Rebound native MIDI inputs trigger each action once');
        midiAccess.close();
        [App.handleInputPress,App.handleInputRelease,App.toggleGlobalOverdub,App.toggleGlobalSubstitute,App.toggleGlobalSus,
            TrackerManager.togglePlay,TrackerManager.selectPattern,LoopManager.stopAll,DroneSynth.handleMidi,DroneSynth.handlePitchBend]=midiMethods;
        state.keyMapping.midi=savedMidiMap;state.midiCCMap=savedCC;state.midiLearn=savedLearn;state.tracker.isPlaying=savedPlaying;
        if(savedMidiStorage===null)localStorage.removeItem('ascii_looper_midicc');else localStorage.setItem('ascii_looper_midicc',savedMidiStorage);
        UIManager.setLoopVolume(0,savedVolume);
        await state.audioContext.close();
        window.ipc.postMessage(JSON.stringify({kind:'self_test',ok:true,checks}));
    } catch (error) {
        window.ipc.postMessage(JSON.stringify({kind:'self_test',ok:false,checks,error:String(error),stack:error.stack}));
    }
}, 50), { once: true });
