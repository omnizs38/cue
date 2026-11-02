// Real browser DOM/fetch with an authoritative mock player; no native filesystem claims.
const {chromium}=require('playwright');
const fs=require('fs'),http=require('http'),assert=require('assert'),path=require('path');
process.chdir(path.join(__dirname,'..'));fs.mkdirSync('build/remote-browser',{recursive:true});
const source=fs.readFileSync('assets/remote/index.html','utf8');
const bmp=Buffer.alloc(54+84*48*3);bmp.write('BM');bmp.writeUInt32LE(bmp.length,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);bmp.writeInt32LE(84,18);bmp.writeInt32LE(48,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);bmp.fill(112,54);
let lang='en',items=[],thumbActive=0,thumbMax=0,queueRequests=0,thumbIDs=[],failQueue=false;
const token='a'.repeat(64),root='C:\\Videos',season=root+'\\Season1';
const server=http.createServer((req,res)=>{
 const url=new URL(req.url,'http://local');
 if(url.pathname==='/font.ttf'){res.end(fs.readFileSync('assets/fonts/CueOmni.ttf'));return;}
 if(['/queue','/queue-thumb','/list'].includes(url.pathname))assert.equal(req.headers.authorization,'Bearer '+token);
 if(url.pathname==='/queue'){queueRequests++;res.setHeader('Content-Type','application/json');if(failQueue){res.statusCode=503;res.end('{}');}else res.end(JSON.stringify({items}));return;}
 if(url.pathname==='/queue-thumb'){
  assert.deepEqual([...url.searchParams.keys()],['id']);const id=+url.searchParams.get('id');thumbIDs.push(id);if(!items.some(i=>i.id===id&&i.preview&&!i.audio)){res.statusCode=404;res.end();return;}
  thumbActive++;thumbMax=Math.max(thumbMax,thumbActive);res.once('close',()=>thumbActive--);
  setTimeout(()=>{res.setHeader('Content-Type','image/bmp');res.end(bmp);},25);return;
 }
 if(url.pathname==='/list'){
  const p=url.searchParams.get('p')||'';
  const j=p===''?{path:'',parent:'',title:'',items:[{dir:true,name:'Videos',path:root}]}:p===root?{path:p,parent:'',title:'Videos',items:[{dir:true,name:'Season 1',path:season}]}:{path:p,parent:root,title:'Videos ▸ Season 1',items:[{name:'Added S01E01.mkv',path:season+'\\Added S01E01.mkv'},{name:'Song.flac',path:season+'\\Song.flac',audio:true}]};
  res.setHeader('Content-Type','application/json');res.end(JSON.stringify(j));return;
 }
 if(url.pathname==='/thumb'){res.statusCode=404;res.end();return;}
 res.setHeader('Content-Type','text/html');res.end(source.replace('/*CUE_BOOT*/','window.CUE='+JSON.stringify({lang,strings:JSON.parse(fs.readFileSync('i18n/'+lang+'.json'))})+';'));
});
function initial(n=16){return Array.from({length:n},(_,i)=>({id:i+1,name:i===2?'Song.flac':i===3?'<script>alert(1)</script>.mkv':`RuPaul's Drag Race - S18E${String(i+1).padStart(2,'0')}.mkv`,current:i===0,audio:i===2,preview:i!==2&&i!==4}));}
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.CUE_CHROMIUM_PATH,args:['--no-sandbox','--disable-dev-shm-usage']});
 const errors=[];
 try{
 for(const language of ['pt-BR','en','es','fr','ru','de','ja','zh-Hans'])for(const [width,height] of [[320,568],[390,844],[844,390]]){
  lang=language;items=initial();queueRequests=0;thumbMax=0;thumbIDs=[];failQueue=false;
  const page=await browser.newPage({viewport:{width,height},isMobile:true,hasTouch:true});page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('cue.token.v2','a'.repeat(64));window.sent=[];window.liveURLs=new Set();
   const create=URL.createObjectURL.bind(URL),revoke=URL.revokeObjectURL.bind(URL);
   URL.createObjectURL=b=>{const u=create(b);window.liveURLs.add(u);return u;};URL.revokeObjectURL=u=>{window.liveURLs.delete(u);revoke(u);};
   window.state={t:'state',view:'play',file:'First.mkv',pos:2,dur:100,speed:1,vol:100,paused:false,ended:false,audioOnly:false,sub:{has:false,on:false},audio:{n:1,i:0},timer:{m:0,left:0},next:'Second.mkv',nextAuto:true,color:'#345678',queueActive:true,queueRevision:1,queueBusy:false,queueNoticeId:0,queueNotice:'',queueResult:0};
   window.publish=extra=>{Object.assign(window.state,extra||{});window.ws.onmessage({data:JSON.stringify(window.state)});};
   window.WebSocket=class{constructor(){window.ws=this;this.readyState=1;setTimeout(()=>this.onopen(),0);}send(text){const m=JSON.parse(text);if(m.t==='auth')setTimeout(()=>{this.onmessage({data:JSON.stringify({t:'hello'})});window.publish();},0);else window.sent.push(m);}close(){}};
  });
  await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForTimeout(60);assert.deepEqual(errors,[]);
  // Wheel modes: tempo (+15 s), volume, and up next while a queue exists (next file).
  assert.equal(await page.locator('#modeDots i').count(),3);
  await page.locator('#bCenter').click();await page.locator('#bCenter').click();
  await page.waitForFunction(()=>document.querySelector('#modeDots i:nth-child(3)').classList.contains('on'));
  await page.locator('#bFwd').click();
  await page.locator('#bCenter').click();
  await page.waitForFunction(()=>document.querySelector('#modeDots i:nth-child(1)').classList.contains('on'));
  await page.locator('#bFwd').click();
  assert.deepEqual((await page.evaluate(()=>window.sent)).slice(-2),[{c:'next'},{c:'skip',v:15}]);
  const baseRequests=queueRequests;  // the wheel's own up-next fetch happened above
  await page.locator('#bQueue').click();await page.waitForFunction(()=>document.querySelectorAll('#queueItems .q-row').length===16);
  await page.waitForFunction(()=>document.querySelector('#queueItems img.ok'));
  const box=await page.locator('#queueSheet').boundingBox();assert(box.x>=0&&box.y>=0&&box.x+box.width<=width+1&&box.y+box.height<=height+1,JSON.stringify({width,height,box}));
  await page.waitForFunction(()=>getComputedStyle(document.querySelector('#wheel')).visibility==='hidden');  // after the crossfade
  assert.equal(await page.locator('#queueItems script').count(),0);
  assert.equal(await page.locator('[data-row-id="3"] img').count(),0);assert.equal(await page.locator('[data-row-id="5"] img').count(),0);assert(thumbMax<=2);
  await page.evaluate(()=>{window.keptRow=document.querySelector('[data-row-id="1"]');for(let i=0;i<20;++i)window.publish({pos:3+i});});
  assert(await page.evaluate(()=>window.keptRow===document.querySelector('[data-row-id="1"]')));assert.equal(queueRequests,baseRequests+1);
  const small=await page.locator('#queueItems .q-remove').first().boundingBox();assert(small.width>=43.9&&small.height>=43.9);
  if(language==='pt-BR'&&width===390){await page.waitForTimeout(300);await page.screenshot({path:'build/remote-browser/queue-polished.png'});}
  await page.locator('[data-row-id="2"] .q-grip').focus();await page.keyboard.press('Control+ArrowUp');
  assert.deepEqual(await page.evaluate(()=>window.sent.at(-1)),{c:'queue-up',v:2});

  await page.locator('#queueAdd').click();await page.waitForFunction(()=>document.querySelector('#grid [data-i="0"]'));
  await page.waitForFunction(()=>document.querySelector('#browseAddFolder').disabled);
  assert.equal(await page.locator('#browseStatus').isVisible(),true);await page.waitForFunction(()=>getComputedStyle(document.querySelector('#wheel')).visibility==='hidden');
  await page.locator('#grid [data-i="0"]').click();await page.waitForFunction(()=>!document.querySelector('#browseAddFolder').disabled);
  if(language==='pt-BR'&&width===390){await page.waitForTimeout(300);await page.screenshot({path:'build/remote-browser/queue-folder-add.png'});}
  await page.locator('#browseAddFolder').click();assert.deepEqual(await page.evaluate(()=>window.sent.at(-1)),{c:'queue-folder',path:season});
  await page.evaluate(()=>window.publish({queueBusy:true,queueNoticeId:1,queueNotice:'reading'}));assert.equal(await page.locator('#browseAddFolder').isDisabled(),true);
  items.push({id:17,name:'Added S01E01.mkv',current:false,preview:true,audio:false});
  await page.evaluate(()=>window.publish({queueBusy:false,queueRevision:2,queueNoticeId:2,queueNotice:'added',queueResult:1}));
  await page.waitForFunction(()=>!document.querySelector('#queueSheet').classList.contains('hide')&&document.querySelectorAll('#queueItems .q-row').length===17);
  assert.equal(await page.locator('#browse').isVisible(),false);assert.equal(await page.evaluate(()=>window.state.file),'First.mkv');
  // Back from adding, the list lands on its end, where the new item is, and marks it briefly.
  assert(await page.evaluate(()=>{const l=document.querySelector('#queueItems');return l.scrollTop>0&&l.scrollTop+l.clientHeight>=l.scrollHeight-2;}));
  assert(await page.evaluate(()=>document.querySelector('[data-row-id="17"]').classList.contains('fresh')));
  await page.evaluate(()=>{document.querySelector('#queueItems').scrollTop=0;});
  // Reorder uses one absolute stable-ID move; grip capture does not play a row.
  const grip=await page.locator('[data-row-id="1"] .q-grip').boundingBox();
  const target=await page.locator('[data-row-id="2"] .q-grip').boundingBox();
  await page.mouse.move(grip.x+grip.width/2,grip.y+grip.height/2);await page.mouse.down();
  await page.mouse.move(grip.x+grip.width/2,target.y+target.height/2+10,{steps:8});await page.mouse.up();
  assert.deepEqual(await page.evaluate(()=>window.sent.at(-1)),{c:'queue-move',v:1,to:1});
  assert.deepEqual(await page.locator('#queueItems [data-row-id]').evaluateAll(es=>es.slice(0,2).map(e=>+e.dataset.rowId)),[2,1]);
  await page.keyboard.press('Escape');
  await page.waitForFunction(()=>document.querySelector('#queueSheet').classList.contains('hide'));
  await page.waitForFunction(()=>window.liveURLs.size===0);
  failQueue=true;const before=queueRequests;await page.locator('#bQueue').click();await page.waitForTimeout(180);assert.equal(queueRequests,before+1); // no hot retry loop on failure
  await page.locator('#queueClose').click();await page.waitForFunction(()=>document.querySelector('#queueSheet').classList.contains('hide'));failQueue=false;
  items=[];await page.evaluate(()=>window.publish({queueRevision:3,queueActive:false,next:'',queueNotice:''}));await page.locator('#bQueue').click();
  await page.waitForFunction(()=>!document.querySelector('#queueEmpty').classList.contains('hide'));
  const title=await page.locator('#queueTitle').textContent();assert(!/ · [01]$/.test(title));assert.equal(await page.locator('#queueClear').isVisible(),false);
  await page.locator('#queueClose').click();await page.waitForFunction(()=>document.querySelector('#queueSheet').classList.contains('hide'));
  await page.waitForFunction(()=>window.liveURLs.size===0);
  if(language==='pt-BR'&&width===390){
    items=initial(512);await page.evaluate(()=>window.publish({queueRevision:4,queueActive:true,next:'Second.mkv'}));await page.locator('#bQueue').click();
    await page.waitForFunction(()=>document.querySelectorAll('#queueItems .q-row').length===512);
    for(let i=0;i<12;++i){await page.locator('#queueItems').evaluate(el=>{el.scrollTop+=el.clientHeight*.85;});await page.waitForTimeout(80);assert(await page.evaluate(()=>window.liveURLs.size<=32));}
    items=[];await page.evaluate(()=>window.publish({queueRevision:5,queueActive:false,next:''}));await page.waitForFunction(()=>document.querySelectorAll('#queueItems .q-row').length===0);
    assert(await page.locator('#queueGhosts .q-row').count()<=12);
    await page.emulateMedia({reducedMotion:'reduce'});await page.waitForTimeout(30);
    assert.equal(await page.locator('#queueGhosts .q-row').count(),0);assert.equal(await page.locator('#queueSheet').evaluate(el=>el.getAnimations({subtree:true}).filter(a=>a.playState==='running').length),0);
    await page.locator('#queueClose').click();await page.waitForFunction(()=>window.liveURLs.size===0);
    items=initial(1);await page.evaluate(()=>window.publish({queueRevision:6,queueActive:true,next:''}));await page.locator('#bQueue').click();
    await page.waitForFunction(()=>document.querySelectorAll('#queueItems .q-row').length===1);assert(!/ · 1$/.test(await page.locator('#queueTitle').textContent()));
    await page.locator('#queueClose').click();await page.waitForFunction(()=>window.liveURLs.size===0);
    await page.emulateMedia({reducedMotion:'no-preference'});items=initial();await page.evaluate(()=>window.publish({queueRevision:7,queueActive:true,next:'Second.mkv'}));
    for(let i=0;i<10;++i){await page.locator('#bQueue').click();await page.locator('#queueClose').click();await page.waitForFunction(()=>document.querySelector('#queueSheet').classList.contains('hide'));assert.equal(await page.evaluate(()=>window.liveURLs.size),0);}
    console.log('queue stress passed: 512 rows, <=32 preview URLs, <=12 exit rows, reduced motion and 10 rapid reopen cycles');
  }
  await page.close();console.log('polish passed',language,width,height);
 }
 assert.deepEqual(errors,[]);
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
