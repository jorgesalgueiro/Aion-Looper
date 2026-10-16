// Generate a surgical runtime adapter and retain the exact reference DSP fallback.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'ui/Modules/loopTracks.js'), 'utf8');
const begin = source.indexOf('            let finalBuffer = rawBuffer;');
const end = source.indexOf('            loop.wavePeaks = UIManager.generateWaveformPeaks(loop.audioBuffer);', begin);
if (begin < 0 || end < 0) throw new Error('Recording source region missing');
const region = source.slice(begin, end);
const dsp = region.slice(0, region.indexOf('            loop.audioBuffer = finalBuffer;'))
    .replace('            state.recordingActualStartTime = 0; // Reset', '')
    .replaceAll('this.compensateLatency(', 'compensate(');
const replacement = `            const recordingContext = state.audioContext;
            const previousBuffer = loop.audioBuffer;
            const previousGraph = loop.graph;
            const submittedGeneration = this.transportGeneration;
            const recordingBpm = state.bpm;
            const settings = {
                syncMode: state.syncMode, syncEnabled: state.syncEnabled,
                loopLength: state.loopLength, recordingActualStartTime: state.recordingActualStartTime,
                inputLatencyMs: state.inputLatencyMs,
                audioContext: {sampleRate: recordingContext.sampleRate,
                    baseLatency: recordingContext.baseLatency, outputLatency: recordingContext.outputLatency,
                    createBuffer: recordingContext.createBuffer.bind(recordingContext)}
            };
            const capturedLoop = {graph: previousGraph ? {startTime: previousGraph.startTime} : null};
            const barDuration = settings.syncMode === 'dynamic' ? SyncManager.getBarDuration() : 0;
            const loopLength = settings.syncMode !== 'dynamic' && settings.syncEnabled ? SyncManager.getLoopLength() : 0;
            const sync = {getBarDuration: () => barDuration, getLoopLength: () => loopLength};
            const fallback = (rawBuffer, compensate) => {
                const state = settings, loop = capturedLoop, SyncManager = sync;
${dsp}
                return finalBuffer;
            };
            const finalBuffer = await window.RustLooper.finalizeRecording(recordingContext, rawBuffer, settings, capturedLoop, startFrame, sync, fallback);
            if (state.loopRecorder !== recorder || state.recordingLoopId !== loopId || state.audioContext !== recordingContext) return;
            if (recordingContext.state === 'closed' || state.loops[loopId] !== loop || loop.audioBuffer !== previousBuffer
                || loop.graph !== previousGraph || this.transportGeneration !== submittedGeneration) {
                this.forceCleanupRecording(state.loops[loopId] === loop ? loop : null);
                return;
            }
            state.recordingActualStartTime = 0;
            loop.audioBuffer = finalBuffer;
            loop.duration = finalBuffer.duration;
            loop.originalBpm = recordingBpm;
            loop.dynamicRecorded = settings.syncMode === 'dynamic';
            loop.playbackRate = 1.0;
`;
fs.writeFileSync(path.join(root, 'crates/looper-desktop/assets/recording-edits.json'), JSON.stringify([[region, replacement]], null, 2) + '\n');
