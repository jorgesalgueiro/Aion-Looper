// Original mapping layout, rendered without legacy Web MIDI.
(()=>{const MAX_LOOPS=20;const state={keyMapping:{kbd:[]}};class KeyMapManager{
    static renderLayout() {
        const container = document.getElementById('keyMappingUI');
        if (!container) return;
        
        // Create the tabbed interface structure
        container.innerHTML = `
            <div class="mapping-tabs">
                <button class="tab-btn active" data-tab="kbd">Keyboard</button>
                <button class="tab-btn" data-tab="midi">MIDI</button>
                <button class="tab-btn" data-tab="gamepad">Gamepad</button>
            </div>

            <div id="tab-kbd" class="tab-content active">
                <div class="mapping-scroll-container"><table class="mapping-table" style="width:100%;">${this.generateTableHTML('kbd')}</table></div>
            </div>
            <div id="tab-midi" class="tab-content">
                <div class="mapping-scroll-container"><table class="mapping-table" style="width:100%;">${this.generateTableHTML('midi')}</table></div>
            </div>
            <div id="tab-gamepad" class="tab-content">
                <div class="mapping-scroll-container"><table class="mapping-table" style="width:100%;">${this.generateTableHTML('gamepad')}</table></div>
            </div>

            <div class="mapping-status" id="mapping-status" style="margin-top:10px; min-height:15px;"></div>
            <div style="margin-top:10px; text-align:center;">
                <button id="saveMappingsBtn" style="width:100%;">Save Mappings</button>
            </div>
        `;
    }
    static generateTableHTML(type) {
        let html = '';
        const droneCount = window.DroneSynth ? DroneSynth.instances.length : 0;
        
        // 33 slots: 20 Loops, 10 Drones, 1 Global OD, 1 Global SUB, 1 Global SUS
        for (let i = 0; i < 33; i++) {
            let label;
            let hidden = false;

            if (i < 20) {
                label = `Loop ${i + 1}`; 
                if (i >= MAX_LOOPS) hidden = true; // Hide unallocated loops
            }
            else if (i < 30) {
                const droneHotkey = state.keyMapping.kbd[i] ? state.keyMapping.kbd[i].toUpperCase() : (i - 19);
                label = `Drone ${droneHotkey}`;
                if ((i - 20) >= droneCount) hidden = true; // Hide unallocated drones
            }
            else if (i === 30) label = `<span style="color:#f0f">Global OD</span>`;
            else if (i === 31) label = `<span style="color:#00cccc">Global SUB</span>`;
            else label = `<span style="color:#ff0">Global SUS</span>`;
            
            if (!hidden) {
            html += `
                <tr>
                    <td style="width:70px;" id="map-lbl-${type}-${i}">${label}</td>
                    <td><input type="text" id="map-${type}-${i}" class="map-input" readonly placeholder="Click to set..." aria-labelledby="map-lbl-${type}-${i}"></td>
                </tr>`;
            }
        }
        return html;
    }
}window.AionMidiMarkup=()=>KeyMapManager.renderLayout();})();
