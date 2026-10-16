// Native worker RPC; the original DOM/CSS and compatibility audio remain intact.
(() => {
    let sequence = 0;
    const session = typeof crypto === 'object' && crypto.randomUUID ? crypto.randomUUID() : `${Date.now()}-${Math.random()}`;
    const pending = new Map();
    window.RustLooper = {
        request(operation, args = {}) {
            const id = ++sequence;
            return new Promise((resolve, reject) => {
                const message = JSON.stringify({ ...args, id, operation, session });
                if (message.length > 24000000) { reject(new Error('Native payload exceeds capacity.')); return; }
                const timer = setTimeout(() => { pending.delete(id); reject(new Error('Native operation timed out.')); }, operation==='stretch'?60000:15000);
                pending.set(id, { resolve, reject, timer });
                try { window.ipc.postMessage(message); }
                catch (error) { clearTimeout(timer); pending.delete(id); reject(error); }
            });
        },
        receive(response) {
            if (response.session != null && response.session !== session) return;
            if (response.kind === 'native_input') { if(response.session===session)window.RustLooper.onInputEvent?.(response); return; }
            if (response.kind === 'native_midi') { window.RustLooper.onMidiEvent?.(response); return; }
            const request = pending.get(response.id);
            if (!request) return;
            pending.delete(response.id);
            clearTimeout(request.timer);
            if (response.ok) request.resolve(response.value);
            else request.reject(new Error(response.error));
        }
    };
    function pack(buffer) {
        const left = buffer.getChannelData(0);
        const right = buffer.getChannelData(buffer.numberOfChannels === 1 ? 0 : 1);
        const bytes = new Uint8Array(buffer.length * 8);
        const data = new DataView(bytes.buffer);
        for (let i = 0; i < buffer.length; i++) {
            data.setFloat32(i * 8, left[i], true);
            data.setFloat32(i * 8 + 4, right[i], true);
        }
        return packBytes(bytes);
    }
    // Keep capture IPC moving while transferring a stopped recording. The take
    // is already private; yielding cannot change the submitted PCM or settings.
    let yieldChannel;
    const yielding=[];
    function yieldInput() {
        // Message tasks yield to IPC without hidden WebViews' timer clamping.
        if(typeof MessageChannel!=='function') return new Promise(resolve=>setTimeout(resolve,0));
        if(!yieldChannel) {
            yieldChannel=new MessageChannel();
            yieldChannel.port1.onmessage=()=>yielding.shift()?.();
        }
        return new Promise(resolve=>{yielding.push(resolve);yieldChannel.port2.postMessage(0);});
    }
    async function packResponsive(buffer) {
        const left=buffer.getChannelData(0),right=buffer.getChannelData(buffer.numberOfChannels===1?0:1);
        const bytes=new Uint8Array(buffer.length*8),data=new DataView(bytes.buffer);
        for(let start=0;start<buffer.length;start+=4096) {
            const end=Math.min(start+4096,buffer.length);
            for(let i=start;i<end;i++) { data.setFloat32(i*8,left[i],true);data.setFloat32(i*8+4,right[i],true); }
            await yieldInput();
        }
        const parts=[];
        for(let offset=0;offset<bytes.length;offset+=32768) {
            parts.push(String.fromCharCode(...bytes.subarray(offset,offset+32768)));
            await yieldInput();
        }
        return btoa(parts.join(''));
    }
    async function copyResponsive(result,buffer) {
        if(result.pcm===undefined) { copyResult(result,buffer);return; }
        if(typeof result.pcm!=='string'||result.pcm.length>Math.ceil(buffer.length*8/3)*4) throw new Error('Native PCM length mismatch.');
        const text=atob(result.pcm);
        if(text.length!==buffer.length*8) throw new Error('Native PCM length mismatch.');
        const bytes=new Uint8Array(text.length);
        for(let start=0;start<bytes.length;start+=32768) {
            const end=Math.min(start+32768,bytes.length);
            for(let i=start;i<end;i++) bytes[i]=text.charCodeAt(i);
            await yieldInput();
        }
        const data=new DataView(bytes.buffer);
        // Finish validation before writing any sample, as in the synchronous path.
        for(let start=0;start<buffer.length*2;start+=8192) {
            const end=Math.min(start+8192,buffer.length*2);
            for(let i=start;i<end;i++) if(!Number.isFinite(data.getFloat32(i*4,true))) throw new Error('Invalid native PCM.');
            await yieldInput();
        }
        const left=buffer.getChannelData(0),right=buffer.numberOfChannels===2?buffer.getChannelData(1):null;
        for(let start=0;start<buffer.length;start+=4096) {
            const end=Math.min(start+4096,buffer.length);
            for(let i=start;i<end;i++) { left[i]=data.getFloat32(i*8,true);if(right)right[i]=data.getFloat32(i*8+4,true); }
            await yieldInput();
        }
    }
    function packBytes(bytes) {
        const parts = [];
        for (let offset = 0; offset < bytes.length; offset += 32768) {
            parts.push(String.fromCharCode(...bytes.subarray(offset, offset + 32768)));
        }
        return btoa(parts.join(''));
    }
    function unpackBytes(encoded, maxBytes) {
        if (typeof encoded !== 'string' || encoded.length > Math.ceil(maxBytes / 3) * 4) throw new Error('Invalid native binary size.');
        const text = atob(encoded);
        if (text.length > maxBytes) throw new Error('Invalid native binary size.');
        return Uint8Array.from(text, character => character.charCodeAt(0));
    }
    function wavInfo(bytes) {
        if (bytes.length < 12) return null;
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        if (view.getUint32(0) !== 0x52494646 || view.getUint32(8) !== 0x57415645) return null;
        const end = view.getUint32(4, true) + 8;
        if (end > bytes.length || end < 12) return null;
        for (let offset = 12; offset + 8 <= end;) {
            const length = view.getUint32(offset + 4, true);
            const next = offset + 8 + length + (length % 2);
            if (next > end) return null;
            if (view.getUint32(offset) === 0x666d7420) {
                if (length < 16) return null;
                let tag = view.getUint16(offset + 8, true), bits = view.getUint16(offset + 22, true);
                const containerBits = bits, extensible = tag === 0xfffe;
                const channels = view.getUint16(offset + 10, true);
                if (extensible) {
                    if (length < 40 || view.getUint16(offset + 24, true) < 22
                        || view.getUint16(offset + 24, true) + 18 > length) return null;
                    const tail = [0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71];
                    if (!tail.every((byte,i)=>bytes[offset+34+i]===byte)) return null;
                    tag = view.getUint16(offset + 32, true);
                    bits = view.getUint16(offset + 26, true);
                    const mask = view.getUint32(offset + 28, true);
                    if (mask !== 0 && mask !== (channels === 1 ? 4 : 3)) return null;
                }
                if (![1, 2].includes(channels) || !(tag === 1 && [16, 24].includes(bits) || tag === 3 && bits === 32)) return null;
                if (![16,24,32].includes(containerBits) || bits > containerBits || tag === 3 && containerBits !== 32) return null;
                return { rate:view.getUint32(offset + 12, true), tag, bits, containerBits, extensible };
            }
            offset = next;
        }
        return null;
    }
    // Source workflows await decoding; callback overloads retain the browser API.
    const Context = window.AudioContext || window.webkitAudioContext;
    if (Context?.prototype.decodeAudioData) {
        const originalDecode = Context.prototype.decodeAudioData;
        const scales = new WeakMap();
        function integerScale(context, info) {
            const {bits, containerBits, extensible} = info;
            const key = `${bits}/${containerBits}/${extensible}`;
            let formats = scales.get(context);
            if (!formats) { formats = new Map(); scales.set(context, formats); }
            if (!formats.has(key)) {
                const bytesPerSample = containerBits / 8, dataOffset = extensible ? 68 : 44;
                const bytes = new Uint8Array(dataOffset + bytesPerSample * 2);
                const view = new DataView(bytes.buffer);
                function text(offset, value) { for (let i = 0; i < value.length; i++) bytes[offset+i] = value.charCodeAt(i); }
                text(0,'RIFF'); view.setUint32(4,bytes.length-8,true); text(8,'WAVEfmt ');
                view.setUint32(16,extensible?40:16,true); view.setUint16(20,extensible?0xfffe:1,true); view.setUint16(22,1,true);
                view.setUint32(24,context.sampleRate,true); view.setUint32(28,context.sampleRate*bytesPerSample,true);
                view.setUint16(32,bytesPerSample,true); view.setUint16(34,containerBits,true);
                if (extensible) {
                    view.setUint16(36,22,true); view.setUint16(38,bits,true); view.setUint32(40,4,true);
                    bytes.set([1,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71],44);
                }
                text(dataOffset-8,'data'); view.setUint32(dataOffset-4,bytesPerSample*2,true);
                const sentinel=bits===16?8784:1048577;
                for (const [index,sample] of [2**(bits-1)-1,sentinel].entries()) {
                    const packed=sample*2**(containerBits-bits);
                    for(let i=0;i<bytesPerSample;i++) bytes[dataOffset+index*bytesPerSample+i]=(packed>>>(i*8))&255;
                }
                const probe = Promise.resolve(originalDecode.call(context,bytes.buffer)).then(buffer => {
                    const [peak,interior] = buffer.getChannelData(0), maximum=2**(bits-1)-1;
                    if (peak===1 && interior===Math.fround(sentinel/maximum)) return {positive_full_scale:true,positive_reciprocal:false};
                    if (peak===1 && interior===Math.fround(sentinel*Math.fround(1/maximum))) return {positive_full_scale:true,positive_reciprocal:true};
                    if (peak===Math.fround(maximum/(maximum+1)) && interior===Math.fround(sentinel/(maximum+1))) return {positive_full_scale:false,positive_reciprocal:false};
                    throw new Error('Unsupported browser WAV integer scaling.');
                });
                formats.set(key,probe);
            }
            return formats.get(key);
        }
        Context.prototype.decodeAudioData = function (data, ...callbacks) {
            if (callbacks.length || Object.prototype.toString.call(data) !== '[object ArrayBuffer]'
                || data.byteLength > 16000000 || typeof structuredClone !== 'function')
                return originalDecode.call(this, data, ...callbacks);
            let bytes;
            try { bytes = new Uint8Array(data); }
            catch (_) { return originalDecode.call(this, data, ...callbacks); }
            const info = wavInfo(bytes);
            if (info?.rate !== this.sampleRate) return originalDecode.call(this, data, ...callbacks);
            const encoded = packBytes(bytes);
            const fallback = data.slice(0);
            // Match decodeAudioData's ownership transfer while retaining independent
            // bytes for the browser fallback if native decoding is unsupported.
            structuredClone(data, { transfer: [data] });
            return (async () => {
                try {
                    const scaling = info.tag === 1 ? await integerScale(this,info) : {positive_full_scale:false,positive_reciprocal:false};
                    if (this.state === 'closed') throw new Error('Audio context closed during WAV import.');
                    const result = await window.RustLooper.request('decode_wav', { wav: encoded, ...scaling });
                    if (this.state === 'closed') throw new Error('Audio context closed during WAV import.');
                    if (result.sample_rate !== this.sampleRate || ![1,2].includes(result.channels)
                        || !Number.isInteger(result.length) || result.length <= 0 || result.length > 1000000) throw new Error('Invalid native WAV metadata.');
                    const buffer = this.createBuffer(result.channels, result.length, result.sample_rate);
                    copyResult(result, buffer);
                    window.RustLooper.lastNativeOperation = 'decode_wav';
                    return buffer;
                } catch (error) {
                    if (this.state === 'closed') throw error;
                    console.error('Native WAV decode fallback:', error);
                    return originalDecode.call(this, fallback, ...callbacks);
                }
            })();
        };
    }
    let encodeQueue = Promise.resolve(), queuedWavFrames = 0, queuedWavJobs = 0;
    window.RustLooper.nativeWavEncodes = 0;
    window.RustLooper.encodeWAVBuffer = function (buffer, fadeSeconds = 0) {
        if (!Number.isFinite(fadeSeconds) || fadeSeconds < 0) return Promise.reject(new Error('Invalid export fade.'));
        const originalEncode = source => {
            if (fadeSeconds === 0) return AudioEngine.bufferToWAV(source);
            const channels = Array.from({length:source.numberOfChannels},(_,c)=>source.getChannelData(c).slice());
            const faded = {numberOfChannels:source.numberOfChannels,length:source.length,sampleRate:source.sampleRate,getChannelData:c=>channels[c]};
            AudioEngine.applyFades(faded,fadeSeconds);
            return AudioEngine.bufferToWAV(faded);
        };
        if (!buffer || buffer.numberOfChannels > 2 || buffer.length > 1000000
            || queuedWavFrames + buffer.length > 4000000 || queuedWavJobs >= 32) {
            return Promise.resolve(originalEncode(buffer));
        }
        // Capture each take when requested, before waiting behind other project audio.
        const pcm = pack(buffer);
        const metadata = {numberOfChannels:buffer.numberOfChannels, length:buffer.length, sampleRate:buffer.sampleRate};
        queuedWavFrames += metadata.length;
        queuedWavJobs++;
        const work = encodeQueue.then(async () => {
            try {
                const result = await window.RustLooper.request('encode_wav', {pcm, sample_rate:metadata.sampleRate, channels:metadata.numberOfChannels, fade_seconds:fadeSeconds});
                const expected = 44 + metadata.length * metadata.numberOfChannels * 2;
                const bytes = unpackBytes(result.wav, expected);
                const view = new DataView(bytes.buffer);
                if (bytes.length !== expected || wavInfo(bytes)?.rate !== metadata.sampleRate
                    || view.getUint32(4,true) !== expected - 8
                    || view.getUint32(12) !== 0x666d7420 || view.getUint32(16,true) !== 16
                    || view.getUint16(20,true) !== 1 || view.getUint16(22,true) !== metadata.numberOfChannels
                    || view.getUint32(28,true) !== metadata.sampleRate * metadata.numberOfChannels * 2
                    || view.getUint16(32,true) !== metadata.numberOfChannels * 2
                    || view.getUint16(34,true) !== 16 || view.getUint32(36) !== 0x64617461
                    || view.getUint32(40,true) !== expected - 44) throw new Error('Invalid native WAV export.');
                window.RustLooper.lastNativeOperation = 'encode_wav';
                window.RustLooper.nativeWavEncodes++;
                return bytes.buffer;
            } catch (error) {
                const snapshot = {...metadata,
                    getChannelData(channel) {
                        const bytes = unpackBytes(pcm, metadata.length * 8), view = new DataView(bytes.buffer);
                        return Float32Array.from({length:metadata.length}, (_, frame) => view.getFloat32(frame * 8 + channel * 4, true));
                    }};
                console.error('Native WAV export fallback:', error);
                return originalEncode(snapshot);
            }
        }).finally(() => { queuedWavFrames -= metadata.length; queuedWavJobs--; });
        // One encode at a time keeps a 20-loop project below the worker IPC queue
        // limit. Failed jobs cannot block the next save or leak their reservations.
        encodeQueue = work.catch(() => {});
        return work;
    };
    if (typeof ProjectManager === 'function') {
        const originalExport = ProjectManager.exportLoop;
        ProjectManager.exportLoop = async function (id) {
            const loop = state.loops[id], buffer = loop?.audioBuffer;
            if (!buffer || buffer.numberOfChannels > 2 || buffer.length > 1000000) return originalExport.call(this, id);
            const name = (loop.name || `Loop_${loop.id + 1}`).replace(/[^a-z0-9]/gi, '_');
            const filename = `${name}_${Date.now()}.wav`;
            const wav = await window.RustLooper.encodeWAVBuffer(buffer);
            this.downloadWAV(wav, filename);
        };
    }
    function copyResult(result, buffer) {
        const left = buffer.getChannelData(0);
        const right = buffer.numberOfChannels === 2 ? buffer.getChannelData(1) : null;
        if (result.pcm !== undefined) {
            const text = atob(result.pcm);
            if (text.length !== buffer.length * 8) throw new Error('Native PCM length mismatch.');
            const bytes = new Uint8Array(text.length);
            for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i);
            const data = new DataView(bytes.buffer);
            // Validate the entire response before mutating the destination.
            for (let i = 0; i < buffer.length * 2; i++) {
                if (!Number.isFinite(data.getFloat32(i * 4, true))) throw new Error('Invalid native PCM.');
            }
            for (let i = 0; i < buffer.length; i++) {
                left[i] = data.getFloat32(i * 8, true);
                if (right) right[i] = data.getFloat32(i * 8 + 4, true);
            }
        } else {
            if (!Array.isArray(result.frames) || result.frames.length !== buffer.length
                || result.frames.some(frame => !Array.isArray(frame) || frame.length !== 2 || frame.some(sample => !Number.isFinite(sample)))) {
                throw new Error('Invalid native PCM.');
            }
            for (let i = 0; i < buffer.length; i++) {
                left[i] = result.frames[i][0];
                if (right) right[i] = result.frames[i][1];
            }
        }
    }
    if (typeof App === 'function') {
        const startApp = App.startApp;
        App.startApp = async function (...args) {
            await window.RustLooper.request('authorize_audio');
            return startApp.apply(this, args);
        };
        const requestMIDI = App.requestMIDIAccess;
        App.requestMIDIAccess = async function (...args) {
            await window.RustLooper.request('authorize_midi');
            return requestMIDI.apply(this, args);
        };
    }
    const originalNormalize = Loop.prototype.normalize;
    const editQueues = new WeakMap();
    function enqueueEdit(owner, action) {
        const currentBuffer = () => owner.rawBuffer || owner.audioBuffer || owner.buffer;
        const submitted = currentBuffer();
        const work = (editQueues.get(owner) || Promise.resolve()).catch(() => {}).then(previous => {
            if (currentBuffer() !== submitted && currentBuffer() !== previous) return;
            return action();
        });
        let tracked;
        tracked = work.finally(() => { if (editQueues.get(owner) === tracked) editQueues.delete(owner); });
        editQueues.set(owner, tracked);
        return tracked;
    }
    const originalMultiply = Loop.prototype.multiplyInstant;
    Loop.prototype.multiplyInstant = function () {
        if (!this.audioBuffer || this.state !== 'stopped' || this.audioBuffer.numberOfChannels > 2
            || this.audioBuffer.length * 2 > 1000000) return originalMultiply.call(this);
        const context = state.audioContext;
        return enqueueEdit(this, async () => {
            const buffer = this.audioBuffer;
            if (!buffer || this.state !== 'stopped' || state.audioContext !== context || context?.state === 'closed') return;
            if (buffer.length * 2 > 1000000 || buffer.numberOfChannels > 2) return originalMultiply.call(this);
            const valid = () => this.audioBuffer === buffer && this.state === 'stopped'
                && state.audioContext === context && context?.state !== 'closed';
            try {
                const result = await window.RustLooper.request('repeat', { pcm: pack(buffer), times: 2 });
                if (!valid()) return;
                const next = context.createBuffer(buffer.numberOfChannels, buffer.length * 2, buffer.sampleRate);
                copyResult(result, next);
                this.saveStateForUndo();
                this.audioBuffer = next;
                this.duration = next.duration;
                this.wavePeaks = window.UIManager ? UIManager.generateWaveformPeaks(next) : null;
                const syncSource = document.getElementById('syncSource');
                if (syncSource && syncSource.value == this.id) SyncManager.updateSettings();
                if (window.UIManager) UIManager.updateLoop(this.id);
                window.RustLooper.lastNativeOperation = 'repeat';
                return next;
            } catch (error) {
                if (valid()) { originalMultiply.call(this); return this.audioBuffer; }
                console.error('Native multiply fallback:', error);
            }
        });
    };
    if (typeof SamplerManager === 'function') {
        for (const operation of ['reverse', 'normalize']) {
            const original = SamplerManager[operation];
            SamplerManager[operation] = function (id) {
                const sampler = state.samplers[id];
                if (!sampler?.buffer || sampler.buffer.numberOfChannels > 2 || sampler.buffer.length > 1000000)
                    return original.call(this, id);
                this._stopSampler(id);
                const context = state.audioContext;
                return enqueueEdit(sampler, async () => {
                    const buffer = sampler.buffer;
                    const valid = () => state.samplers[id] === sampler && sampler.buffer === buffer
                        && sampler.state === 'stopped' && state.audioContext === context && context?.state !== 'closed';
                    if (!buffer || !valid()) return;
                    if (buffer.numberOfChannels > 2 || buffer.length > 1000000) return original.call(this, id);
                    try {
                        const result = await window.RustLooper.request(operation, { pcm: pack(buffer) });
                        if (!valid()) return;
                        const next = context.createBuffer(buffer.numberOfChannels, buffer.length, buffer.sampleRate);
                        copyResult(result, next);
                        sampler.buffer = next;
                        sampler.wavePeaks = UIManager.generateWaveformPeaks(next);
                        this.drawWaveform(id);
                        window.RustLooper.lastNativeOperation = operation;
                        return next;
                    } catch (error) {
                        if (valid()) { original.call(this, id); return sampler.buffer; }
                        console.error(`Native sampler ${operation} fallback:`, error);
                    }
                });
            };
        }
    }
    if (typeof SampleLab === 'function') {
        window.RustLooper.labEditRevision=0;
        const originalStretch=SampleLab.soundTouchTimeStretch;
        window.RustLooper.stretchSample=async function(source,ratio,semitones) {
            const context=state.audioContext, expected=Math.max(1,Math.round(source.length*ratio));
            if(source.numberOfChannels>2 || source.length>1000000 || expected>1000000
                || source.sampleRate<8000 || source.sampleRate>192000 || !Number.isFinite(ratio)||ratio<=.01
                || !Number.isFinite(semitones)||Math.abs(semitones)>24) {
                window.RustLooper.lastStretchEngine='SoundTouch JS fallback';
                return originalStretch.call(SampleLab,source,ratio,semitones);
            }
            const snapshot=context.createBuffer(source.numberOfChannels,source.length,source.sampleRate);
            for(let c=0;c<source.numberOfChannels;c++) snapshot.getChannelData(c).set(source.getChannelData(c));
            try {
                const result=await window.RustLooper.request('stretch',{pcm:pack(snapshot),sample_rate:snapshot.sampleRate,
                    channels:snapshot.numberOfChannels,stretch_ratio:ratio,pitch_semitones:semitones});
                if(state.audioContext!==context||context.state==='closed') throw new Error('Audio context changed during stretch.');
                if(result.engine!=='rubberband-r3'||result.length!==expected||result.sample_rate!==snapshot.sampleRate
                    ||result.channels!==snapshot.numberOfChannels) throw new Error('Invalid native stretch result.');
                const output=context.createBuffer(snapshot.numberOfChannels,expected,snapshot.sampleRate);
                copyResult(result,output);
                window.RustLooper.lastNativeOperation='stretch';
                window.RustLooper.lastStretchEngine='Rubber Band 4.0.0 R3';
                return output;
            } catch(error) {
                if(state.audioContext!==context||context.state==='closed') throw error;
                console.error('Native stretch fallback:',error);
                window.RustLooper.lastStretchEngine='SoundTouch JS fallback';
                return originalStretch.call(SampleLab,snapshot,ratio,semitones);
            }
        };
        for (const operation of ['reverse','normalize','crop']) {
            const original = SampleLab[operation];
            SampleLab[operation] = function () {
                const submitted=this.rawBuffer, context=state.audioContext;
                if (!submitted || submitted.numberOfChannels>2 || submitted.length>1000000
                    || this.isRecording || this.isProcessing || !context || context.state==='closed') {
                    const result=original.call(this);
                    // Source normalization is in-place, so identity alone cannot
                    // cancel a stretch whose capture predates that edit.
                    if(operation==='normalize'&&document.getElementById('labStatus')?.textContent==='NORMALIZED.') window.RustLooper.labEditRevision++;
                    return result;
                }
                let start=0,end=1,selection,previewRequest;
                if (operation==='crop') {
                    const startEl=document.getElementById('labStart'),endEl=document.getElementById('labEnd');
                    selection={start:startEl.value,end:endEl.value,snap:this.zcEnabled()};
                    start=Math.min(parseFloat(selection.start),parseFloat(selection.end));
                    end=Math.max(parseFloat(selection.start),parseFloat(selection.end));
                    // Keep all source alerts and zero-crossing selection semantics.
                    if (!Number.isFinite(start)||!Number.isFinite(end)||start<0||end>1
                        ||end-start<.0005||end-start>=.999) return original.call(this);
                    if (selection.snap) {
                        start=this.snapToZeroCross(submitted,start);end=this.snapToZeroCross(submitted,end);
                        if (end-start<.0005) return original.call(this);
                    }
                    this.stopPreview();previewRequest=this.previewRequest;
                }
                return enqueueEdit(this,async()=>{
                    const buffer=this.rawBuffer;
                    const valid=()=>this.rawBuffer===buffer&&state.audioContext===context&&context.state!=='closed'
                        &&!this.isRecording&&!this.isProcessing
                        &&(operation!=='crop'||this.previewRequest===previewRequest
                            &&document.getElementById('labStart').value===selection.start
                            &&document.getElementById('labEnd').value===selection.end&&this.zcEnabled()===selection.snap);
                    if (!buffer||!valid()) return;
                    const first=Math.floor(start*buffer.length),length=operation==='crop'?Math.floor(end*buffer.length)-first:buffer.length;
                    if (length<=0||first<0||first+length>buffer.length) return original.call(this);
                    const before=AudioEngine.cloneBuffer(buffer);
                    const next=context.createBuffer(buffer.numberOfChannels,length,buffer.sampleRate);
                    let changed=true;
                    try {
                        const result=await window.RustLooper.request(operation==='crop'?'slice':operation,
                            {pcm:pack(before),start_sample:first,target_frames:length});
                        if (!valid()) return;
                        copyResult(result,next);
                        if (operation==='normalize') {
                            if (typeof result.changed!=='boolean') throw new Error('Invalid normalization outcome.');
                            changed=result.changed;
                        }
                        window.RustLooper.lastNativeOperation=operation==='crop'?'slice':operation;
                    } catch(error) {
                        if (!valid()) return;
                        // Run only on the submitted snapshot, preserving source PCM
                        // and the prior undo slot until an edit can commit.
                        const fallback=AudioEngine.cloneBuffer(before);
                        if (operation==='normalize') changed=AudioEngine.normalizeBuffer(fallback);
                        for(let c=0;c<next.numberOfChannels;c++) {
                            const data=fallback.getChannelData(c);
                            if(operation==='reverse') data.reverse();
                            next.getChannelData(c).set(data.subarray(first,first+length));
                        }
                        console.error('Native Sample Lab edit fallback:',error);
                    }
                    if (!valid()) return;
                    if (!changed) { document.getElementById('labStatus').textContent='ALREADY MAXED.';return buffer; }
                    this._undoBuffer=before;this.updateUndoBtn();
                    if (operation==='normalize') {
                        for(let c=0;c<buffer.numberOfChannels;c++) buffer.getChannelData(c).set(next.getChannelData(c));
                    } else this.rawBuffer=next;
                    this.processedBuffer=null;
                    if(operation==='crop') {document.getElementById('labStart').value=0;document.getElementById('labEnd').value=1;}
                    this.updateDisplays();this.renderWaveform();
                    document.getElementById('labStatus').textContent=operation==='crop'?`CROPPED: ${this.rawBuffer.duration.toFixed(2)}s`
                        :operation==='normalize'?'NORMALIZED.':'REVERSED.';
                    return this.rawBuffer;
                });
            };
        }
        SampleLab.save=async function() {
            const buffer=this.processedBuffer||this.rawBuffer;
            if(!buffer) return alert('No audio to save.');
            const filename=`sampler_${this.processedBuffer?'processed':'edited'}_${Date.now()}.wav`;
            const wav=await window.RustLooper.encodeWAVBuffer(buffer);
            ProjectManager.downloadWAV(wav,filename);
        };
    }
    Loop.prototype.normalize = async function () {
        const buffer = this.audioBuffer;
        if (!buffer || this.state !== 'stopped' || buffer.numberOfChannels > 2 || buffer.length > 1000000) return originalNormalize.call(this);
        const context = state.audioContext;
        const valid = () => this.audioBuffer === buffer && this.state === 'stopped'
            && state.audioContext === context && context?.state !== 'closed';
        try {
            const result = await window.RustLooper.request('normalize', { pcm: pack(buffer) });
            // Discard results after a take, transport, or audio-session change.
            if (!valid()) return;
            copyResult(result, buffer);
            this.wavePeaks = UIManager.generateWaveformPeaks(buffer);
            UIManager.updateLoop(this.id);
            window.RustLooper.lastNativeOperation = 'normalize';
        } catch (error) {
            if (valid()) originalNormalize.call(this);
            console.error('Native normalize fallback:', error);
        }
    };
    if (typeof AudioEngine === 'function') {
        window.RustLooper.finalizeRecording = async function(context, raw, settings, loop, startFrame, sync, fallback) {
            const take = context.createBuffer(raw.numberOfChannels, raw.length, raw.sampleRate);
            for (let c=0;c<raw.numberOfChannels;c++) take.getChannelData(c).set(raw.getChannelData(c));
            const latency = Math.max(0,(settings.audioContext.baseLatency||0)+(settings.audioContext.outputLatency||0)+(settings.inputLatencyMs||0)/1000);
            const rate = context.sampleRate;
            let mode='free', target=raw.length, start=0, destinationOffset=0;
            if (settings.syncMode === 'dynamic') {
                const bar=sync.getBarDuration();
                if (bar>0) { mode='dynamic'; target=Math.round(Math.max(1,Math.round(raw.duration/bar))*bar*rate); }
            } else if (settings.syncEnabled && settings.recordingActualStartTime && loop.graph && loop.graph.startTime) {
                mode='precision'; target=Math.round(sync.getLoopLength()*rate);
                const captureStart=startFrame>=0 ? startFrame/rate : settings.recordingActualStartTime;
                const prefix=loop.graph.startTime+latency-captureStart;
                if (prefix>=0) start=Math.floor(prefix*rate); else destinationOffset=Math.floor(-prefix*rate);
            } else if (settings.syncEnabled && settings.loopLength>0) {
                mode='sync_fallback'; target=Math.round(sync.getLoopLength()*rate);
            }
            const length=mode==='precision' ? target : mode==='dynamic'||mode==='sync_fallback' ? Math.min(target,raw.length) : raw.length;
            if (raw.numberOfChannels<=2 && raw.sampleRate===rate && raw.length+target<=1000000
                && [target,start,destinationOffset,length].every(n=>Number.isSafeInteger(n)&&n>=0) && length>0
                && Number.isFinite(latency)) {
                try {
                    const pcm=take.length>4096 ? await packResponsive(take) : pack(take);
                    const result=await window.RustLooper.request('finalize_recording',{pcm,sample_rate:rate,
                        latency_seconds:latency,record_mode:mode,target_frames:target,start_sample:start,destination_offset:destinationOffset});
                    const next=context.createBuffer(raw.numberOfChannels,length,rate);
                    if(next.length>4096) await copyResponsive(result,next); else copyResult(result,next);
                    window.RustLooper.lastNativeOperation='finalize_recording';
                    return next;
                } catch(error) { console.error('Native recording finalization fallback:',error); }
            }
            // The generated fallback is the original DSP region, with frozen
            // settings and private captured PCM. Device/session state stays live.
            return fallback(take, buffer => {
                if (latency<.0001) return buffer;
                const next=context.createBuffer(buffer.numberOfChannels,buffer.length,buffer.sampleRate);
                const shift=Math.min(buffer.length,Math.floor(latency*buffer.sampleRate));
                for (let c=0;c<buffer.numberOfChannels;c++) {
                    const src=buffer.getChannelData(c),dst=next.getChannelData(c);
                    if (buffer.length>shift) {
                        const wrap=buffer.length-shift;
                        dst.set(src.subarray(shift)); dst.set(src.subarray(0,shift),wrap);
                        const fade=Math.min(Math.floor(buffer.sampleRate*.005),shift,wrap);
                        for (let i=0;i<fade;i++) {
                            dst[wrap-fade+i]*=Math.cos(i/fade*(Math.PI/2));
                            dst[wrap+i]*=Math.sin(i/fade*(Math.PI/2));
                        }
                    } else dst.set(src);
                }
                return next;
            });
        };
        window.RustLooper.finalizePunch = async function (context,destination,captured,offsetSeconds,feedback) {
            const next=context.createBuffer(destination.numberOfChannels,destination.length,destination.sampleRate);
            const take=context.createBuffer(captured.numberOfChannels,captured.length,captured.sampleRate);
            for (let c=0;c<next.numberOfChannels;c++) next.getChannelData(c).set(destination.getChannelData(c));
            for (let c=0;c<take.numberOfChannels;c++) take.getChannelData(c).set(captured.getChannelData(c));
            const latency=Math.max(0,(context.baseLatency||0)+(context.outputLatency||0)+(state.inputLatencyMs||0)/1000);
            const offset=Math.floor(offsetSeconds*destination.sampleRate);
            if (destination.numberOfChannels<=2 && captured.numberOfChannels<=2
                && destination.length+captured.length<=1000000 && destination.sampleRate===captured.sampleRate
                && Number.isSafeInteger(offset) && Number.isFinite(latency)
                && Number.isFinite(feedback) && feedback>=0 && feedback<=1) {
                try {
                    const result=await window.RustLooper.request('finalize_punch',{pcm:pack(next),take_pcm:pack(take),offset,
                        sample_rate:destination.sampleRate,feedback,latency_seconds:latency});
                    copyResult(result,next);
                    window.RustLooper.lastNativeOperation='finalize_punch';
                    return next;
                } catch(error) { console.error('Native punch finalization fallback:',error); }
            }
            // Work only on submitted snapshots. Preserve source order and frozen
            // latency settings if a worker failure requires original processing.
            for (let c=0;c<take.numberOfChannels;c++) {
                const data=take.getChannelData(c);
                let sum=0;
                for (const sample of data) sum+=sample;
                const mean=sum/data.length;
                for (let i=0;i<data.length;i++) data[i]-=mean;
                if (latency>=0.0001) {
                    const shift=Math.min(data.length,Math.floor(latency*take.sampleRate));
                    if (data.length>shift) {
                        const original=data.slice(),wrap=data.length-shift;
                        data.set(original.subarray(shift)); data.set(original.subarray(0,shift),wrap);
                        const fade=Math.min(Math.floor(take.sampleRate*.005),shift,wrap);
                        for (let i=0;i<fade;i++) {
                            data[wrap-fade+i]*=Math.cos(i/fade*(Math.PI/2));
                            data[wrap+i]*=Math.sin(i/fade*(Math.PI/2));
                        }
                    }
                }
            }
            AudioEngine.seamlessLoopCrossfade(take,.008);
            await originalMix.call(AudioEngine,next,take,offsetSeconds,feedback);
            return next;
        };
        window.RustLooper.appendCapture = async function (context, original, captured) {
            const next = context.createBuffer(original.numberOfChannels, original.length + captured.length, original.sampleRate);
            const take = context.createBuffer(captured.numberOfChannels, captured.length, captured.sampleRate);
            for (let c = 0; c < original.numberOfChannels; c++) next.getChannelData(c).set(original.getChannelData(c));
            for (let c = 0; c < captured.numberOfChannels; c++) take.getChannelData(c).set(captured.getChannelData(c));
            if (original.numberOfChannels <= 2 && captured.numberOfChannels <= 2
                && original.sampleRate === captured.sampleRate && next.length <= 1000000) {
                try {
                    const result = await window.RustLooper.request('append_capture', {pcm:pack(original),take_pcm:pack(take),sample_rate:original.sampleRate});
                    copyResult(result,next);
                    window.RustLooper.lastNativeOperation = 'append_capture';
                    return next;
                } catch (error) { console.error('Native capture append fallback:',error); }
            }
            // Retain original DC-removal, splice and channel-mapping semantics
            // for oversized takes, wider layouts, or a failed native request.
            for (let c = 0; c < take.numberOfChannels; c++) {
                const data = take.getChannelData(c);
                let sum = 0;
                for (const sample of data) sum += sample;
                const mean = sum / data.length;
                for (let i = 0; i < data.length; i++) data[i] -= mean;
            }
            AudioEngine.seamlessLoopCrossfade(take,0.008);
            for (let c = 0; c < next.numberOfChannels; c++) next.getChannelData(c).set(take.getChannelData(Math.min(c,take.numberOfChannels-1)),original.length);
            return next;
        };
        const originalMix = AudioEngine.mixBuffersCircular;
        const queues = new WeakMap();
        AudioEngine.mixBuffersCircular = function (destination, take, offsetSeconds, feedback = 0.80) {
            const offset = Math.floor(offsetSeconds * (destination?.sampleRate || 0));
            if (!destination || !take || !destination.length || !take.length
                || destination.numberOfChannels > 2 || take.numberOfChannels > 2
                || destination.length + take.length > 1000000 || destination.sampleRate !== take.sampleRate
                || !Number.isSafeInteger(offset) || !Number.isFinite(feedback) || feedback < 0 || feedback > 1) {
                return originalMix.call(this, destination, take, offsetSeconds, feedback);
            }
            const context = state.audioContext;
            const work = (queues.get(destination) || Promise.resolve()).catch(() => {}).then(async () => {
                if (state.audioContext !== context || context?.state === 'closed') return;
                try {
                    const result = await window.RustLooper.request('mix', {
                        pcm: pack(destination), take_pcm: pack(take), offset,
                        sample_rate: destination.sampleRate, feedback
                    });
                    if (state.audioContext !== context || context?.state === 'closed') return;
                    copyResult(result, destination);
                    window.RustLooper.lastNativeOperation = 'mix';
                } catch (error) {
                    if (state.audioContext === context && context?.state !== 'closed') {
                        await originalMix.call(AudioEngine, destination, take, offsetSeconds, feedback);
                    }
                    console.error('Native mix fallback:', error);
                }
            });
            let tracked;
            tracked = work.finally(() => { if (queues.get(destination) === tracked) queues.delete(destination); });
            queues.set(destination, tracked);
            return tracked;
        };
    }
})();
