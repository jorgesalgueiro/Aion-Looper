// SPDX-License-Identifier: AGPL-3.0-only
// Original manual chain field; native owns routing and saved settings.
(()=>{
 window.AionSignalChainUI=({current,enqueue,select})=>{
  const fields=[],groups=[];
  const keys={Q:'eq',C:'compressor',A:'arpDelay',T:'distortion',F:'fuzz',O:'overdrive',D:'delay',B:'reverb',V:'machineReverb',K:'dusk',Z:'zigZ',G:'griz',H:'harmony'};
  return {
   bindRow(row,track){
    const container=row.querySelector(`[id$="-fx-container-${track}"]`);
    if(container){
     const prefix=container.id.startsWith('live-')?'live-loop':'loop',template=document.createElement('div');
     template.innerHTML=window.AionLoopEffectsMarkup(track,prefix);
     // Keep original toggle nodes/listeners across reorder/omission. Populate
     // absent codes once, before the EQ/C binders attach their shared controls.
     for(const node of template.querySelectorAll('*'))for(const attr of [...node.attributes])if(attr.name.startsWith('on'))node.removeAttribute(attr.name);
     for(const key of Object.values(keys))if(!container.querySelector(`[id$="-${key}"]`)){
      const input=template.querySelector(`[id$="-${key}"]`);input.disabled=true;input.title='Unavailable in this preview';container.append(input.parentElement);
     }
     const nodes=new Map([...container.children].map(span=>[span.querySelector('input').id.split('-').at(-1),span]));
     groups.push({container,track,nodes,order:null});
    }
    const input=row.querySelector('[aria-label="Loop FX Chain"]');if(!input)return;
    input.disabled=false;input.maxLength=256;input.dataset.nativeControl='true';
    input.title='Manual FX Chain — Q and C are ported; other codes retain their position';
    input.addEventListener('click',e=>{e.stopPropagation();select(track);});
    input.addEventListener('keydown',e=>e.stopPropagation());
    input.addEventListener('change',e=>{e.stopPropagation();enqueue(track,input.value);});
    fields.push({input,track});
   },
   update(){
    const snapshot=current();if(!snapshot)return;
    for(let i=groups.length-1;i>=0;--i){
     const group=groups[i];if(!group.container.isConnected){groups.splice(i,1);continue;}
     const order=snapshot.tracks[group.track].signalChain;if(order===group.order)continue;
     const wanted=['reverse',...[...order].map(code=>keys[code])];
     for(const [key,node] of group.nodes)if(!wanted.includes(key))node.remove();
     for(const key of wanted){const node=group.nodes.get(key);if(node)group.container.append(node);}
     group.order=order;
    }
    for(const {input,track} of fields){
     input.disabled=!snapshot.audioReady;
     if(document.activeElement!==input)input.value=snapshot.tracks[track].signalChain;
    }
   }
  };
 };
})();
