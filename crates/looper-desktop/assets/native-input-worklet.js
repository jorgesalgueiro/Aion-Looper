// Bounded compatibility transport from native capture to the existing input graph.
class NativeInputProcessor extends AudioWorkletProcessor {
    constructor(options) {
        super();
        const config = options.processorOptions;
        this.capacity = 16384;
        this.ring = new Float32Array(this.capacity * 2);
        this.target = config.targetFrames;
        this.channels = config.channels;
        this.read = 0;
        this.write = 0;
        this.step = 1;
        this.started = false;
        this.active = true;
        this.underruns = 0;
        this.lastUnderrunTime = null;
        this.overruns = 0;
        this.port.onmessage = ({data}) => {
            if (data.type === 'stop') { this.active = false; return; }
            if (data.type === 'flush') { this.read = 0; this.write = 0; this.started = false; this.step = 1; return; }
            if (data.type === 'status') {
                this.port.postMessage({underruns:this.underruns,overruns:this.overruns,bufferedFrames:this.write-this.read,clockRatio:this.step,lastUnderrunTime:this.lastUnderrunTime,started:this.started});
                return;
            }
            if (data.type !== 'pcm' || !this.active) return;
            const samples = data.samples;
            const frames = samples.length / 2;
            if (!Number.isInteger(frames) || this.write - Math.floor(this.read) + frames > this.capacity - 4) { this.overruns++; return; }
            for (let frame = 0; frame < frames; frame++) {
                const index = ((this.write + frame) % this.capacity) * 2;
                this.ring[index] = samples[frame*2]; this.ring[index+1] = samples[frame*2+1];
            }
            this.write += frames;
        };
    }
    value(frame,channel) { return this.ring[((frame % this.capacity + this.capacity) % this.capacity)*2+channel]; }
    process(_inputs,outputs) {
        const output = outputs[0];
        if (!this.active) { for (const channel of output) channel.fill(0); return false; }
        if (!this.started && this.write - this.read >= this.target) this.started = true;
        if (!this.started) { for (const channel of output) channel.fill(0); return true; }
        // Slowly track separate input/output clocks. Cubic interpolation is exact
        // at integer positions and avoids dropping/duplicating whole frames.
        const error = (this.write - this.read - this.target) / this.target;
        const desired = 1 + Math.max(-.001,Math.min(.001,error * .0005));
        this.step += (desired - this.step) * .01;
        for (let frame = 0; frame < output[0].length; frame++) {
            const index = Math.floor(this.read), fraction = this.read - index;
            if (index + 2 >= this.write) {
                for (const channel of output) channel.fill(0,frame);
                this.underruns++; this.started = false;
                this.lastUnderrunTime = typeof currentTime === 'number' ? currentTime : null;
                return true;
            }
            for (let channel = 0; channel < this.channels; channel++) {
                const a = this.value(index-1,channel), b = this.value(index,channel), c = this.value(index+1,channel), d = this.value(index+2,channel);
                const c0 = b, c1 = (c-a)*.5, c2 = a-b*2.5+c*2-d*.5, c3 = (d-a)*.5+(b-c)*1.5;
                output[channel][frame] = ((c3*fraction+c2)*fraction+c1)*fraction+c0;
            }
            this.read += this.step;
        }
        return true;
    }
}
registerProcessor('rust-native-input',NativeInputProcessor);
