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
  if (process.env.CUE_COVER && req.url.startsWith('/cover')) { res.setHeader('Content-Type','image/bmp'); res.end(fs.readFileSync(process.env.CUE_COVER)); return; }
  if (req.url.startsWith('/cover') || req.url.startsWith('/queue-thumb')) {
    // a 2x2 BMP: enough for the page to show a picture and read its colour
    const b = Buffer.alloc(54 + 16); b.write('BM'); b.writeUInt32LE(b.length, 2); b.writeUInt32LE(54, 10); b.writeUInt32LE(40, 14);
    b.writeInt32LE(2, 18); b.writeInt32LE(2, 22); b.writeUInt16LE(1, 26); b.writeUInt16LE(24, 28); b.writeUInt32LE(16, 34);
    for (let i = 54; i < b.length; i += 8) { b[i] = 40; b[i+1] = 80; b[i+2] = 200; b[i+3] = 40; b[i+4] = 80; b[i+5] = 200; }
    res.setHeader('Content-Type','image/bmp'); res.end(b); return;
  }
  if (req.url === '/queue') { res.setHeader('Content-Type','application/json'); res.end(JSON.stringify({items:[{id:1,name:'01 a.flac',current:false,audio:true,preview:true},{id:2,name:'song.flac',current:true,audio:true},{id:3,name:'other song.flac',current:false,audio:true}]})); return; }
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
            next:audio?'other song.flac':'Show S01E02.mkv',nextAuto:true,queueActive:true,color:'#a0508f',cover:audio?77:0,title:audio?'Night Swim':undefined,artist:audio?'tinypeep \u00b7 demo days':undefined})});
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
      // its title and artist, like the computer, not the file's name
      assert.deepEqual(await page.evaluate(() => [document.getElementById('file').textContent, document.getElementById('artist').textContent, document.getElementById('artist').classList.contains('hide')]),
        ['Night Swim', 'tinypeep \u00b7 demo days', false]);
      // scrubbing turns the cover into a record that turns with the thumb (the stage never goes
      // black); the big time stays away. Seven or eight detents of the wheel: 7.5 degrees each.
      const w = await page.locator('#wheel').boundingBox(), cx = w.x + w.width / 2, cy = w.y + w.height / 2, R = w.width * .43;
      const at = deg => [cx + R * Math.sin(deg * Math.PI / 180), cy - R * Math.cos(deg * Math.PI / 180)];
      await page.mouse.move(...at(0)); await page.mouse.down();
      for (let d = 4; d <= 122; d += 4) { await page.mouse.move(...at(d)); await page.waitForTimeout(16); }
      await page.waitForTimeout(400);
      const vinyl = await page.evaluate(() => { const f = document.getElementById('coverFace'), d = document.getElementById('spinDisc');
        const px = d.getContext('2d').getImageData(0, 0, 72, 72).data;
        return [!d.classList.contains('hide'), f.classList.contains('disc'), document.getElementById('scrub').classList.contains('hide'),
                document.getElementById('spinNote').textContent, px[3], px[(36 * 72 + 20) * 4 + 3], px[(36 * 72 + 36) * 4]]; });
      // a disc: the corner is empty, the body is there, the spindle hole is dark
      assert.ok(vinyl[0] && vinyl[1] && vinyl[2] && /^\+\d+:\d\d$/.test(vinyl[3]) && vinyl[4] === 0 && vinyl[5] === 255 && vinyl[6] < 20, 'scrubbing a song: ' + vinyl);
      if (process.env.CUE_SHOT) await page.screenshot({path: process.env.CUE_SHOT});
      await page.mouse.up(); await page.waitForTimeout(1200);
      assert.deepEqual(await page.evaluate(() => [document.getElementById('spinDisc').classList.contains('hide'), document.getElementById('coverFace').classList.contains('disc'),
        document.getElementById('coverFace').classList.contains('hide'), document.getElementById('spinNote').classList.contains('hide')]),
        [true, false, false, true], 'the record turns back into the cover');
      // the time on the right: a song's length; a tap shows what is left, and it is remembered
      await page.click('#rightNote');
      assert.ok(/^-0:\d\d$/.test(await page.evaluate(() => document.getElementById('rightNote').textContent)), 'a tap: what is left');
      await page.click('#rightNote');
      assert.equal(await page.evaluate(() => document.getElementById('rightNote').textContent), '3:20');
      await page.evaluate(() => { window.__sent = []; });
      await page.click('#bFwd'); await page.waitForFunction(() => (window.__sent || []).length > 0, null, {timeout: 2000}).catch(() => {}); await page.waitForTimeout(60);
      const sent = await page.evaluate(() => (window.__sent || []).map(m => m.c));
      assert.ok(sent.includes('next') && !sent.includes('skip'), 'music: the side button goes to the next song, not 15 s');
      // the wheel's centre icon sits in the middle of the button
      const centred = await page.evaluate(() => {
        const b = document.getElementById('bCenter').getBoundingClientRect(), i = document.getElementById('cIcon').getBoundingClientRect();
        return [Math.abs((i.left + i.right) / 2 - (b.left + b.right) / 2), Math.abs((i.top + i.bottom) / 2 - (b.top + b.bottom) / 2), i.width];
      });
      assert.ok(centred[2] > 0 && centred[0] <= 1 && centred[1] <= 1, 'the centre icon is centred: ' + centred);
      // up next: a song's cover arrives in a square picture
      await page.click('#bQueue');
      await page.waitForFunction(() => document.querySelector('#queueItems .q-row img.ok'));
      const pic = await page.evaluate(() => { const r = document.querySelector('#queueItems .q-picture.sq').getBoundingClientRect(); return [r.width, r.height]; });
      assert.ok(Math.abs(pic[0] - pic[1]) < 1, 'a song\'s picture is square: ' + pic);
    } else {
      assert.ok(!face.cover && face.eyes, 'video: the eyes stay');
      assert.ok(/^-0:\d\d$/.test(await page.evaluate(() => document.getElementById('rightNote').textContent)), 'video: what is left, not when it ends');
    }
    await context.close();
    assert.deepEqual(errors, []);
    return shown;
  };
  assert.equal(await card(false), true, 'video: the up-next card shows near the end');
  assert.equal(await card(true), false, 'music: the face stays clear');
  await browser.close(); server.close();
  console.log('remote audio: passed');
})().catch(e => { console.error(e); process.exit(1); });
