// Optional full-page browser validation. Requires Playwright and Chromium.
// Uses a mock PC; Windows server/socket and physical phone validation remain separate.
const {chromium}=require('playwright');
const fs=require('fs'),http=require('http'),path=require('path'),assert=require('assert');
process.chdir(path.join(__dirname,'..'));
const html=fs.readFileSync('assets/remote/index.html','utf8').replace('/*CUE_BOOT*/','window.CUE='+JSON.stringify({lang:'pt-BR',strings:JSON.parse(fs.readFileSync('i18n/pt-BR.json'))})+';');
let lists=0;
const server=http.createServer((req,res)=>{
 if(req.url==='/font.ttf'){res.end(fs.readFileSync('assets/fonts/CueOmni.ttf'));return;}
 if(req.url.startsWith('/list')){++lists;res.setHeader('Content-Type','application/json');res.end(JSON.stringify({path:'',parent:'',title:'Folders',items:[{dir:true,name:'Videos',path:'C:\\Videos'},{dir:true,name:'Music',path:'C:\\Music'}]}));return;}
 if(req.url==='/queue'){res.setHeader('Content-Type','application/json');res.end('{"items":[]}');return;}
 res.setHeader('Content-Type','text/html');res.end(html);
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({executablePath:process.env.CUE_CHROMIUM_PATH||undefined,headless:true,args:['--no-sandbox','--disable-dev-shm-usage']});
 try{for(const [width,height]of [[320,568],[390,844],[844,390]]){
  const page=await browser.newPage({viewport:{width,height},isMobile:true,hasTouch:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
   localStorage.setItem('cue.token.v2','a'.repeat(64));window.__sockets=[];
   window.WebSocket=class{
    constructor(){this.readyState=0;this.bufferedAmount=0;window.__sockets.push(this);setTimeout(()=>{this.readyState=1;this.onopen?.();},5);}
    send(s){const m=JSON.parse(s);if(m.t==='auth')setTimeout(()=>this.onmessage?.({data:'{"t":"hello","pc":"test"}'}),5);if(m.t==='ping')this.onmessage?.({data:'{"t":"pong"}'});}
    close(){this.readyState=3;}
   };
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.__sockets[0]?.readyState===1);
  await page.locator('#bFolder').click();await page.waitForFunction(()=>document.querySelectorAll('#grid .tile').length===2);
  const before=lists;
  await page.evaluate(()=>{Object.defineProperty(document,'hidden',{value:true,configurable:true});document.dispatchEvent(new Event('visibilitychange'));});
  await page.waitForTimeout(600);assert.strictEqual(lists,before);
  await page.evaluate(()=>{Object.defineProperty(document,'hidden',{value:false,configurable:true});document.dispatchEvent(new Event('visibilitychange'));});
  await page.waitForFunction(()=>window.__sockets.length===2&&document.querySelectorAll('#grid .tile').length===2);
  assert.strictEqual(await page.evaluate(()=>window.__sockets[0].readyState),3);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await page.locator('#bFolder').click();await page.setViewportSize({width:height,height:width});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  assert.deepStrictEqual(errors,[]);console.log('reconnect + folders + orientation',width,height,'passed');await page.close();
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>server.close());
