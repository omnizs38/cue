// Optional browser regression test: when Cue's language changes on the computer, an open phone page
// reloads once to receive the new words, and never loops if the page cannot follow.
// Requires playwright and Chromium. Run from the source root.
const {chromium} = require('playwright');
const fs = require('fs');
const http = require('http');
const assert = require('assert');
const path = require('path');
process.chdir(path.join(__dirname,'..'));
const html = fs.readFileSync('assets/remote/index.html','utf8');
const page_for = lang => html.replace('<script>','<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/','window.CUE='+JSON.stringify({lang,strings:JSON.parse(fs.readFileSync('i18n/'+lang+'.json','utf8'))})+';');
let served = [], next_lang = () => 'pt-BR';
const server = http.createServer((req,res) => {
  if (req.url === '/font.ttf') { res.setHeader('Content-Type','font/ttf'); res.end(fs.readFileSync('assets/fonts/CueOmni.ttf')); return; }
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  const lang = next_lang(); served.push(lang);
  res.setHeader('Content-Type','text/html'); res.end(page_for(lang));
});
(async () => {
  await new Promise(r => server.listen(8131,'127.0.0.1',r));
  const browser = await chromium.launch({headless:true,executablePath:process.env.CUE_CHROMIUM_PATH||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
  const run = async () => {
    const context = await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
    const page = await context.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => {
      localStorage.setItem('cue.token.v2','a'.repeat(64));
      window.WebSocket = class {
        constructor(){ this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); }, 5); }
        send(data){ const m = JSON.parse(data); if (m.t === 'auth') setTimeout(() => {
          this.onmessage?.({data:JSON.stringify({t:'hello',pc:'Cue PC'})});
          this.onmessage?.({data:JSON.stringify({t:'state',view:'home',file:'',pos:0,dur:0,speed:1,vol:100,paused:true,ended:false,audioOnly:false,
            sub:{has:false,on:false,delay:0,size:44,sizeMin:28,sizeMax:72,pos:93,posDefault:93,posMin:12,posMax:97},audio:{n:0,i:0},timer:{m:0,left:0},
            next:'',lang:'en',color:'#a0508f'})});
        }, 5); }
        close(){ this.readyState = 3; }
      };
    });
    await page.goto('http://127.0.0.1:8131/'); await page.waitForTimeout(900);
    const lang = await page.evaluate(() => document.documentElement.lang);
    await context.close();
    assert.deepEqual(errors, []);
    return lang;
  };
  // the computer switched to English: the Portuguese page reloads once and speaks English
  served = []; let first = true; next_lang = () => first ? (first = false, 'pt-BR') : 'en';
  assert.equal(await run(), 'en'); assert.deepEqual(served, ['pt-BR','en']);
  // a page that keeps arriving in the old language reloads only once
  served = []; next_lang = () => 'pt-BR';
  assert.equal(await run(), 'pt-BR'); assert.deepEqual(served, ['pt-BR','pt-BR']);
  // same language: no reload
  served = []; next_lang = () => 'en';
  assert.equal(await run(), 'en'); assert.deepEqual(served, ['en']);
  await browser.close(); server.close();
  console.log('remote language: passed');
})().catch(e => { console.error(e); process.exit(1); });
