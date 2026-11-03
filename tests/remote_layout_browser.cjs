// Optional browser regression test: on phones of every height (Safari's bars included), what
// appears while the wheel turns or after a tap never lands on the title: the film's preview box,
// a song's record and its offset stay above it, and a short note (+15 s) takes the title's row
// for a moment instead of covering it. Requires playwright and Chromium. Run from the source root.
const {chromium} = require('playwright');
const fs = require('fs'), http = require('http'), assert = require('assert'), path = require('path');
process.chdir(path.join(__dirname, '..'));
const html = fs.readFileSync('assets/remote/index.html', 'utf8');
const page_html = html.replace('<script>', '<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/', 'window.CUE=' + JSON.stringify({lang: 'pt-BR', strings: JSON.parse(fs.readFileSync('i18n/pt-BR.json', 'utf8'))}) + ';');
function bmp(w, h) {
  const row = Math.ceil(w * 3 / 4) * 4, b = Buffer.alloc(54 + row * h);
  b.write('BM'); b.writeUInt32LE(b.length, 2); b.writeUInt32LE(54, 10); b.writeUInt32LE(40, 14); b.writeInt32LE(w, 18); b.writeInt32LE(h, 22);
  b.writeUInt16LE(1, 26); b.writeUInt16LE(24, 28);
  for (let i = 54; i < b.length; i++) b[i] = (i * 7) & 255;
  return b;
}
const pic = bmp(64, 36);
const server = http.createServer((req, res) => {
  if (req.url === '/font.ttf') { res.setHeader('Content-Type', 'font/ttf'); res.end(fs.readFileSync('assets/fonts/CueOmni.ttf')); return; }
  if (/^\/(cover|queue-thumb|peek|thumb)/.test(req.url)) { res.setHeader('Content-Type', 'image/bmp'); res.end(pic); return; }
  if (req.url === '/queue') { res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify({items: []})); return; }
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  res.setHeader('Content-Type', 'text/html'); res.end(page_html);
});
(async () => {
  await new Promise(r => server.listen(8141, '127.0.0.1', r));
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined, args: ['--no-sandbox', '--disable-dev-shm-usage']});
  let checked = 0;
  for (const [W, H] of [[320, 480], [375, 553], [375, 640], [390, 664], [390, 750], [430, 840]]) for (const audio of [true, false]) {
    const ctx = await browser.newContext({viewport: {width: W, height: H}, isMobile: true, hasTouch: true});
    const page = await ctx.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(audio => {
      localStorage.setItem('cue.token.v2', 'a'.repeat(64));
      window.WebSocket = class {
        constructor() { this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); }, 5); }
        send(d) { const m = JSON.parse(d); if (m.t === 'auth') setTimeout(() => {
          this.onmessage?.({data: JSON.stringify({t: 'hello', pc: 'PC'})});
          this.onmessage?.({data: JSON.stringify({t: 'state', view: 'play', file: audio ? 'Jane Remover - Beauty Sleep.flac' : "RuPaul's Drag Race - S18E15.mkv", pos: 206, dur: audio ? 223 : 3900,
            speed: 1, vol: 100, paused: false, ended: false, audioOnly: audio, sub: {has: false, on: false, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97},
            audio: {n: 1, i: 0}, timer: {m: 0, left: 0}, next: '', queueActive: true, color: '#a0508f', cover: audio ? 7 : 0,
            title: audio ? 'Jane Remover - Beauty Sleep' : undefined, artist: audio ? 'Jane Remover' : undefined})});
        }, 5); }
        close() {}
      };
    }, audio);
    await page.goto('http://127.0.0.1:8141/'); await page.waitForTimeout(400);
    const title = await page.locator('#file').boundingBox();
    // turn the wheel: the preview box (film) or the record and its offset (song) stay above the title
    const w = await page.locator('#wheel').boundingBox(), cx = w.x + w.width / 2, cy = w.y + w.height / 2, R = w.width * .43;
    const at = d => [cx + R * Math.sin(d * Math.PI / 180), cy - R * Math.cos(d * Math.PI / 180)];
    await page.mouse.move(...at(0)); await page.mouse.down();
    for (let d = 0; d <= 60; d += 5) { await page.mouse.move(...at(d)); await page.waitForTimeout(16); }
    await page.waitForTimeout(200);
    const above = await page.evaluate(audio => {
      const ids = audio ? ['spinNote', 'spinDisc'] : ['scrub'];
      return ids.map(id => { const e = document.getElementById(id), r = e.getBoundingClientRect(); return {id, shown: !e.classList.contains('hide') && r.height > 0, bottom: r.bottom}; });
    }, audio);
    for (const a of above) if (a.shown && a.id !== 'spinDisc') assert(a.bottom <= title.y + 1, `${W}x${H} ${a.id} ends at ${a.bottom}, the title starts at ${title.y}`);
    await page.mouse.up(); await page.waitForTimeout(150);
    // the place it went to is not announced over the title (the time under the bar says it)
    assert.equal(await page.evaluate(() => document.getElementById('toast').textContent), '', `${W}x${H} no time over the stage`);
    if (!audio) {
      await page.click('#bFwd'); await page.waitForTimeout(250);
      const note = await page.evaluate(() => ({toast: document.getElementById('toast').textContent, note: document.getElementById('infoNote').textContent,
        noting: document.getElementById('info').classList.contains('noting'), file: getComputedStyle(document.getElementById('file')).opacity}));
      assert(note.noting && note.note === '+15 s' && note.toast === '' && +note.file < .5, `${W}x${H} the note takes the title's row: ` + JSON.stringify(note));
      await page.waitForTimeout(1700);
      assert(!(await page.evaluate(() => document.getElementById('info').classList.contains('noting'))), `${W}x${H} the title comes back`);
    }
    assert.equal(errors.length, 0, errors.join('\n'));
    checked++;
    await ctx.close();
  }
  await browser.close(); server.close();
  console.log(`remote layout: passed (${checked} phones and files)`);
})().catch(e => { console.error(e); process.exit(1); });
