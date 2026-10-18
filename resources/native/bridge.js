(() => {
    'use strict';
    const backend = window.__JUCE__?.backend;
    if (!backend) return;
    let sequence = 0;
    const pending = new Map();
    backend.addEventListener('__juce__complete', message => {
        const entry = pending.get(message.promiseId);
        if (!entry) return;
        pending.delete(message.promiseId); clearTimeout(entry.timer); entry.resolve(message.result);
    });
    window.AionNative = Object.freeze({
        protocol: 1,
        request(payload) {
            if (pending.size >= 128) return Promise.reject(new Error('Native request queue is full'));
            const id = ++sequence;
            return new Promise((resolve, reject) => {
                const timer = setTimeout(() => { pending.delete(id); reject(new Error('Native request timed out')); }, 5000);
                pending.set(id, {resolve, reject, timer});
                backend.emitEvent('__juce__invoke', {name: 'aionRequest', params: [payload], resultId: id});
            });
        }
    });
    window.addEventListener('pagehide', () => {
        for (const entry of pending.values()) {clearTimeout(entry.timer); entry.reject(new Error('Editor closed'));}
        pending.clear();
    });
    document.addEventListener('DOMContentLoaded', () => {
        document.querySelectorAll('button,input,select,textarea').forEach(element => element.disabled = true);
    });
})();
