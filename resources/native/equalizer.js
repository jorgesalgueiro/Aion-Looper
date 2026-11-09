// SPDX-License-Identifier: AGPL-3.0-only
// Presentation only. Commands and response curves use the SAME native settings
// as CLI. No AudioContext, live filter access, analyser or second audio engine.
(() => {
    const presets=['Drums','Low End Clean','Vocal Presence','De-Ess & Clean','Guitar','Master Air','Bass','Radio','Synth Pad','Telephone'];
    const groups=[[
        ['Cut','lcFreq',20,500,20],['ShF','lsFreq',40,1000,100],['ShG','lsGain',-24,24,0],
        ['P1 F','p1Freq',60,4000,1000],['P1 G','p1Gain',-24,24,0],['P1 Q','p1Q',.1,10,.707]
    ],[
        ['P2 F','p2Freq',100,4000,500],['P2 G','p2Gain',-24,24,0],['P2 Q','p2Q',.1,10,.707],
        ['P3 F','p3Freq',200,6000,1000],['P3 G','p3Gain',-24,24,0],['P3 Q','p3Q',.1,10,.707],
        ['P4 F','p4Freq',400,8000,2000],['P4 G','p4Gain',-24,24,0],['P4 Q','p4Q',.1,10,.707]
    ],[
        ['P5 F','p5Freq',1000,12000,4000],['P5 G','p5Gain',-24,24,0],['P5 Q','p5Q',.1,10,.707],
        ['P6 F','p6Freq',2000,16000,8000],['P6 G','p6Gain',-24,24,0],['P6 Q','p6Q',.1,10,.707],
        ['ShF','hsFreq',1500,20000,5000],['ShG','hsGain',-24,24,0],['Cut','hcFreq',3000,22000,20000]
    ]];
    const bands=[['LC','lcFreq',null],['LS','lsFreq','lsGain'],...Array.from({length:6},(_,i)=>[`P${i+1}`,`p${i+1}Freq`,`p${i+1}Gain`]),['HS','hsFreq','hsGain'],['HC','hcFreq',null]];
    const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
    window.AionEqualizerUI=({rpc,enqueue,current,notify})=>{
        let track=0,selected=2,alive=true,serial=0,curveBusy=false,curveKey='',curve=null,drag=null,dirty=true,drawAt=0;
        const tabs=document.getElementById('fx-tabs-container'),container=document.getElementById('fx-controls-container');
        const controls=new Map(),toggles=[];
        const send=(action,data={},key)=>{dirty=true;enqueue(track,{action,...data},key);};
        const select=id=>{if(drag)return;track=id;dirty=true;curve=null;curveKey='';++serial;render();};
        const module=document.createElement('div');module.className='retro-module';module.style.borderColor='#4fd';
        module.innerHTML='<div class="module-header" style="color:#4fd"><span id="native-eq-title">EQ (10-Band) (Q)</span><label><input id="native-eq-enabled" type="checkbox"> eQ</label></div><div class="module-content"><div class="effect-preset-row effect-preset-row-wide"><select id="sel_eq" class="effect-preset-select" aria-label="EQ Preset"><option value="">Load Preset...</option></select><button id="native-eq-reset" type="button">RESET</button></div><canvas id="eqCanvas" class="eq-canvas" width="600" height="240" tabindex="0" role="application" aria-label="EQ curve: click a point, drag frequency and gain; horizontal handles edit bandwidth. Arrow keys move selected point, brackets change Q."></canvas><div id="native-eq-point" style="font-size:10px;color:#4fd;min-height:2em"></div><div id="native-eq-controls" style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px"></div></div>';
        container.append(module);
        const canvas=module.querySelector('#eqCanvas'),context=canvas.getContext('2d'),preset=module.querySelector('#sel_eq'),enabled=module.querySelector('#native-eq-enabled'),readout=module.querySelector('#native-eq-point');
        canvas.style.cssText='width:100%;height:180px;touch-action:none;background:#000;cursor:crosshair';
        const bandSelect=document.createElement('select');bandSelect.id='native-eq-band';bandSelect.setAttribute('aria-label','EQ point');
        bands.forEach(([label],i)=>bandSelect.add(new Option(label,String(i))));bandSelect.value=String(selected);readout.before(bandSelect);
        bandSelect.addEventListener('change',()=>{selected=Number(bandSelect.value);dirty=true;});
        for(const title of presets)preset.add(new Option(title,title));
        preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});
        enabled.addEventListener('change',()=>send('toggle'));
        module.querySelector('#native-eq-reset').addEventListener('click',()=>send('reset'));
        for(let i=0;i<20;++i){const b=document.createElement('button');b.className='tab-btn';b.textContent=`Loop ${i+1}`;b.addEventListener('click',()=>select(i));tabs.append(b);}
        for(const [index,group] of groups.entries()){
            const col=document.createElement('div');col.style.cssText='border:1px solid #222;padding:2px';
            const title=document.createElement('div');title.textContent=['LOWS','MIDS','HIGHS'][index];title.style.cssText=`text-align:center;color:${['#0f0','#ff0','#0ff'][index]};border-bottom:1px solid #222`;col.append(title);
            for(const [label,name,min,max,initial] of group){
                const row=document.createElement('div'),text=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');
                row.style.cssText='display:flex;align-items:center;font-size:9px;margin-bottom:20px';text.textContent=label;text.style.cssText='width:24px;color:#888';
                input.id='eq'+name[0].toUpperCase()+name.slice(1);text.htmlFor=input.id;input.type='range';input.min=min;input.max=max;input.step=name.endsWith('Freq')?1:name.endsWith('Gain')?.1:.01;input.value=initial;input.style.cssText='width:100%;min-width:20px;margin:8px 4px';
                value.id=input.id+'Val';value.style.cssText='width:38px;text-align:right';
                let editing=false;const release=()=>{editing=false;dirty=true;};
                input.addEventListener('pointerdown',()=>{editing=true;});input.addEventListener('pointerup',release);input.addEventListener('pointercancel',release);input.addEventListener('lostpointercapture',release);input.addEventListener('blur',release);
                input.addEventListener('input',()=>{value.textContent=input.value;send('set',{values:{[name]:Number(input.value)}},`eq:${name}`);});
                input.addEventListener('dblclick',()=>send('set',{values:{[name]:initial}},`eq:${name}`));
                controls.set(name,{input,value,editing:()=>editing});row.append(text,input,value);col.append(row);
            }module.querySelector('#native-eq-controls').append(col);
        }
        const positions=()=>{
            const values=current()?.tracks[track]?.equalizer?.values;if(!values)return [];
            const maximum=curve?.maximum??Math.min(22000,(current()?.sampleRate??48000)*.5*(1-1e-6));
            return bands.map(([label,f,g])=>({label,f,g,x:Math.log(clamp(values[f],10,maximum)/10)/Math.log(maximum/10)*canvas.width,y:(24-(g?values[g]:0))/48*canvas.height}));
        };
        const pointAt=e=>{const r=canvas.getBoundingClientRect();return {x:(e.clientX-r.left)*canvas.width/r.width,y:(e.clientY-r.top)*canvas.height/r.height};};
        const frequency=x=>10*Math.pow((curve?.maximum??22000)/10,clamp(x/canvas.width,0,1));
        function move(e){
            if(!drag)return;const p=pointAt(e),b=bands[selected];
            if(drag.edge){send('width',{band:selected-1,edge:frequency(p.x)},'eq:graph');return;}
            const fine=e.shiftKey?.1:1,dx=(p.x-drag.x)*fine,dy=(p.y-drag.y)*fine,values={};
            if(!e.altKey)values[b[1]]=clamp(frequency(drag.start.x+dx),10,22000);
            if(b[2]&&!e.ctrlKey)values[b[2]]=clamp(24-(drag.start.y+dy)/canvas.height*48,-24,24);
            if(Object.keys(values).length)send('set',{values},'eq:graph');
        }
        const finish=e=>{if(!drag)return;if(e?.type==='pointerup')move(e);drag=null;dirty=true;};
        canvas.addEventListener('pointerdown',e=>{
            if(e.button!==0||e.isPrimary===false)return;const p=pointAt(e),points=positions();if(!points.length)return;
            const scale=canvas.width/canvas.getBoundingClientRect().width;
            let nearest=selected;for(const [i,b]of points.entries())if(Math.hypot(p.x-b.x,p.y-b.y)<Math.hypot(p.x-points[nearest].x,p.y-points[nearest].y))nearest=i;
            const distance=Math.hypot(p.x-points[nearest].x,p.y-points[nearest].y);let edgeDistance=Infinity;
            if(selected>=2&&selected<=7&&curve?.ranges){const r=curve.ranges[selected-2],sy=points[selected].y;for(const f of r){const x=Math.log(f/10)/Math.log(curve.maximum/10)*canvas.width;edgeDistance=Math.min(edgeDistance,Math.hypot(p.x-x,p.y-sy));}}
            const edge=edgeDistance<distance&&edgeDistance<12*scale;
            if(!edge){if(distance>20*scale)return;selected=nearest;bandSelect.value=String(selected);}
            e.preventDefault();canvas.focus();canvas.setPointerCapture(e.pointerId);drag={x:p.x,y:p.y,start:points[selected],edge};dirty=true;
        });
        canvas.addEventListener('pointermove',move);canvas.addEventListener('pointerup',finish);canvas.addEventListener('pointercancel',finish);canvas.addEventListener('lostpointercapture',finish);
        window.addEventListener('blur',()=>{finish();for(const c of controls.values())c.input.blur();});
        canvas.addEventListener('keydown',e=>{
            const values=current()?.tracks[track]?.equalizer?.values;if(!values)return;const b=bands[selected],factor=e.shiftKey?.1:1,patch={};
            if(e.key==='ArrowLeft'||e.key==='ArrowRight')patch[b[1]]=clamp(values[b[1]]*Math.pow(2,(e.key==='ArrowLeft'?-1:1)*factor/12),10,22000);
            else if(b[2]&&(e.key==='ArrowUp'||e.key==='ArrowDown'))patch[b[2]]=clamp(values[b[2]]+(e.key==='ArrowUp'?.1:-.1)*factor,-24,24);
            else if(selected>=2&&selected<=7&&(e.key==='['||e.key===']'))patch[`p${selected-1}Q`]=clamp(values[`p${selected-1}Q`]+(e.key==='['?-.1:.1)*factor,.1,10);
            else return;e.preventDefault();e.stopPropagation();send('set',{values:patch},'eq:graph');
        });
        module.addEventListener('keydown',e=>e.stopPropagation());
        function draw(){
            context.clearRect(0,0,canvas.width,canvas.height);context.strokeStyle='#333';context.lineWidth=1;
            for(const db of [-24,-12,0,12,24]){const y=(24-db)/48*canvas.height;context.beginPath();context.moveTo(0,y);context.lineTo(canvas.width,y);context.stroke();context.fillStyle='#777';context.fillText(`${db}dB`,2,clamp(y,10,canvas.height-2));}
            if(curve){context.strokeStyle='#4fd';context.lineWidth=2;context.beginPath();curve.points.forEach((db,i)=>{const x=i/(curve.points.length-1)*canvas.width,y=clamp((24-db)/48*canvas.height,0,canvas.height);i?context.lineTo(x,y):context.moveTo(x,y);});context.stroke();}
            const points=positions();if(!points.length)return;
            if(selected>=2&&selected<=7&&curve?.ranges){const p=points[selected];context.strokeStyle='#ff0';context.beginPath();for(const [i,f] of curve.ranges[selected-2].entries()){const x=Math.log(f/10)/Math.log(curve.maximum/10)*canvas.width;i?context.lineTo(x,p.y):context.moveTo(x,p.y);context.strokeRect(x-4,p.y-6,8,12);}context.stroke();}
            for(const [i,p]of points.entries()){context.fillStyle=i===selected?'#ff0':'#4fd';context.beginPath();context.arc(p.x,p.y,i===selected?6:4,0,Math.PI*2);context.fill();context.fillText(p.label,p.x+7,p.y-7);}
            const b=bands[selected],v=current().tracks[track].equalizer.values;
            readout.textContent=`${b[0]}: ${v[b[1]].toFixed(1)} Hz${b[2]?` / ${v[b[2]].toFixed(1)} dB`:''}${selected>=2&&selected<=7?` / Q ${v[`p${selected-1}Q`].toFixed(2)}`:''} · Shift: fine · Alt: gain · Ctrl: frequency`;
        }
        function render(){
            const s=current(),settings=s?.tracks[track]?.equalizer;if(!settings||!alive)return;
            module.querySelector('#native-eq-title').textContent=`Loop ${track+1} — EQ (10-Band) (Q)`;
            [...tabs.children].forEach((b,i)=>b.classList.toggle('active',i===track));
            enabled.checked=settings.enabled;preset.value=settings.preset;
            for(const [name,c]of controls){const v=settings.values[name];if(!c.editing()&&document.activeElement!==c.input){c.input.min=Math.min(Number(c.input.min),v);c.input.max=Math.max(Number(c.input.max),v);c.input.value=v;}c.value.textContent=Number(v).toFixed(name.endsWith('Freq')?0:name.endsWith('Q')?2:1);}
            for(const t of toggles){t.input.checked=!!s.tracks[t.track].equalizer?.enabled;t.input.disabled=!s.audioReady;}
            const key=JSON.stringify([s.generation,s.sampleRate,track,settings]);
            if(!curveBusy&&key!==curveKey&&Date.now()-drawAt>=100){
                curveBusy=true;drawAt=Date.now();const owner=serial,id=track;
                rpc({operation:'equalizer',action:'curve',track:id}).then(reply=>{
                    if(!alive||serial!==owner||id!==track||key!==JSON.stringify([current()?.generation,current()?.sampleRate,track,current()?.tracks[track]?.equalizer]))return;
                    if(!reply.ok)throw Error(reply.error);curve=reply;curveKey=key;dirty=true;
                }).catch(e=>{if(alive)notify(e.message);}).finally(()=>{curveBusy=false;});
            }
            if(dirty){draw();dirty=false;}
        }
        const reveal=()=>{const sticky=document.querySelector('.sticky-top');canvas.style.scrollMarginTop=`${Math.ceil(sticky?.getBoundingClientRect().height||0)+16}px`;canvas.scrollIntoView({block:'start',behavior:'instant'});};
        function bindRow(row,id){
            const input=row.querySelector(`input[id$="-${id}-eq"]`);if(!input)return;
            input.disabled=false;input.dataset.nativeControl='true';input.title='Toggle eQ';
            input.addEventListener('change',e=>{e.stopPropagation();enqueue(id,{action:'toggle'});select(id);});
            const label=input.nextElementSibling;if(label){label.title='EQ controls';label.addEventListener('click',e=>{e.stopPropagation();select(id);reveal();});}
            const fx=[...row.querySelectorAll('button')].find(b=>b.textContent.trim()==='FX');if(fx){fx.disabled=false;fx.dataset.nativeControl='true';fx.addEventListener('click',e=>{e.stopPropagation();select(id);reveal();});}
            toggles.push({input,track:id});
        }
        window.addEventListener('pagehide',()=>{alive=false;++serial;drag=null;});
        return {bindRow,update(){dirty=true;render();}};
    };
})();
