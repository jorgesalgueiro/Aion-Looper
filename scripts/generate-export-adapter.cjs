// Keep the original project schema and UI; replace only export PCM work.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root,'ui/Modules/projectManager.js'),'utf8');
const begin = source.indexOf('        // 1. Export Dry Loops');
const end = source.indexOf('        // 2. Export Master Mix',begin);
if (begin<0 || end<0) throw new Error('Original export region missing');
const dry = `        // Snapshot all dry takes before awaiting the first encode.
        const dryExports = state.loops.filter(l => l.audioBuffer).map(l => {
            const id = l.id;
            try {
                const name = (l.name || \`Loop_\${id + 1}\`).replace(/[^a-z0-9]/gi, '_');
                const work = window.RustLooper.encodeWAVBuffer(l.audioBuffer)
                    .then(wav => ({wav}), error => ({error}));
                return {id, name, work};
            } catch(error) { return {id, work: Promise.resolve({error})}; }
        });
        for (const item of dryExports) {
            try {
                const result = await item.work;
                if (result.error) throw result.error;
                this.downloadWAV(result.wav, \`\${safeName}_\${item.name}_\${ts}.wav\`);
                exported++;
            } catch(e) { errors.push(\`Dry loop \${item.id + 1}: \${e.message}\`); }
        }

`;
const pattern = /AudioEngine\.applyFades\(buffer, 0\.01\);\r?\n([ \t]*)const wavData = AudioEngine\.bufferToWAV\(buffer\);/g;
const matches = [...source.matchAll(pattern)];
if (matches.length!==6) throw new Error('Expected six master/input/stem export finalizers');
const edits = [[source.slice(begin,end),dry]];
for (const match of matches) if (!edits.some(([from])=>from===match[0])) {
    edits.push([match[0],`// Rust worker applies the original export fades and PCM16 encoding.\n${match[1]}const wavData = await window.RustLooper.encodeWAVBuffer(buffer, 0.01);`]);
}
fs.writeFileSync(path.join(root,'crates/looper-desktop/assets/export-edits.json'),JSON.stringify(edits,null,2)+'\n');
