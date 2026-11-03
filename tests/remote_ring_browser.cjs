// Optional browser regression test: the wheel's outer ring follows the song's time. A song opened
// from the phone's folders or on the computer (so a queue is playing) used to leave the ring on
// the old song's place in the queue while the time under the bar moved on. In up-next mode the
// ring still shows the place in the queue. Requires playwright and Chromium. Run from the source root.
const {chromium} = require('playwright');
const fs = require('fs'), http = require('http'), assert = require('assert'), path = require('path');
process.chdir(path.join(__dirname, '..'));
const html = fs.readFileSync('assets/remote/index.html', 'utf8');
const page_html = html.replace('<script>', '<script nonce="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa">')
  .replace('/*CUE_BOOT*/', 'window.CUE=' + JSON.stringify({lang: 'pt-BR', strings: JSON.parse(fs.readFileSync('i18n/pt-BR.json', 'utf8'))}) + ';');
// a four-song queue whose third song is current: its place in the queue is 2/3 of the ring
const queue = {items: [0, 1, 2, 3].map(i => ({name: 'Song ' + i + '.flac', current: i === 2}))};
const server = http.createServer((req, res) => {
  if (req.url.startsWith('/queue')) { res.setHeader('Content-Type', 'application/json'); res.end(JSON.stringify(queue)); return; }
  if (req.url !== '/') { res.statusCode = 404; res.end(); return; }
  res.setHeader('Content-Type', 'text/html'); res.end(page_html);
});
(async () => {
  await new Promise(r => server.listen(8142, '127.0.0.1', r));
  const browser = await chromium.launch({headless: true, executablePath: process.env.CUE_CHROMIUM_PATH || undefined, args: ['--no-sandbox', '--disable-dev-shm-usage']});
  const ctx = await browser.newContext({viewport: {width: 390, height: 750}, isMobile: true, hasTouch: true});
  const page = await ctx.newPage(); const errors = []; page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(() => {
    localStorage.setItem('cue.token.v2', 'a'.repeat(64));
    window.WebSocket = class {
      constructor() { window.__ws = this; this.readyState = 0; setTimeout(() => { this.readyState = 1; this.onopen?.(); }, 5); }
      send(d) { const m = JSON.parse(d); if (m.t === 'auth') setTimeout(() => this.onmessage?.({data: JSON.stringify({t: 'hello', pc: 'PC'})}), 5); }
      close() {}
    };
    window.__state = (file, pos, dur, rev) => window.__ws.onmessage({data: JSON.stringify({t: 'state', view: 'play', file, pos, dur, speed: 1, vol: 100,
      paused: false, ended: false, audioOnly: true, sub: {has: false, on: false, delay: 0, size: 44, sizeMin: 28, sizeMax: 72, pos: 93, posDefault: 93, posMin: 12, posMax: 97},
      audio: {n: 1, i: 0}, timer: {m: 0, left: 0}, next: '', queueActive: true, queueRevision: rev, color: '#a0508f', cover: 0, title: file, artist: 'Artist'})});
  });
  await page.goto('http://127.0.0.1:8142/'); await page.waitForTimeout(300);
  const tip = () => page.evaluate(() => Array.prototype.findIndex.call(document.getElementById('dots').children, d => d.__c === '#ffffff'));
  const expect = (pos, dur) => Math.min(47, Math.floor(pos / dur * 48));
  // the queue's list is loaded once (up-next mode, then back to time), then a new song opens on
  // the computer: the ring follows its time, not the old list
  await page.evaluate(() => window.__state('Song 2.flac', 30, 240, 1)); await page.waitForTimeout(200);
  // the centre button turns time → volume → up next
  await page.click('#bCenter'); await page.waitForTimeout(150); await page.click('#bCenter'); await page.waitForTimeout(400);
  assert.equal(await tip(), Math.min(47, Math.floor(2 / 3 * 48)), 'up-next mode: the ring is the place in the queue');
  await page.click('#bCenter'); await page.waitForTimeout(300);
  for (const [file, pos, dur] of [['Song 2.flac', 71, 251], ['Other folder - New.flac', 5, 180], ['Other folder - New.flac', 120, 180]]) {
    await page.evaluate(([f, p, d]) => window.__state(f, p, d, 2), [file, pos, dur]); await page.waitForTimeout(150);
    assert.equal(await tip(), expect(pos, dur), `${file} at ${pos}/${dur}: the ring's tip follows the time`);
  }
  assert.equal(errors.length, 0, errors.join('\n'));
  await browser.close(); server.close();
  console.log('remote ring: passed');
})().catch(e => { console.error(e); process.exit(1); });
