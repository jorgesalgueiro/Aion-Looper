(() => {
    'use strict';
    const backend = window.__JUCE__?.backend;
    if (!backend) return;
    // Request IDs restart on navigation. A completion from another document
    // must not resolve a newly reused ID. This is correlation, not authentication.
    const session = window.crypto?.randomUUID?.() ?? (window.crypto?.getRandomValues
        ? Array.from(window.crypto.getRandomValues(new Uint32Array(4)),value=>value.toString(16).padStart(8,'0')).join('')
        : `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}-${Math.random().toString(36).slice(2)}`);
    let sequence = 0;
    const pending = new Map();
    backend.addEventListener('__juce__complete', message => {
        const entry = pending.get(message.promiseId);
        if (!entry || message.result?._aionSession !== session) return;
        const {_aionSession, ...result} = message.result;
        pending.delete(message.promiseId); clearTimeout(entry.timer); entry.resolve(result);
    });
    window.AionNative = Object.freeze({
        protocol: 1,
        request(payload) {
            if (!payload || typeof payload !== 'object' || Array.isArray(payload)) return Promise.reject(new Error('Expected one request object'));
            if (pending.size >= 128) return Promise.reject(new Error('Native request queue is full'));
            const id = ++sequence;
            return new Promise((resolve, reject) => {
                const timer = setTimeout(() => { pending.delete(id); reject(new Error('Native request timed out')); }, 5000);
                pending.set(id, {resolve, reject, timer});
                try {
                    backend.emitEvent('__juce__invoke', {name: 'aionRequest', params: [{...payload, _aionSession: session}], resultId: id});
                } catch (error) {
                    // Failed serialization/transport never owns an in-flight slot.
                    pending.delete(id); clearTimeout(timer); reject(error);
                }
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
