// Markup methods and effect colours copied from the preserved GPL-2.0 reference.
// See resources/ui/LICENSE. Only presentation code executes in this adapter.
(() => {
    const state = {loops: [], samplers: [], customEffects: {}, fxPresets: {}, globalPresets: {}, soloState: {active: false}, globalOverdubMode: false, globalSubstituteMode: false};
    const SamplerManager = {escapeHTML: value => String(value).replace(/[&<>"']/g, ch => ({'&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;'}[ch]))};
    const I18n = {t: key => key.replace(/^TIP_/, '').replaceAll('_', ' ')};
const effectColors = {
    reverb: 'lightblue',
    machineReverb: 'cyan',
    delay: 'green',
    distortion: 'yellow',
    fuzz: 'orange',
    overdrive: 'red',
    compressor: 'lightgreen',
    arpDelay: '#e0f',
    dusk: 'SlateBlue',
    harmony: '#F5F5DC',
    griz: 'gray',
    zigZ: '#ff00ff',
    reverse: 'white', 
    eq: '#4fd'
};
    class UIManager {
    static createEffectToggleHTML(loop, index, effectKey, label, prefix = 'loop') {
        const checked = loop.effects[effectKey] ? 'checked' : '';
        const color = effectColors[effectKey] || '#888';
        return `<span style="margin-right: 8px; color: ${color}; white-space:nowrap;">
            <input type="checkbox" ${checked} id="${prefix}-${index}-${effectKey}" 
                   onchange="event.stopPropagation(); state.loops[${index}].toggleEffect('${effectKey}'); EffectManager.setActiveTab(${index});" title="Toggle" aria-label="Toggle ${label}"> 
            <span style="cursor:pointer; text-decoration:underline;" onclick="event.stopPropagation(); EffectManager.goToControl(${index}, '${effectKey}');" title="Go to Controls">${label}</span>
        </span>`;
    }
    static generateEffectsControlsHTML(loop, index, prefix = 'loop') {
        // Just the toggle buttons
        let html = `<div id="${prefix}-fx-container-${index}" style="display:flex; flex-wrap:wrap; gap:5px; margin-top:2px;">`;
        
        const effectMap = {
            'B': { key: 'reverb', label: 'revB' },
            'V': { key: 'machineReverb', label: 'reVm' },
            'D': { key: 'delay', label: 'Dlay' },
            'A': { key: 'arpDelay', label: 'Arpd' },
            'T': { key: 'distortion', label: 'disTr' },
            'F': { key: 'fuzz', label: 'Fuzz' },
            'O': { key: 'overdrive', label: 'Odrv' },
            'C': { key: 'compressor', label: 'Comp' },
            'K': { key: 'dusk', label: 'dusK' },
            'Q': { key: 'eq', label: 'eQ' },
                'Z': { key: 'zigZ', label: 'zigZ' },
                'G': { key: 'griz', label: 'Griz' },
                'H': { key: 'harmony', label: 'Harm' }
            };
            
            // Add reverse effect first (not in signal chain)
            html += UIManager.createEffectToggleHTML(loop, index, 'reverse', 'Rvers', prefix);
        
        // Add other effects in signal chain order
        const uniqueChain = [...new Set((loop.signalChain || "QCATFODBVKZG").split(''))].join('');
        for (const char of uniqueChain) {
            const effect = effectMap[char];
            if (effect) {
                html += UIManager.createEffectToggleHTML(loop, index, effect.key, effect.label, prefix);
            } else {
                // Check custom effects
                for (const [name, fx] of Object.entries(state.customEffects)) {
                    if (fx.code === char) 
                        html += UIManager.createEffectToggleHTML(loop, index, name, fx.name.substring(0,5), prefix);
                }
            }
        }
        html += `</div>`;
        return html;
    }
    static generateLiveLoopHTML(loop, index) {
        let stateColor = '#555'; 
        if (loop.state === 'recording') stateColor = '#f00';
        else if (loop.state === 'armed') stateColor = '#ff0';
        else if (loop.state === 'playing') stateColor = '#0f0';
        else if (loop.state === 'stopped') stateColor = '#0ff';
        else if (loop.state === 'overdubbing') stateColor = '#f0f';
        else if (loop.state === 'substituting') stateColor = '#00cccc';
        else if (loop.state === 'multiplying') stateColor = '#ffaa00';
        else if (loop.state === 'queued') stateColor = '#0088aa';

        return `
        <div class="loop ${loop.state}" id="live-loop-${index}" style="border: 1px solid ${stateColor}; margin-bottom: 5px; height: 120px; box-sizing: border-box; display: flex; flex-direction: column; overflow: hidden;">
            <div class="loop-main-content" style="display: flex; flex-direction: column; gap: 4px; flex: 1; min-width: 0;">
                <div class="loop-header" style="display: flex; justify-content: space-between; align-items: center; cursor: pointer; padding: 8px; min-height: 44px; background: rgba(0,20,20,0.6); border-bottom: 1px dashed ${stateColor};" onclick="LoopManager.handleAction(${index}, 'short')">
                    <div style="display: flex; align-items: center; gap: 8px; overflow: hidden; flex: 1;">
                        <strong style="color: ${stateColor}; pointer-events:none;">[${index + 1}]</strong> 
                        <span style="color: ${stateColor}; font-size: 11px; font-family: 'Courier New', monospace; pointer-events:none;">${(loop.name || `Loop ${index + 1}`).replace(/"/g, '&quot;')}</span>
                    </div>
                    <span class="live-state-text" style="color: ${stateColor}; font-size: 10px; font-weight:bold; pointer-events:none;">${loop.state.toUpperCase()}</span>
                </div>
                <div class="loop-controls" style="padding: 4px; display: flex; justify-content: flex-start; flex-direction: column; min-height: 24px;">
                    ${UIManager.generateEffectsControlsHTML(loop, index, 'live-loop')}
                </div>
            </div>
        </div>
        `;
    }
    static updateLivePerformanceState() {
        const panel = document.getElementById('ws-live');
        if (!panel) return;
        let mode = 'stopped';
        const recStates = ['recording', 'overdubbing', 'substituting', 'multiplying'];
        const anyRec = state.masterRecording || state.loops.some(l => l && recStates.includes(l.state))
            || (window.DroneSynth && DroneSynth.instances && DroneSynth.instances.some(d => d.isRecording));
        const anyArmed = state.loops.some(l => l && (l.state === 'armed' || l.state === 'queued'))
            || state.samplers.some(s => s.state === 'armed');
        const anyPlaying = state.loops.some(l => l && l.state === 'playing')
            || (window.DroneSynth && DroneSynth.instances && DroneSynth.instances.some(d => d.state === 'playing'))
            || state.samplers.some(s => s.state === 'playing' || s.state === 'stopping');
        if (anyRec) mode = 'recording';
        else if (anyArmed) mode = 'armed';
        else if (anyPlaying) mode = 'playing';
        panel.classList.remove('live-recording', 'live-armed', 'live-playing', 'live-stopped');
        panel.classList.add('live-' + mode);
    }
    static getLoopAction(loop) {
        if (state.isFinishingRecording && state.recordingLoopId === loop.id) return 'SAVING';
        if (loop.state === 'playing') {
            if (state.globalSubstituteMode) return 'REPLACE';
            if (state.globalOverdubMode) return 'OVERDUB';
            return 'STOP';
        }
        return {
            empty: 'RECORD', stopped: 'PLAY', armed: 'CANCEL', queued: 'CANCEL',
            recording: 'FINISH', overdubbing: 'FINISH', substituting: 'FINISH',
            multiplying: 'FINISH', stopping: 'STOP NOW'
        }[loop.state];
    }
    static generateLoopHTML(loop, index) {
        const stateSymbols = {
            empty: '( )', armed: '(A)', recording: '(R)',
            playing: '[>]', stopped: '[S]', stopping: '[.]',
            overdubbing: '(O)', substituting: '(-)',
            multiplying: '(M)', queued: '[Q]'
        };
        
        const stateSymbol = stateSymbols[loop.state] || '( )';
        const effectiveDur = loop.duration / loop.playbackRate;
        const durationText = loop.audioBuffer && loop.duration > 0 ? `(${effectiveDur.toFixed(1)}s)` : '';
        const mutedText = loop.muted ? '[MUTED] ' : '';
        const action = this.getLoopAction(loop);
        const isSolo = state.soloState.active && state.soloState.loopId === index;
        const muteStyle = loop.muted ? 'background:#f00; color:#000; border-color:#f00;' : '';
        const soloStyle = isSolo ? 'background:#ff0; color:#000; border-color:#ff0;' : '';
        const volDb = loop.volume <= 0 ? '-inf' : (20 * Math.log10(loop.volume)).toFixed(1);
        const escape = SamplerManager.escapeHTML;
        const presetOptions = Object.keys(state.fxPresets).map(name =>
            `<option value="${escape(name)}" ${state.fxPresets[name] === loop.signalChain ? 'selected' : ''}>[CHAIN] ${escape(name)}</option>`
        ).join('');
        
        const mergeOptions = state.loops.map((l, i) => 
            (i !== index && l.audioBuffer) ? `<option value="${i}">${i+1}</option>` : ''
        ).join('');

        return `
            <div class="loop-main-content">
                <div class="loop-header loop-track-header">
                    <div class="loop-track-identity">
                        <strong>[${index + 1}]</strong>
                        <button type="button" id="loop-action-${index}" class="loop-track-action"
                                onclick="event.stopPropagation(); LoopManager.handleAction(${index}, 'short');"
                                aria-label="Loop ${index + 1}: ${action}" ${action === 'SAVING' ? 'disabled' : ''}>${action}</button>
                        <input type="text" class="track-name" value="${escape(loop.name || '')}" name="loop-name-${index}" placeholder="Loop ${index + 1}"
                               onkeydown="if(event.key==='Enter') this.blur(); event.stopPropagation();"
                               oninput="event.stopPropagation(); state.loops[${index}].name = this.value; if(window.UIManager && UIManager.updateLiveLoop) UIManager.updateLiveLoop(${index});"
                               onclick="event.stopPropagation(); EffectManager.setActiveTab(${index});" aria-label="Loop Name">
                        <div class="track-signal-pair loop-track-signal">
                            <canvas id="loop-wave-${index}" width="180" height="26" title="Click to Export WAV" aria-label="Loop ${index + 1} waveform" onclick="event.stopPropagation(); ProjectManager.exportLoop(${index});"></canvas>
                            <div id="loop-ascii-vu-${index}" class="ascii-vu-meter" aria-label="Loop ${index + 1} output level"></div>
                        </div>
                    </div>
                    <div class="loop-track-summary">
                        <span class="loop-track-state"><span id="loop-state-symbol-${index}">${stateSymbol}</span> <span id="loop-state-text-${index}">${loop.state.toUpperCase()}</span></span>
                        <span id="loop-extra-info-${index}">${mutedText}${durationText}</span>
                    </div>
                </div>
                <div class="loop-controls loop-track-controls">
                    
                    <div class="loop-track-parameters">
                        <div class="loop-track-parameter">
                            <div class="loop-track-parameter-heading"><label for="loop-vol-slider-${index}">Volume</label><span id="loop-vol-display-${index}">${volDb}dB</span></div>
                            <input type="range" id="loop-vol-slider-${index}" min="0" max="2" step="0.01" value="${loop.volume}" title="${I18n.t('TIP_VOL')}"
                                   oninput="UIManager.setLoopVolume(${index}, this.value); EffectManager.setActiveTab(${index});"
                                   onclick="event.stopPropagation()" aria-label="Loop Volume">
                        </div>
                        <div class="loop-track-parameter">
                            <div class="loop-track-parameter-heading"><label for="loop-pan-slider-${index}">Pan</label><span id="loop-pan-display-${index}">${loop.pan}</span></div>
                            <input type="range" id="loop-pan-slider-${index}" min="0" max="10" step="1" value="${loop.pan}" title="${I18n.t('TIP_PAN')}"
                                   oninput="UIManager.setLoopPan(${index}, this.value); EffectManager.setActiveTab(${index});"
                                   onclick="event.stopPropagation()" aria-label="Loop Pan">
                        </div>
                        <div class="loop-track-parameter loop-track-parameter-start">
                            <div class="loop-track-parameter-heading"><label for="loop-start-slider-${index}">Start</label><span id="loop-delay-display-${index}">${Math.floor(loop.startDelay * loop.duration * 1000)}ms</span></div>
                            <input type="range" id="loop-start-slider-${index}" min="0" max="1" step="0.001" value="${loop.startDelay}" title="${I18n.t('TIP_START')}"
                                   oninput="UIManager.setLoopStartDelay(${index}, this.value); EffectManager.setActiveTab(${index});"
                                   onchange="state.loops[${index}].restart(AudioEngine.currentTime + 0.05);"
                                   onclick="event.stopPropagation()" aria-label="Loop Start Delay">
                            <div class="loop-track-mini-actions">
                                <button type="button" class="small" onclick="event.stopPropagation(); UIManager.nudgeLoopStart(${index}, -1)" title="Nudge Left (1/64)" aria-label="Nudge loop start left">&lt;</button>
                                <button type="button" class="small btn-purple" onclick="event.stopPropagation(); UIManager.quantizeLoopStart(${index})" title="Quantize Start to 16th Note">Q</button>
                                <button type="button" class="small" onclick="event.stopPropagation(); UIManager.nudgeLoopStart(${index}, 1)" title="Nudge Right (1/64)" aria-label="Nudge loop start right">&gt;</button>
                            </div>
                        </div>
                        <div class="loop-track-parameter">
                            <div class="loop-track-parameter-heading"><label for="loop-speed-slider-${index}">Speed</label><span id="loop-speed-display-${index}">${loop.playbackRate.toFixed(2)}x</span></div>
                            <input type="range" id="loop-speed-slider-${index}" min="0.25" max="2" step="0.01" value="${loop.playbackRate}" title="${I18n.t('TIP_SPEED')}"
                                   oninput="UIManager.setLoopSpeed(${index}, this.value);"
                                   ondblclick="UIManager.resetLoopSpeed(${index});"
                                   onclick="event.stopPropagation()" aria-label="Loop Playback Speed">
                            <div class="loop-track-mini-actions">
                                <button type="button" class="small btn-cyan" onclick="event.stopPropagation(); state.loops[${index}].toggleHalfSpeed(); EffectManager.setActiveTab(${index});" title="Toggle Half Speed [H]">1/2</button>
                                <button type="button" class="small" onclick="event.stopPropagation(); UIManager.resetLoopSpeed(${index});" title="Reset Loop Playback Speed" ${loop.playbackRate === 1 ? 'disabled' : ''}>1x</button>
                            </div>
                        </div>
                        <div class="loop-track-parameter">
                            <div class="loop-track-parameter-heading"><label for="loop-fbk-slider-${index}">Feedback</label><span id="loop-fbk-display-${index}">${Math.round(loop.feedback * 100)}%</span></div>
                            <input type="range" id="loop-fbk-slider-${index}" min="0" max="1" step="0.01" value="${loop.feedback}" title="Overdub Feedback (Decay); double-click to reset to 80%"
                                   oninput="UIManager.setLoopFeedback(${index}, this.value);"
                                   ondblclick="UIManager.resetLoopFeedback(${index});"
                                   onclick="event.stopPropagation()" aria-label="Loop Overdub Feedback">
                        </div>
                    </div>

                    <div class="loop-track-actions">
                        <div class="loop-track-action-group" role="group" aria-label="Performance controls">
                            <button type="button" class="small" onclick="event.stopPropagation(); LoopManager.toggleMute(${index}); EffectManager.setActiveTab(${index});" style="${muteStyle}" aria-pressed="${loop.muted}" title="${I18n.t('TIP_MUTE')}">${loop.muted ? 'UNM' : 'MUTE'}</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); SoloManager.toggleSolo(${index}); EffectManager.setActiveTab(${index});" style="${soloStyle}" aria-pressed="${isSolo}" title="${I18n.t('TIP_SOLO')}">SOLO</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); state.loops[${index}].retrigger(); EffectManager.setActiveTab(${index});" title="Retrigger [/] — Instantly restart on-beat" ${loop.audioBuffer ? '' : 'disabled'}>RTRG</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); state.loops[${index}].multiply(); EffectManager.setActiveTab(${index});" title="Multiply [\\] — press to capture, press again to extend. No input = double." ${loop.audioBuffer ? '' : 'disabled'}>MULT</button>
                            <button type="button" class="small" onpointerdown="event.preventDefault(); event.stopPropagation(); state.loops[${index}].stutterStart(); const b=this; const up=()=>{state.loops[${index}].stutterStop(); b.classList.remove('btn-active'); window.removeEventListener('pointerup',up); window.removeEventListener('pointercancel',up);}; window.addEventListener('pointerup',up); window.addEventListener('pointercancel',up); b.classList.add('btn-active');" title="Stutter: hold during playback, release to resume" style="touch-action:none; user-select:none;" ${loop.audioBuffer && loop.state === 'playing' ? '' : 'disabled'}>STUT</button>
                        </div>
                        <div class="loop-track-action-group" role="group" aria-label="Audio editing and export">
                            <button type="button" class="small" onclick="event.stopPropagation(); state.loops[${index}].undo(); EffectManager.setActiveTab(${index});" title="Undo Overdub" ${loop.undoStack.length > 0 ? '' : 'disabled'}>UNDO</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); state.loops[${index}].redo(); EffectManager.setActiveTab(${index});" title="Redo Overdub" ${loop.redoStack.length > 0 ? '' : 'disabled'}>REDO</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); state.loops[${index}].normalize(); UIManager.updateLoop(${index}); EffectManager.setActiveTab(${index});" title="${I18n.t('TIP_NORM')}" ${loop.audioBuffer ? '' : 'disabled'}>NORM</button>
                            <button type="button" class="small" onclick="event.stopPropagation(); ProjectManager.exportDryWet('loop', ${index}); EffectManager.setActiveTab(${index});" title="${I18n.t('TIP_SAVE')} Dry/Wet" ${loop.audioBuffer ? '' : 'disabled'}>SAVE</button>
                            <button type="button" class="danger small loop-track-delete" onclick="event.stopPropagation(); state.loops[${index}].clear(); EffectManager.setActiveTab(${index});" title="${I18n.t('TIP_DEL')}">DEL</button>
                        </div>
                    </div>

                    <div class="loop-track-fx-toolbar">
                        <div class="loop-track-fx-group">
                            <button type="button" class="small" onclick="event.stopPropagation(); EffectManager.setActiveTab(${index}); EffectManager.scrollToEffects();" title="Go to FX Controls">FX</button>
                            <select onchange="event.stopPropagation(); EffectManager.applyPresetToLoop(${index}, this.value)"
                                    onclick="event.stopPropagation(); EffectManager.setActiveTab(${index});" title="Load Complete FX-State Preset" aria-label="Load Complete Loop FX Preset">
                                <option value="">FULL PRESET</option>
                                ${Object.keys(state.globalPresets).map(name => `<option value="GLOBAL:${escape(name)}">[FULL] ${escape(name)}</option>`).join('')}
                            </select>
                            <select onchange="event.stopPropagation(); EffectManager.applyPresetToLoop(${index}, this.value)"
                                    onclick="event.stopPropagation(); EffectManager.setActiveTab(${index});" aria-label="Apply FX Preset">
                                <option value="">CHAIN PRESET</option>${presetOptions}
                            </select>
                            <input type="text" value="${escape(loop.signalChain)}"
                                   onchange="event.stopPropagation(); state.loops[${index}].setSignalChain(this.value);"
                                   onclick="event.stopPropagation(); EffectManager.setActiveTab(${index});" title="Manual FX Chain" aria-label="Loop FX Chain">
                            <a href="#mod-sync" onclick="document.getElementById('fxMixTimeSel').focus()" class="mixin-link">mixin time: ${escape(state.fxMixTime || '2s')}</a>
                        </div>
                        <div class="loop-track-fx-group">
                            <label for="merge-src-${index}">MERGE &#8592;</label>
                            <select id="merge-src-${index}" onclick="event.stopPropagation()" aria-label="Merge Source">
                                <option value="">-</option>${mergeOptions}
                            </select>
                            <button type="button" class="small btn-purple" onclick="event.stopPropagation(); LoopManager.mergeLoops(${index}, document.getElementById('merge-src-${index}').value)" title="Merge selected source into this loop">GO</button>
                        </div>
                    </div>

                    ${UIManager.generateEffectsControlsHTML(loop, index)}
                </div>
            </div>
        `;
    }
    }
    const Log={debug(){}};
    class SyncMarkup {
    static renderTimelineMarkers() {
        const markersContainer = document.getElementById('markers');
        if (!markersContainer) return;

        markersContainer.innerHTML = '';

        if (!state.syncEnabled || state.loopLength <= 0) return;

        const timeline = document.getElementById('timeline');
        if (!timeline) return;

        const width = timeline.offsetWidth - 20; // 20 for padding
        const beatsPerBar = state.timeSig.num;
        const totalBeats = beatsPerBar * state.bars;

        // Safety check to prevent infinite loops or UI freezing
        if (!totalBeats || totalBeats <= 0 || totalBeats > 256) {
             Log.debug('Scales', 'Invalid beat count:', totalBeats);
             return;
        }

        for (let beat = 0; beat < totalBeats; beat++) {
            const beatNumber = beat + 1;
            const position = (beat / totalBeats) * width; // Use beat, not beatNumber, for 0-indexed start

            const marker = document.createElement('div');
            marker.className = 'timeline-marker';
            marker.style.left = `${position + 10}px`;

            const label = document.createElement('span');
            label.className = 'timeline-marker-label';

            if (beat % beatsPerBar === 0) {
                // Bar marker
                marker.classList.add('bar');
                const barNumber = Math.floor(beat / beatsPerBar) + 1;
                label.textContent = `Bar ${barNumber}`;
                marker.style.height = '70px'; // Taller bar markers
            } else {
                // Beat marker
                label.textContent = `${beatNumber}`;
            }

            marker.appendChild(label);
            markersContainer.appendChild(marker);
        }
    }
    }
    window.AionLiveLoopMarkup=snapshot=>snapshot.tracks.map((t,i)=>UIManager.generateLiveLoopHTML({...t,name:'',effects:{reverse:!!t.reverse},signalChain:'QCATFODBVKZG'},i));
    window.AionLivePanelState=snapshot=>{state.loops=snapshot.tracks;UIManager.updateLivePerformanceState();};
    window.AionSyncMarkup = snapshot => {
        state.syncEnabled=snapshot.syncMode!==0;state.bars=snapshot.syncBars;state.timeSig={num:snapshot.syncNum};
        SyncMarkup.renderTimelineMarkers();
    };
    window.AionLoopMarkup = (snapshot, mode) => {
        state.globalOverdubMode = mode === 'overdub'; state.globalSubstituteMode = mode === 'substitute';
        state.loops = snapshot.tracks.map(t => ({...t, duration: t.length / snapshot.sampleRate, playbackRate: t.playbackRate ?? 1, startDelay: t.startDelay ?? 0, pan: t.pan ?? 5,
            audioBuffer: t.length > 0, undoStack: Array(t.undoCount||0), redoStack: Array(t.redoCount||0), effects: {reverse:!!t.reverse}, signalChain: 'QCATFODBVKZG', name: t.name??''}));
        return state.loops.map((t, i) => UIManager.generateLoopHTML({...t, id:i}, i));
    };
})();

// Original committed-waveform renderer; dimensions match the original HiDPI canvas.
window.AionDrawWaveform=(cvs,loop,progress,live=null)=>{
    const state={syncMode:live?.syncMode,liveRecordAnalyser:live?{getFloatTimeDomainData:out=>out.set(live.samples)}:null,liveRecordAnalyserData:live?Float32Array.from(live.samples):null};
    const now=live?.now??0,SyncManager={getBarDuration:()=>live?.barDuration??0};
    const dpr=window.devicePixelRatio||1,w=Math.max(1,cvs.clientWidth),h=Math.max(1,cvs.clientHeight);
    const width=Math.round(w*dpr),height=Math.round(h*dpr);
    if(cvs.width!==width)cvs.width=width;if(cvs.height!==height)cvs.height=height;
    const ctx=cvs.getContext('2d'),playheadX=Math.floor(progress*w);
    ctx.resetTransform();ctx.scale(dpr,dpr);ctx.clearRect(0,0,w,h);
                // Draw Waveform
                if (loop.wavePeaks) {
                    const peaks = loop.wavePeaks;
                    const amp = h;

                    let waveColor = '#666';
                    if (loop.state === 'recording') waveColor = '#f00';
                    else if (loop.state === 'overdubbing') waveColor = '#f0f';
                    else if (loop.state === 'armed') waveColor = '#ff0';
                    else if (loop.state === 'substituting') waveColor = '#00cccc';
                    else if (loop.state === 'multiplying') waveColor = '#ffaa00';
                    else if (loop.state === 'queued') waveColor = '#0088aa';
                    else if (loop.state === 'playing') waveColor = '#0f0';
                    else if (loop.state === 'stopped') waveColor = '#888';
                    else if (loop.state === 'stopping') waveColor = '#0a0';

                    // Draw dim waveform for full width (unplayed portion)
                    ctx.globalAlpha = 0.35;
                    ctx.fillStyle = waveColor;
                    for (let i = 0; i < w && peaks.length; i++) {
                        const peakIdx = Math.floor(i / w * peaks.length);
                        const mag = peaks[peakIdx];
                        const hBar = Math.max(1, mag * amp);
                        const y = (h - hBar) / 2;
                        ctx.fillRect(i, y, 1, hBar);
                    }

                    // Draw bright waveform for played portion (up to playhead)
                    ctx.globalAlpha = 1.0;
                    for (let i = 0; i < playheadX && i < w && peaks.length; i++) {
                        const peakIdx = Math.floor(i / w * peaks.length);
                        const mag = peaks[peakIdx];
                        const hBar = Math.max(1, mag * amp);
                        const y = (h - hBar) / 2;
                        ctx.fillRect(i, y, 1, hBar);
                    }
                    ctx.globalAlpha = 1.0;
                } else if ((loop.state === 'recording' || loop.state === 'overdubbing' || loop.state === 'substituting' || loop.state === 'multiplying') && state.liveRecordAnalyser) {
                    // Live input waveform — oscilloscope-style trace
                    const analyser = state.liveRecordAnalyser;
                    const data = state.liveRecordAnalyserData;
                    analyser.getFloatTimeDomainData(data);
                    const mid = h / 2;
                    const step = data.length / w;

                    // Draw filled waveform shape (symmetric around center)
                    let waveColor = '#f00';
                    if (loop.state === 'overdubbing') waveColor = '#f0f';
                    else if (loop.state === 'substituting') waveColor = '#00cccc';
                    else if (loop.state === 'multiplying') waveColor = '#ffaa00';
                    ctx.fillStyle = waveColor;
                    ctx.globalAlpha = 0.6;
                    for (let i = 0; i < w; i++) {
                        const idx = Math.floor(i * step);
                        const v = data[idx] || 0;
                        const barH = Math.max(1, Math.abs(v) * h);
                        ctx.fillRect(i, mid - barH / 2, 1, barH);
                    }
                    // Draw center line trace on top
                    ctx.globalAlpha = 1.0;
                    ctx.strokeStyle = waveColor;
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    for (let i = 0; i < w; i++) {
                        const idx = Math.floor(i * step);
                        const v = data[idx] || 0;
                        const y = mid - v * (h * 0.5);
                        if (i === 0) ctx.moveTo(i, y);
                        else ctx.lineTo(i, y);
                    }
                    ctx.stroke();
                    // Bar boundary markers — rolling window so user sees approaching bar lines
                    if (state.syncMode === 'dynamic' && loop.graph && loop.graph.startTime) {
                        const barDur = SyncManager.getBarDuration();
                        if (barDur > 0) {
                            const elapsed = now - loop.graph.startTime;
                            const visibleBars = 4;
                            const windowDur = visibleBars * barDur;
                            // Align window start to bar boundary, keep playhead in right half
                            const windowStart = Math.max(0, Math.floor(elapsed / windowDur) * windowDur);
                            const windowEnd = windowStart + windowDur;
                            ctx.strokeStyle = 'rgba(255, 255, 0, 0.45)';
                            ctx.lineWidth = 1;
                            for (let b = 1; b < visibleBars; b++) {
                                const barTime = windowStart + b * barDur;
                                const barX = ((barTime - windowStart) / windowDur) * w;
                                ctx.beginPath();
                                ctx.moveTo(barX, 0);
                                ctx.lineTo(barX, h);
                                ctx.stroke();
                            }
                            // Bar count labels at top
                            ctx.fillStyle = 'rgba(255, 255, 0, 0.6)';
                            ctx.font = '8px monospace';
                            const startBar = Math.round(windowStart / barDur);
                            for (let b = 0; b <= visibleBars; b++) {
                                const barX = (b / visibleBars) * w;
                                ctx.fillText(`${startBar + b}`, barX + 2, 8);
                            }
                        }
                    }
                }

    if(loop.state!=='empty'){ctx.fillStyle='#fff';ctx.fillRect(playheadX,0,1,h);}
};
