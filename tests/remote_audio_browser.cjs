// Optional browser regression test: while music plays, the up-next card never covers the face
// (it still appears for video); a song's cover becomes the face and the side buttons step through
// the songs. Requires playwright and Chromium. Run from the source root.
const {chromium} = require('playwright');
const fs = require('fs');
const http = require('http');
const assert = require('assert');
const path = require('path');
process.chdir(path.join(__dirname,'..'));
const html = fs.readFileSync('assets/remote/index.html','utf8');
const page_html = html.replace('<script>','<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/','window.CUE='+JSON.stringify({lang:'pt-BR',strings:JSON.parse(fs.readFileSync('i18n/pt-BR.json','utf8'))})+';');
const server = http.createServer((req,res) => {
  if (req.url === '/font.ttf') { res.setHeader('Content-Type','font/ttf'); res.end(fs.readFileSync('assets/fonts/CueOmni.ttf')); return; }
  if (req.url.startsWith('/cover')) {
    // a 2x2 BMP: enough for the page to show a picture and read its colour
    const b = Buffer.alloc(54 + 16); b.write('BM'); b.writeUInt32LE(b.length, 2); b.writeUInt32LE(54, 10); b.writeUInt32LE(40, 14);
    b.writeInt32LE(2, 18); b.writeInt32LE(2, 22); b.writeUInt16LE(1, 26); b.writeUInt16LE(24, 28); b.writeUInt32LE(16, 34);
    for (let i = 54; i < b.length; i += 8) { b[i] = 40; b[i+1] = 80; b[i+2] = 200; b[i+3] = 40; b[i+4] = 80; b[i+5] = 200; }
    res.setHeader('Content-Type','image/bmp'); res.end(b); return;
  }
  if (req.url === '/queue') { res.setHeader('Content-Type','application/json'); res.end(JSON.stringify({items:[{id:1,name:'01 a.flac',current:false,audio:true},{id:2,name:'song.flac',current:true,audio:true},{id:3,name:'other song.flac',current:false,audio:true}]})); return; }
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  res.setHeader('Content-Type','text/html'); res.end(page_html);
});
(async () => {
  await new Promise(r => server.listen(8133,'127.0.0.1',r));
  const browser = await chromium.launch({headless:true,executablePath:process.env.CUE_CHROMIUM_PATH||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
  const card = async audio => {
    const context = await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
    const page = await context.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(audio => {
      localStorage.setItem('cue.token.v2','a'.repeat(64));
      window.WebSocket = class {
        constructor(){ this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); }, 5); }
        send(data){ const m = JSON.parse(data); setTimeout(() => { if (m.t === 'auth') {
          this.onmessage?.({data:JSON.stringify({t:'hello',pc:'Cue PC'})});
          this.onmessage?.({data:JSON.stringify({t:'state',view:'play',file:audio?'song.flac':'Show S01E01.mkv',pos:150,dur:200,speed:1,vol:100,paused:false,ended:false,audioOnly:audio,
            sub:{has:false,on:false,delay:0,size:44,sizeMin:28,sizeMax:72,pos:93,posDefault:93,posMin:12,posMax:97},audio:{n:1,i:0},timer:{m:0,left:0},
            next:audio?'other song.flac':'Show S01E02.mkv',nextAuto:true,queueActive:true,color:'#a0508f',cover:audio?77:0})});
        } else window.__sent = (window.__sent || []).concat([m]); }, 5); }
        close(){ this.readyState = 3; }
      };
    }, audio);
    await page.goto('http://127.0.0.1:8133/'); await page.waitForTimeout(400);
    const shown = await page.evaluate(() => !document.getElementById('next').classList.contains('hide'));
    const face = await page.evaluate(() => ({cover: !document.getElementById('coverFace').classList.contains('hide'),
      eyes: !document.getElementById('eyeL').classList.contains('hide')}));
    if (audio) {
      assert.ok(face.cover && !face.eyes, 'music: the cover is the face');
      assert.equal(await page.evaluate(() => document.getElementById('rightNote').textContent), '3:20', 'music: its length, not when it ends');
      await page.click('#bFwd'); await page.waitForTimeout(150);
      const sent = await page.evaluate(() => (window.__sent || []).map(m => m.c));
      assert.ok(sent.includes('next') && !sent.includes('skip'), 'music: the side button goes to the next song, not 15 s');
    } else assert.ok(!face.cover && face.eyes, 'video: the eyes stay');
    await context.close();
    assert.deepEqual(errors, []);
    return shown;
  };
  assert.equal(await card(false), true, 'video: the up-next card shows near the end');
  assert.equal(await card(true), false, 'music: the face stays clear');
  await browser.close(); server.close();
  console.log('remote audio: passed');
})().catch(e => { console.error(e); process.exit(1); });
