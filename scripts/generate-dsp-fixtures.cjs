// Execute the unchanged Ascii-Looper AudioEngine to generate native parity data.
// Run from the repository root with Node.js. Never changes the reference UI.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '..');
function buffer(frames, sampleRate = 1000) {
  const channels = [0, 1].map(c => Float32Array.from(frames, f => f[c]));
  return { numberOfChannels: 2, length: frames.length, sampleRate,
    get duration() { return this.length / sampleRate; },
    getChannelData: c => channels[c] };
}
function frames(buf) {
  return Array.from({ length: buf.length }, (_, i) => [0, 1].map(c => buf.getChannelData(c)[i]));
}
const context = vm.createContext({ console, Math, Float32Array, ArrayBuffer, DataView,
  setTimeout, document: { createElement: () => ({}), head: { appendChild() {} } },
  state: { audioContext: { state: 'running', baseLatency: .003, outputLatency: .004,
    createBuffer: (_channels, length, rate) => buffer(Array.from({ length }, () => [0, 0]), rate) }, inputLatencyMs: 2 },
  Log: { warn() {} } });
vm.runInContext(fs.readFileSync(path.join(root, 'ui/Modules/audioEngine.js'), 'utf8') +
  '\nglobalThis.referenceEngine = AudioEngine;', context);
