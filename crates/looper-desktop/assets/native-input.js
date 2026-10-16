(() => {
    const rpc = (...args) => window.RustLooper.request(...args);
    const streams = new WeakMap(), active = new Set(), modules = new WeakMap(), subscriptions = new Map();
    window.RustLooper.onInputEvent = event => subscriptions.get(event.token)?.(event);
    function deviceId(info) { return 'native:' + JSON.stringify({host:info.host,device:info.device}); }
    async function getStream(constraints) {
        const context = state.audioContext;
        if (!context || context.state === 'closed') throw new Error('Audio context is not available.');
        const selected = constraints.audio?.deviceId?.exact;
        let selection = {...window.AionNativeInputSelection};
        if (selected && selected !== 'default') {
            if (!selected.startsWith('native:')) throw new Error('Select a native input device from the device list.');
            selection = JSON.parse(selected.slice(7));
        }
        if (!modules.has(context)) {
            const loading = context.audioWorklet.addModule('./native-input-worklet.js');
            modules.set(context,loading);
            // Share concurrent loads, but let MIC/LINE retry a transient failure.
            loading.catch(() => { if (modules.get(context) === loading) modules.delete(context); });
        }
        await modules.get(context);
        if (state.audioContext !== context || context.state === 'closed') throw new Error('Input context was replaced.');
        const opened = await rpc('input_open',{...selection,sample_rate:context.sampleRate,synthetic:window.RustLooper.nativeInputTestMode === true,streaming:true});
        const token = opened.token, info = opened.info;
        if (!Number.isSafeInteger(token) || token <= 0 || !info || ![1,2].includes(info.channels)
            || info.sample_rate !== context.sampleRate || typeof info.host !== 'string' || typeof info.device !== 'string') {
            if (Number.isSafeInteger(token) && token > 0) await rpc('input_close',{token});
            throw new Error('Invalid native input configuration.');
        }
        let node, source, keepAlive, alive = true, timer, next = 0, reportAt = 0, lastPacketAt = Date.now(), suspended = false, lastPullAt = Date.now();
        const status = {host:info.host,device:info.label,sampleRate:info.sample_rate,channels:info.channels,deviceChannels:info.device_channels ?? info.channels,transport:'native-to-WebAudio',underruns:0,overruns:0,maxReadMs:0,maxPullGapMs:0,delivery:opened.streaming===true?'acknowledged-push':'poll',maxAckMs:0};
        const track = {
            kind:'audio', readyState:'live',
            getSettings:() => ({deviceId:deviceId(info),sampleRate:info.sample_rate,channelCount:info.channels}),
            stop() {
                if (!alive) return;
                alive = false; track.readyState = 'ended'; clearTimeout(timer); active.delete(stream); subscriptions.delete(token);
                if (node) { node.port.postMessage({type:'stop'}); try { node.disconnect(); } catch (_) {} }
                for (const gain of [source,keepAlive]) { try { gain?.disconnect(); } catch (_) {} }
                rpc('input_close',{token}).catch(error => console.error('Native input close:',error));
            }
        };
        const stream = {getTracks:()=>[track],getAudioTracks:()=>[track],getVideoTracks:()=>[],nativeStatus:status};
        function fault(error) {
            track.stop(); status.error = String(error);
            const input = state.inputs.find(input => input.stream === stream);
            if (input) { InputManager.lastInputError = `INPUT ${input.id+1}: ${error.message}`; InputManager.renderUI(); }
            console.error('Native input:',error);
        }
        try {
            if (state.audioContext !== context || context.state === 'closed') throw new Error('Input context was replaced.');
            node = new AudioWorkletNode(context,'rust-native-input',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[info.channels],channelCount:info.channels,channelCountMode:'explicit',processorOptions:{channels:info.channels,targetFrames:Math.max(384,Math.round(context.sampleRate*.064))}});
            source = context.createGain(); source.channelCount = info.channels; source.channelCountMode = 'explicit';
            keepAlive = context.createGain(); keepAlive.gain.value = 0;
            // Input graph rebuilds disconnect their source. A separate silent branch
            // keeps capture draining even while MON is off, without an audible tap.
            node.connect(source); node.connect(keepAlive); keepAlive.connect(context.destination);
            node.port.onmessage = ({data}) => {
                Object.assign(status,data);
                if (data.overruns > 0) { fault(new Error('Native input transport overflow.')); return; }
                if (data.underruns > 0) {
                    const input = state.inputs.find(input => input.stream === stream);
                    if (input) { InputManager.lastInputError = `INPUT ${input.id+1}: compatibility transport underruns (${data.underruns}); captured audio may contain gaps. Use Start-Native-Audio.cmd for Rust ASIO audio.`; InputManager.renderUI(); }
                }
            };
            streams.set(stream,{source,context}); active.add(stream);
            function consume(packet) {
                if (packet.position !== next || !Number.isInteger(packet.length) || packet.length < 0 || packet.length > 4096) throw new Error('Native input packet discontinuity.');
                if(typeof packet.pcm!=='string'||packet.pcm.length>Math.ceil(packet.length*8/3)*4) throw new Error('Invalid native input PCM length.');
                const text = atob(packet.pcm);
                if (text.length !== packet.length*8) throw new Error('Invalid native input PCM length.');
                const bytes = new Uint8Array(text.length);
                for(let i=0;i<text.length;i++) bytes[i]=text.charCodeAt(i);
                const view = new DataView(bytes.buffer),samples = new Float32Array(packet.length*2);
                for (let i = 0; i < samples.length; i++) { samples[i] = view.getFloat32(i*4,true); if (!Number.isFinite(samples[i])) throw new Error('Invalid native input sample.'); }
                next += packet.length; status.frames = packet.frames; status.maxCallbackFrames = packet.max_callback_frames;
                if (samples.length) lastPacketAt = Date.now();
                if (context.state !== 'running') {
                    if (!suspended) node.port.postMessage({type:'flush'});
                    suspended = true;
                } else {
                    suspended = false;
                    if (samples.length) node.port.postMessage({type:'pcm',samples},[samples.buffer]);
                    if (Date.now()-lastPacketAt > 3000) throw new Error('Native input stream stalled.');
                }
                if (Date.now() >= reportAt) { node.port.postMessage({type:'status'}); reportAt = Date.now()+500; }
            }
            async function pull() {
                if (!alive) return;
                if (state.audioContext !== context || context.state === 'closed') { track.stop(); return; }
                try {
                    const started = Date.now();
                    status.maxPullGapMs = Math.max(status.maxPullGapMs,started-lastPullAt);
                    lastPullAt = started;
                    const packet = await rpc('input_read',{token});
                    status.maxReadMs = Math.max(status.maxReadMs,Date.now()-started);
                    if (!alive) return;
                    if (state.audioContext !== context || context.state === 'closed') { track.stop(); return; }
                    consume(packet);
                    timer = setTimeout(pull,4);
                } catch (error) { if (alive) fault(error); }
            }
            if(opened.streaming===true) {
                let sequence=0;
                function acknowledge(sequence) {
                    const started=Date.now();
                    const pending=rpc('input_ack',{token,sequence}).then(()=>{status.maxAckMs=Math.max(status.maxAckMs,Date.now()-started);});
                    if(sequence!==0)pending.catch(error=>{if(alive)fault(error);});
                    return pending;
                }
                subscriptions.set(token,event=>{
                    if(!alive)return;
                    if(state.audioContext!==context||context.state==='closed'){track.stop();return;}
                    try {
                        if(event.error)throw new Error(event.error);
                        if(!Number.isSafeInteger(event.sequence)||event.sequence!==sequence+1)throw new Error('Native input event discontinuity.');
                        const now=Date.now();status.maxPullGapMs=Math.max(status.maxPullGapMs,now-lastPullAt);lastPullAt=now;
                        consume(event.packet);
                        sequence=event.sequence;status.sequence=sequence;
                        acknowledge(sequence);
                    }catch(error){fault(error);}
                });
                // The Rust owner waits for this handshake before sending PCM.
                await acknowledge(0);
            } else {
                pull();
            }
            return stream;
        } catch (error) { track.stop(); throw error; }
    }
    window.RustLooper.input = {
        getStream,
        source(context,stream) {
            const native = streams.get(stream);
            if (!native) return context.createMediaStreamSource(stream); // APP/SYS capture remains browser-owned.
            if (native.context !== context) throw new Error('Native source belongs to another audio context.');
            return native.source;
        },
        async devices() {
            const result = await rpc('input_devices',{synthetic:window.RustLooper.nativeInputTestMode === true});
            return result.devices.map(info=>({kind:'audioinput',deviceId:deviceId(info),label:`${info.host} / ${info.label}`,groupId:info.host}));
        },
        status:() => Array.from(active,stream=>({...stream.nativeStatus}))
    };
    window.addEventListener('pagehide',()=>{for (const stream of active) stream.getTracks()[0].stop();});
})();
