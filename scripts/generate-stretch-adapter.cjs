// Move Sample Lab's existing asynchronous processing step onto the native worker.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'ui/Modules/tracker.js'),'utf8');
const edits=[
    ['const srcRef = this.rawBuffer; // detect source swaps during async DSP','const srcRef = this.rawBuffer; // detect source swaps during async DSP\n        const processingContext = state.audioContext;\n        const processingRevision = window.RustLooper.labEditRevision;'],
    ['setTimeout(() => {\n                try {\n                    if (typeof SoundTouch === \'undefined\') throw new Error("SoundTouch lib missing");\n                    const result = this.soundTouchTimeStretch(sliced, ratio, pitchSemi);',
     'setTimeout(async () => {\n                try {\n                    const result = await window.RustLooper.stretchSample(sliced, ratio, pitchSemi);'],
    ['if (this.rawBuffer !== srcRef) {','if (this.rawBuffer !== srcRef || state.audioContext !== processingContext || processingContext.state === \'closed\' || window.RustLooper.labEditRevision !== processingRevision) {'],
    ["Log.error('SampleLab', 'DSP error:', e);\n                    statusEl.textContent = \"DSP ERROR.\";",
     "if (this.rawBuffer === srcRef && state.audioContext === processingContext && processingContext.state !== 'closed' && window.RustLooper.labEditRevision === processingRevision) {\n                        Log.error('SampleLab', 'DSP error:', e);\n                        statusEl.textContent = \"DSP ERROR.\";\n                    }"]
];
for(const [from] of edits) if(source.split(from).length!==2) throw new Error('Ambiguous stretch adapter region');
fs.writeFileSync(path.join(root,'crates/looper-desktop/assets/stretch-edits.json'),JSON.stringify(edits,null,2)+'\n');