const engine = context.referenceEngine;
const declarations = [];
function emit(name, values) {
  function literal(n) { return Number.isInteger(n) ? `${n}.0` : String(n); }
  declarations.push(`pub const ${name}: &[[f32; 2]] = &[\n` + values.map(f =>
    `    [${literal(f[0])}, ${literal(f[1])}],`).join('\n') + '\n];\n');
}
(async () => {
  const seam = buffer(Array.from({ length: 128 }, (_, i) => [
    Math.sin((i - 3) * .73) * .4, Math.cos(i * .31) * .6]));
  emit('SEAM_INPUT', frames(seam));
  engine.seamlessLoopCrossfade(seam, .025);
  emit('SEAM_OUTPUT', frames(seam));
  const selection=buffer(Array.from({length:37},(_,i)=>[Math.sin(i*.41)*.8,-.2+Math.cos(i*.31)*.6]));
  emit('SLICE_INPUT',frames(selection));
  emit('SLICE_OUTPUT',frames(engine.sliceBuffer(selection,.2,.8)));
  const normalized = buffer([[.1, -.2], [.25, -.5], [0, .125]]);
  emit('NORMALIZE_INPUT', frames(normalized));
  engine.normalizeBuffer(normalized);
  emit('NORMALIZE_OUTPUT', frames(normalized));
  const original = buffer(Array.from({ length: 8 }, (_, i) => [.7 - i * .1, -.8 + i * .1]));
  const take = buffer(Array.from({ length: 37 }, (_, i) => [Math.sin(i * .71) * .8, Math.cos(i * .19) * .9]));
  emit('MIX_INPUT', frames(original));
  emit('MIX_TAKE', frames(take));
  const overdub = buffer(frames(original));
  await engine.mixBuffersCircular(overdub, take, -.003, .8);
  emit('MIX_OUTPUT', frames(overdub));
  const substitute = buffer(frames(original));
  await engine.mixBuffersCircular(substitute, take, -.003, 0);
  emit('SUBSTITUTE_OUTPUT', frames(substitute));
  const latency = buffer(Array.from({ length: 32 }, (_, i) => [(i + 1) / 32, -(i + 1) / 64]));
  emit('LATENCY_INPUT', frames(latency));
  emit('LATENCY_OUTPUT', frames(engine.compensateLatency(latency)));
  const wav = new Uint8Array(engine.bufferToWAV(buffer([[-1, 1], [.125, -.25], [0, .5]], 48000)));
  declarations.push(`pub const WAV16_REFERENCE: &[u8] = &[${Array.from(wav).join(', ')}];\n`);
  for (const [label,length,duration] of [['LONG',137,.01],['SHORT',18,.01],['TINY',2,.01],['ZERO',137,0],['FULL',137,1]]) {
    const exported = buffer(Array.from({length},(_,i)=>[Math.sin(i*.41)*1.3,Math.cos(i*.19)*.7]));
    emit(`EXPORT_${label}_INPUT`,frames(exported));
    engine.applyFades(exported,duration);
    emit(`EXPORT_${label}_OUTPUT`,frames(exported));
    declarations.push(`pub const EXPORT_${label}_WAV: &[u8] = &[${Array.from(new Uint8Array(engine.bufferToWAV(exported))).join(', ')}];\n`);
  }
  // Run the actual original interactive MULT finalizer, including DC removal
  // before its 8 ms splice fade, rather than reimplementing it in this generator.
  Object.assign(context, {InputManager:{getRecordingNode:()=>null},UIManager:{generateWaveformPeaks:()=>[],updateLoop(){},updateStatus(){}}});
  context.document.getElementById = () => null;
  vm.runInContext(fs.readFileSync(path.join(root,'ui/Modules/loopTracks.js'),'utf8')+'\nUIManager.generateWaveformPeaks=()=>[]; UIManager.updateLoop=()=>{}; UIManager.updateStatus=()=>{}; globalThis.referenceLoops = LoopManager;',context);
  const appendInput = buffer([[1.5,-0],[-.75,.125],[.5,-1.25]]);
  const appendTake = buffer(Array.from({length:160},(_,i)=>[.3+Math.sin(i*.41)*.8,-.2+Math.cos(i*.19)*.6]));
  emit('APPEND_INPUT',frames(appendInput)); emit('APPEND_TAKE',frames(appendTake));
  const loop = {audioBuffer:appendInput,state:'multiplying',id:0,saveStateForUndo(){},restart(){}};
  context.state.loops = [loop]; context.state.audioContext.sampleRate = 1000;
  context.referenceLoops.createBufferFromChunks = () => appendTake;
  await context.referenceLoops.processMultiplyData([],0);
  if (loop.audioBuffer.length !== 163) throw new Error('Original MULT fixture did not complete');
  emit('APPEND_OUTPUT',frames(loop.audioBuffer));
  const punchInput = buffer(frames(original));
  const punchRaw = buffer(Array.from({length:137},(_,i)=>[.2+Math.sin(i*.43)*.6,-.3+Math.cos(i*.23)*.5]));
  emit('PUNCH_INPUT',frames(punchInput)); emit('PUNCH_RAW',frames(punchRaw));
  context.referenceLoops.pushUndoState = () => {};
  vm.runInContext('UIManager.updateLoopDisplays=()=>{};',context);
  for (const [latency,label] of [[0,'ZERO'],[.009,'SHORT'],[1,'FULL']]) {
    for (const substitute of [false,true]) {
      const current={id:0,state:substitute?'substituting':'overdubbing',feedback:.8,audioBuffer:buffer(frames(punchInput)),graph:null};
      Object.assign(context.state,{loops:[current],recordingLoopId:0,recordingStartOffset:-.003,inputLatencyMs:0,loopRecorder:{disconnect(){}}});
      Object.assign(context.state.audioContext,{baseLatency:latency,outputLatency:0});
      context.referenceLoops.createBufferFromChunks = () => buffer(frames(punchRaw));
      await context.referenceLoops.processOverdubData([],0,substitute);
      if (context.state.loopRecorder !== null) throw new Error('Original punch fixture did not complete');
      emit(`PUNCH_${substitute?'SUBSTITUTE':'OVERDUB'}_${label}`,frames(current.audioBuffer));
    }
  }
  // Execute the entire original first-recording finalizer in every branch.
  const recordingRaw = buffer(Array.from({length:137},(_,i)=>[.23+Math.sin(i*.43)*.81,-.31+Math.cos(i*.23)*.73]));
  emit('RECORD_RAW',frames(recordingRaw));
  context.SyncManager = {getBarDuration:()=>context.fixtureBar,getLoopLength:()=>context.fixtureLength};
  vm.runInContext('UIManager.updateExportButtons=()=>{};',context);
  context.Log.error = (...args) => { throw new Error(args.join(' ')); };
  const cases = [
    ['FREE_ZERO',0,'none',false,0,0,0,0],
    ['FREE_LATENCY',.009,'none',false,0,0,0,0],
    ['FREE_FULL',1,'none',false,0,0,0,0],
    ['DYNAMIC_TRIM',.009,'dynamic',false,.06,0,0,0],
    ['DYNAMIC_SHORT',.009,'dynamic',false,.2,0,0,0],
    ['DYNAMIC_INVALID_BAR',.009,'dynamic',false,0,0,0,0],
    ['PRECISION_TAIL',.009,'none',true,0,.09,.013,0],
    ['PRECISION_LATE',.009,'none',true,0,.09,.013,40],
    ['PRECISION_SHORT',.009,'none',true,0,.2,.013,0],
    ['PRECISION_TINY',.009,'none',true,0,.008,.013,0],
    ['SYNC_FOLD',.009,'none',true,0,.09,0,0],
    ['SYNC_SHORT',.009,'none',true,0,.2,0,0]
  ];
  for (const [label,latency,mode,sync,bar,length,graphStart,startFrame] of cases) {
    const current={id:0,state:'recording',graph:graphStart?{startTime:graphStart}:null,clear(){throw new Error('Original recording finalizer failed');}};
    Object.assign(context.state,{loops:[current],recordingLoopId:0,loopRecorder:{port:{postMessage(){}},disconnect(){}},
      syncMode:mode,syncEnabled:sync,loopLength:length,recordingActualStartTime:.001,inputLatencyMs:0,
      autoPlayAfterRecord:false,autoRecordNext:false});
    Object.assign(context,{fixtureBar:bar,fixtureLength:length});
    Object.assign(context.state.audioContext,{baseLatency:latency,outputLatency:0});
    context.referenceLoops.createBufferFromChunks=()=>buffer(frames(recordingRaw));
    await context.referenceLoops.processRecordedData([{}],0,startFrame);
    if (context.state.loopRecorder!==null || !current.audioBuffer) throw new Error('Original recording fixture did not finish');
    emit(`RECORD_${label}`,frames(current.audioBuffer));
  }
  const output = path.join(root, 'crates/looper-core/tests/fixtures/original_dsp.rs');
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, '// Generated by scripts/generate-dsp-fixtures.cjs from the original JS.\n' + declarations.join('\n'));
  console.log('Generated DSP parity fixtures from unchanged Ascii-Looper code.');
})().catch(error => { console.error(error); process.exitCode = 1; });
