// Queue behavior with a mock player and real Chromium DOM/fetch/layout.
const {chromium}=require('playwright');
const fs=require('fs'),http=require('http'),assert=require('assert'),path=require('path');
process.chdir(path.join(__dirname,'..'));
const html=fs.readFileSync('assets/remote/index.html','utf8');
let language='en',delay=0;
const items=[{id:41,name:'First.mkv',current:true},{id:73,name:'<script>second</script>.mp3',current:false}];
const server=http.createServer((req,res)=>{
 if(req.url==='/font.ttf'){res.end(fs.readFileSync('assets/fonts/CueOmni.ttf'));return;}
 if(req.url==='/queue'){
  assert.equal(req.headers.authorization,'Bearer '+'a'.repeat(64));
  return setTimeout(()=>{res.setHeader('Content-Type','application/json');res.end(JSON.stringify({items}));},delay);
 }
 if(req.url.startsWith('/list')){const p=new URL(req.url,'http://local').searchParams.get('p')||'';res.setHeader('Content-Type','application/json');res.end(JSON.stringify(p?{path:p,parent:'',title:'Videos',items:[{name:'Added.mp4',path:'C:\\Videos\\Added.mp4'}]}:{path:'',parent:'',title:'',items:[{name:'Videos',path:'C:\\Videos',dir:true}]}));return;}
 const catalog=JSON.parse(fs.readFileSync('i18n/'+language+'.json','utf8'));
 res.setHeader('Content-Type','text/html');res.end(html.replace('/*CUE_BOOT*/','window.CUE='+JSON.stringify({lang:language,strings:catalog})+';'));
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.CUE_CHROMIUM_PATH,args:['--no-sandbox','--disable-dev-shm-usage']});
 const errors=[];
 try{
 for(const lang of ['en','pt-BR','es','fr','zh-Hans','ru','de','ja'])for(const [width,height] of [[320,568],[390,844],[844,390]]){
  language=lang;delay=0;
  const page=await browser.newPage({viewport:{width,height}});
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('cue.token.v2','a'.repeat(64));window.sent=[];
   window.WebSocket=class {constructor(){window.ws=this;this.readyState=1;setTimeout(()=>this.onopen(),0);}
    send(text){const m=JSON.parse(text);if(m.t==='auth'){setTimeout(()=>{this.onmessage({data:JSON.stringify({t:'hello'})});window.publish();},0);}else window.sent.push(m);}
    close(){}
   };
   let rev=1;window.publish=()=>window.ws.onmessage({data:JSON.stringify({t:'state',view:'play',file:'First.mkv',pos:2,dur:100,speed:1,vol:100,paused:false,ended:false,audioOnly:false,sub:{has:false,on:false},audio:{n:1,i:0},timer:{m:0,left:0},next:'second.mp3',nextAuto:true,color:'#123456',queueRevision:rev++})});
  });
  await page.goto('http://127.0.0.1:'+server.address().port);
  await page.locator('#bQueue').click();
  await page.waitForFunction(()=>document.querySelectorAll('#queueItems [data-qcmd="queue-play"]').length===2);
  assert.equal(await page.locator('#queueItems script').count(),0);
  assert(!await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth));
  const sizes=await page.locator('#queueItems .icon:visible').evaluateAll(es=>es.map(e=>{const b=e.getBoundingClientRect();return[b.width,b.height];}));
  assert(sizes.every(([w,h])=>w>=43.9&&h>=43.9),JSON.stringify({lang,width,sizes}));
  // Row: drag zone (handle + thumbnail) on the left, title plays, x removes; Ctrl+Up on the handle moves.
  await page.locator('[data-row-id="73"] .q-grip').focus();await page.keyboard.press('Control+ArrowUp');
  for(const cmd of ['queue-remove','queue-play'])await page.locator('[data-qid="73"][data-qcmd="'+cmd+'"]').click();
  assert.deepEqual((await page.evaluate(()=>window.sent)).slice(-3),[{c:'queue-up',v:73},{c:'queue-remove',v:73},{c:'queue-play',v:73}]);
  const zone=await page.locator('[data-row-id="73"] .q-grip').boundingBox(),remove=await page.locator('[data-row-id="73"] .q-remove').boundingBox();
  const sheet=await page.locator('#queueSheet').boundingBox();
  assert(zone.x<sheet.x+sheet.width/3&&zone.width>=80&&remove.x+remove.width>sheet.x+sheet.width*0.8,JSON.stringify({zone,remove,sheet}));
  await page.locator('#queueAdd').click();await page.waitForSelector('#grid [data-i="0"]');
  // Adding mode: a done button instead of close, a footer for the whole folder, and no open-mode toggle.
  assert(await page.locator('#browseDone').isVisible()&&!await page.locator('#browseClose').isVisible());
  assert(await page.locator('#browseAddFolder').isVisible());
  await page.locator('#grid [data-i="0"]').click();
  assert.deepEqual(await page.evaluate(()=>window.sent.at(-1)),{c:'queue-add',path:'C:\\Videos\\Added.mp4'});
  await page.waitForFunction(()=>document.querySelector('#grid [data-i="0"]').classList.contains('queued'));
  const count=await page.evaluate(()=>window.sent.length);
  await page.locator('#grid [data-i="0"]').click();  // a second tap on a marked file sends nothing
  assert.equal(await page.evaluate(()=>window.sent.length),count);
  assert((await page.locator('#browseStatus').textContent()).endsWith('· 1'));
  await page.locator('#browseAddFolder').click();
  assert.deepEqual(await page.evaluate(()=>window.sent.at(-1)),{c:'queue-folder',path:'C:\\Videos'});
  await page.locator('#browseDone').click();
  await page.waitForFunction(()=>!document.querySelector('#queueSheet').classList.contains('hide')&&document.querySelector('#browse').classList.contains('hide'));
  await page.locator('#queueClose').click();await page.waitForFunction(()=>document.querySelector('#queueSheet').classList.contains('hide'));
  delay=100;await page.locator('#bQueue').click();await page.locator('#queueClose').click();await page.waitForTimeout(260);  // after the 200 ms close
  assert.equal(await page.locator('#queueItems').textContent(),'');
  await page.close();console.log('queue passed',lang,width,height);
 }
 assert.deepEqual(errors,[]);
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
