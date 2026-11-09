// Async ordering and visual cadence against the production page, without browser dependencies.
const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const root=process.env.CUE_SOURCE_ROOT||path.join(__dirname,'..');
const html=fs.readFileSync(path.join(root,'assets/remote/index.html'),'utf8');
const section=(a,b)=>html.slice(html.indexOf(a),html.indexOf(b,html.indexOf(a)));
function fixture(){
 let requests=[],timers=[],frames=0,draws=0,urls=0;const nodes=new Map();
 const node=id=>{if(!nodes.has(id))nodes.set(id,{src:'',onload:null,removeAttribute(){this.src='';},classList:{add(){},remove(){},toggle(){}}});return nodes.get(id);};
 const c={console,AbortController,performance:{now:()=>10000},document:{hidden:false},S:{view:'play',audioOnly:true,cover:1,queueRevision:1},U:{pick:0},linkReady:true,
  trackFetch:null,trackItems:[],trackRev:-1,coverRetry:0,coverRev:0,coverURL:'',coverOK:false,coverTint:'',coverFetch:null,disc:{src:null,key:'',k:1,ang:0,want:1,on:true,raf:0,t:0},
  pickT:0,pickFetch:null,peekEpoch:0,peekURL:null,reduced:false,authHeaders:()=>({}),render(){},replay(){},averageColor:()=> '#fff',clearPeek(){++c.peekEpoch;},$:node,
  URL:{createObjectURL:()=>{++urls;return 'blob:'+urls;},revokeObjectURL(){}},
  setTimeout:fn=>{timers.push(fn);return timers.length;},clearTimeout:i=>{if(i)timers[i-1]=null;},
  requestAnimationFrame:()=>++frames,cancelAnimationFrame(){},discSource:()=>true,discDraw:()=>{++draws;},
  fetch:(url,options)=>new Promise((resolve,reject)=>requests.push({url,options,resolve,reject}))};
 vm.createContext(c);
 vm.runInContext(section('function loadTracks(', 'function musicSteps(')+section('function syncCover()', 'function averageColor(')+section('function pickPreview()', 'function stepTrack('),c);
 return {c,node,requests,flush:()=>new Promise(r=>setImmediate(r)),timers,frames:()=>frames,draws:()=>draws,urls:()=>urls,
  discCode:()=>vm.runInContext(section('function discTo(', 'function syncCover()'),c)};
}
(async()=>{
 {
  const f=fixture();f.c.loadTracks();f.c.S.queueRevision=2;
  f.requests[0].resolve({ok:true,json:async()=>({items:[{id:1}]})});await f.flush();
  assert.strictEqual(f.c.trackRev,-1,'a superseded queue revision must not enter the wheel');
  assert.strictEqual(f.c.trackItems.length,0);
 }
 {
  const f=fixture();f.c.syncCover();const first=f.c.coverFetch;f.c.S.cover=2;f.c.syncCover();const second=f.c.coverFetch;
  assert(first.signal.aborted);
  f.requests[0].resolve({ok:true,blob:async()=>({})});await f.flush();
  assert.strictEqual(f.c.coverFetch,second,'old finally must not clear the new cover controller');assert.strictEqual(f.urls(),0);
  f.requests[1].resolve({ok:true,blob:async()=>({})});await f.flush();assert.strictEqual(f.urls(),1);
  f.c.S.cover=0;f.c.syncCover();assert.strictEqual(f.c.coverURL,'','leaving music must release the cover URL');
 }
 {
  const f=fixture();f.c.syncCover();f.requests[0].reject(Error('503'));await f.flush();assert.strictEqual(f.c.coverRev,0);
  f.c.syncCover();assert.strictEqual(f.requests.length,1,'failed cover must back off');f.c.coverRetry=0;f.c.syncCover();assert.strictEqual(f.requests.length,2,'cover failure must be retryable');
 }
 {
  const f=fixture();f.c.trackItems=[{id:1,preview:true},{id:2,preview:true}];
  f.c.pickPreview();f.timers[0]();
  f.c.U.pick=1;f.c.pickPreview();
  f.requests[0].resolve({ok:true,blob:async()=>({})});await f.flush();
  assert.strictEqual(f.urls(),0,'old selection preview must be invalidated at selection, before the next debounce ends');
 }
 {
  const f=fixture();f.discCode();f.c.discStep(10000);
  assert.strictEqual(f.frames(),0,'a settled disc must not schedule another frame');assert.strictEqual(f.draws(),1);
  f.c.discTo(false);assert.strictEqual(f.frames(),1,'release must still animate the unfold');
 }
 {
  const f=fixture(),requests=[];let releases=0;
  f.c.navigator={wakeLock:{request:()=>new Promise(resolve=>requests.push(resolve))}};
  f.c.linkAvailable=()=>!f.c.document.hidden;f.c.wakeLink=()=>{};f.c.document.addEventListener=()=>{};
  vm.runInContext(section('var lock = null,', '// ── go '),f.c);
  f.c.keepAwake();f.c.keepAwake();assert.strictEqual(requests.length,1,'concurrent touches must share one pending wake-lock request');
  f.c.document.hidden=true;requests[0]({release(){++releases;return Promise.resolve();},addEventListener(){}});await f.flush();
  assert.strictEqual(releases,1,'a lock delivered after suspension must be released');assert.strictEqual(f.c.lock,null);
  f.c.document.hidden=false;f.c.keepAwake();requests[1]({release(){++releases;return Promise.resolve();},addEventListener(){}});await f.flush();
  f.c.releaseAwake();assert.strictEqual(releases,2);assert.strictEqual(f.c.lock,null);
 }
 console.log('remote async: stale queue revisions, cover cancellation/retry/release, selection debounce and settled-disc cadence passed');
})().catch(e=>{console.error(e);process.exitCode=1});
