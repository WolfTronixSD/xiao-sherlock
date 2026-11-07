#pragma once
// The whole web GUI. Settings forms are generated from /api/schema, so new options appear automatically.
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>221B Baker Street</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=IM+Fell+English+SC&family=Libre+Baskerville:ital@0;1&family=Special+Elite&display=swap" rel="stylesheet">
<style>
:root{--paper:#f1e6cc;--paper2:#e6d6b0;--ink:#2b1d0e;--faded:#6b5536;--red:#7b1e1e;--brass:#b08d57;--green:#24402f;--card:#f8f0dc}
*{box-sizing:border-box}
body{margin:0;color:var(--ink);font-family:'Libre Baskerville',Georgia,serif;font-size:15px;
 background:radial-gradient(ellipse at top,#fbf4e2,transparent 70%),repeating-linear-gradient(0deg,rgba(120,90,40,.035) 0 2px,transparent 2px 5px),var(--paper)}
.wrap{max-width:1100px;margin:0 auto;padding:12px 16px 60px}
header{display:flex;flex-wrap:wrap;align-items:center;gap:14px;border-bottom:3px double var(--faded);padding-bottom:10px}
.plaque{background:linear-gradient(145deg,#d8b878,#9c7840);color:#2a1a05;font-family:'IM Fell English SC',serif;font-size:34px;
 padding:4px 16px;border-radius:6px;box-shadow:inset 0 0 0 2px #7a5a28,0 2px 4px rgba(0,0,0,.35);text-shadow:0 1px 0 #f3dfa8}
h1{font-family:'IM Fell English SC',serif;font-weight:normal;margin:0;font-size:24px;line-height:1.1}
.quip{font-style:italic;color:var(--faded);font-size:13px;min-height:1.2em}
.seal{margin-left:auto;border:0;cursor:pointer;border-radius:50px;padding:12px 20px;font-family:'IM Fell English SC',serif;font-size:17px;color:#fff;
 background:radial-gradient(circle at 35% 30%,#b23a3a,#6d1414);box-shadow:0 3px 6px rgba(0,0,0,.4),inset 0 -3px 6px rgba(0,0,0,.4)}
.seal.off{background:radial-gradient(circle at 35% 30%,#8a8a7a,#4e4e44)}
nav{display:flex;flex-wrap:wrap;gap:4px;margin:12px 0}
nav button{font-family:'IM Fell English SC',serif;font-size:16px;background:var(--paper2);border:1px solid var(--faded);border-bottom:0;
 border-radius:8px 8px 0 0;padding:7px 14px;cursor:pointer;color:var(--ink)}
nav button.on{background:var(--card);font-weight:bold;position:relative;top:1px}
.tab{display:none}.tab.on{display:block}
.card{background:var(--card);border:1px solid #cdb98c;border-radius:6px;padding:14px;margin-bottom:14px;box-shadow:2px 3px 0 rgba(90,60,20,.12)}
.card h2{font-family:'IM Fell English SC',serif;font-weight:normal;margin:0 0 10px;font-size:20px;border-bottom:1px dashed #b9a171}
.grid{display:grid;grid-template-columns:minmax(0,3fr) minmax(260px,2fr);gap:14px}
@media(max-width:820px){.grid{grid-template-columns:1fr}.seal{margin-left:0}}
.scene{position:relative;background:#111;border:6px solid #5a3d1e;border-radius:4px;box-shadow:inset 0 0 20px #000}
.scene img{display:block;width:100%;filter:sepia(.15)}
.scene canvas{position:absolute;inset:0;width:100%;height:100%}
.scene canvas.edit{touch-action:none;cursor:crosshair;outline:3px dashed #f0c040;outline-offset:-3px}
.btn{font-family:'Libre Baskerville',serif;font-size:13px;background:var(--green);color:#f1e6cc;border:0;border-radius:4px;padding:7px 12px;cursor:pointer;margin:2px}
.btn.red{background:var(--red)}.btn.plain{background:var(--paper2);color:var(--ink);border:1px solid #b9a171}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0}
.small{font-size:12px;color:var(--faded)}
.meter{height:16px;border:1px solid var(--faded);border-radius:8px;background:#e9dcbc;position:relative;overflow:hidden}
.meter i{position:absolute;left:0;top:0;bottom:0;background:linear-gradient(90deg,#3e6b4a,#a8892f,#8e2a2a)}
.meter b{position:absolute;top:-2px;bottom:-2px;width:3px;background:var(--red)}
#micMeter{height:22px;cursor:ew-resize;touch-action:none}#micMeter b{width:5px;margin-left:-2px;box-shadow:0 0 0 1px #fff8}
.btn:disabled{opacity:.45;cursor:not-allowed}
.lamp{display:inline-block;width:12px;height:12px;border-radius:50%;background:#998;border:1px solid #554;vertical-align:middle;margin-right:6px}
.health td{padding:3px 6px;vertical-align:top}.health td:first-child{white-space:nowrap}
.lamp.hi{background:#e8b030;box-shadow:0 0 6px #f0c040}.lamp.lo{background:#3e6b4a}.lamp.rec{background:#c62828;box-shadow:0 0 6px #f44;animation:blink 1s infinite}
@keyframes blink{50%{opacity:.3}}
.inst div{margin:5px 0}
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;font-family:'IM Fell English SC',serif;font-weight:normal;font-size:15px;border-bottom:2px solid var(--faded)}
td{border-bottom:1px dotted #b9a171;padding:5px 4px;vertical-align:top}
.log td{font-family:'Special Elite',monospace}
.kind{display:inline-block;padding:1px 6px;border-radius:3px;font-size:11px;color:#fff;background:var(--faded)}
.k-sound{background:#5c3b7a}.k-motion{background:#8a5a14}.k-radar{background:#1d5c63}.k-sonar{background:#3b4f8a}.k-test{background:#555}
.k-saved{background:var(--green)}.k-error,.k-warning{background:var(--red)}.k-input1,.k-input2,.k-input3,.k-input4{background:#7b1e1e}
.quote{font-style:italic;color:var(--faded);font-size:12px}
fieldset{border:1px solid #cdb98c;border-left:10px solid var(--sc,#6b5536);border-radius:8px;margin:0 0 14px;padding:0;background:var(--card);scroll-margin-top:90px}
details.sec>summary{list-style:none;cursor:pointer;display:flex;align-items:center;gap:12px;padding:10px 12px;background:linear-gradient(90deg,color-mix(in srgb,var(--sc) 22%,transparent),transparent);border-radius:0 8px 8px 0}
details.sec>summary::-webkit-details-marker{display:none}
details.sec>summary .ico{font-size:36px;line-height:1;filter:drop-shadow(1px 2px 0 rgba(0,0,0,.15))}
details.sec>summary .ttl{font-family:'IM Fell English SC',serif;font-size:20px;color:var(--sc);flex:1}
details.sec>summary .arw{font-size:20px;color:var(--sc);transition:transform .2s}
details.sec[open]>summary .arw{transform:rotate(90deg)}
details.sec>.body{padding:4px 14px 12px}
.onoff{font-size:11px;font-family:'Libre Baskerville',serif;padding:2px 8px;border-radius:10px;background:#d8ccb0;color:#6b5536;white-space:nowrap}
.onoff.on{background:#2e7d32;color:#fff}
.jump{display:grid;grid-template-columns:repeat(auto-fill,minmax(104px,1fr));gap:8px;padding:4px 0;margin-bottom:10px}
.totop{position:fixed;right:14px;bottom:14px;z-index:9;border-radius:50%;width:46px;height:46px;font-size:20px;box-shadow:2px 3px 0 rgba(0,0,0,.25)}
.jump button{border:0;border-radius:10px;padding:8px 4px 6px;cursor:pointer;color:#fff;background:var(--sc);box-shadow:2px 3px 0 rgba(0,0,0,.18);font-family:'Libre Baskerville',serif;font-size:11.5px;line-height:1.2;position:relative}
.jump button .ico{display:block;font-size:30px;margin-bottom:3px}
.jump button .dot{position:absolute;top:5px;right:6px;width:9px;height:9px;border-radius:50%;background:#bbb;border:1px solid #fff}
.jump button .dot.on{background:#5be35b}
@media(max-width:600px){.jump{grid-template-columns:repeat(4,1fr);gap:5px}.jump button{font-size:10px;padding:6px 2px}.jump button .ico{font-size:24px}}
legend{font-family:'IM Fell English SC',serif;font-size:18px;padding:0 6px}
.f{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:5px 0;border-bottom:1px dotted #d6c49a}
.f label{flex:1}.f input[type=text],.f input[type=password],.f input[type=number],.f select{width:220px;max-width:55vw;font-family:inherit;font-size:13px;
 background:#fffaf0;border:1px solid #b9a171;border-radius:3px;padding:4px}
.f input[type=checkbox]{width:20px;height:20px;accent-color:var(--red)}
.sw,.f label.sw{position:relative;display:inline-block;width:46px;height:24px;flex:none}.sw input{opacity:0;width:0;height:0;position:absolute}
.sw span{position:absolute;inset:0;background:#c9b48a;border-radius:24px;cursor:pointer;transition:.15s}
.sw span:before{content:'';position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.15s;box-shadow:0 1px 2px #0004}
.sw input:checked+span{background:var(--green)}.sw input:checked+span:before{transform:translateX(22px)}
.seg{display:inline-flex;flex-wrap:wrap;border:1px solid #b9a171;border-radius:6px;overflow:hidden;max-width:60vw}
.seg button{font-family:inherit;font-size:12px;border:0;border-left:1px solid #b9a171;background:var(--paper2);color:var(--ink);padding:6px 10px;cursor:pointer}
.seg button:first-of-type{border-left:0}.seg button.on{background:var(--red);color:#fff}
.acts{display:flex;flex-wrap:wrap;gap:6px;padding:6px 0;border-bottom:1px dotted #d6c49a;align-items:center}
.chip{display:inline-flex;align-items:center;gap:4px;border:1px solid #b9a171;border-radius:16px;padding:4px 10px;font-size:12px;cursor:pointer;background:var(--paper2);user-select:none}
.chip input{display:none}.chip:has(input:checked){background:var(--green);color:#f1e6cc;border-color:var(--green)}
.chip.on{background:var(--green);color:#f1e6cc;border-color:var(--green)}
.help{font-size:11px;color:var(--faded);font-style:italic;padding:2px 0 6px}
.files{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:10px}
.clue{background:#fffaf0;border:1px solid #cdb98c;padding:6px;font-size:11px;word-break:break-all;box-shadow:1px 2px 0 rgba(0,0,0,.08);transform:rotate(-.4deg)}
.clue:nth-child(2n){transform:rotate(.5deg)}
.clue img{width:100%;height:110px;object-fit:cover;background:#ddd;cursor:pointer;filter:sepia(.25)}
.clue .ic{height:110px;display:flex;align-items:center;justify-content:center;font-size:44px;background:#efe3c6}
.clue audio{width:100%;height:30px}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--ink);color:var(--paper);padding:9px 16px;border-radius:6px;
 font-family:'Special Elite',monospace;opacity:0;transition:opacity .4s;pointer-events:none;z-index:9}
.toast.on{opacity:.95}
.zones td input{width:100%;font-family:inherit;font-size:12px}
#radar{width:100%;max-width:340px;display:block;margin:auto}
.modal{position:fixed;inset:0;background:rgba(20,12,4,.88);display:none;align-items:center;justify-content:center;z-index:8;padding:10px}
.modal.on{display:flex}
.kino{background:var(--card);border:8px solid #5a3d1e;border-radius:6px;max-width:900px;width:100%;padding:10px}
.kino h2{font-family:'IM Fell English SC',serif;font-weight:normal;margin:0 0 6px;font-size:20px}
.kino img{width:100%;background:#000;display:block;filter:sepia(.2);min-height:120px}
.kino input[type=range]{flex:1;accent-color:var(--red)}
:root{--c-jpg:#a8792a;--c-avi:#8e2a2a;--c-wav:#5c3b7a}
.chart{width:100%;display:block;background:#fffaf0;border:1px solid #cdb98c;border-radius:4px;margin:4px 0 10px;touch-action:manipulation}
.grid2{display:grid;grid-template-columns:minmax(0,3fr) minmax(200px,2fr);gap:14px}@media(max-width:700px){.grid2{grid-template-columns:1fr}}
.key{display:inline-block;width:12px;height:12px;border-radius:2px;margin:0 2px 0 8px;vertical-align:middle}
.stat{background:#fffaf0;border:1px solid #cdb98c;border-radius:4px;padding:6px 10px;text-align:center;min-width:90px}.stat b{display:block;font-size:20px;font-family:'IM Fell English SC',serif}
</style></head><body><div class="wrap">
<header>
 <div class="plaque">221B</div>
 <div><h1>The Baker Street Irregular<br>Surveillance Society</h1><div class="quip" id="quip"></div><div class="small" id="hdrInfo"></div></div>
 <span class="small" id="homeChip" style="margin-left:auto"></span>
 <button id="seal" class="seal" onclick="toggleArm()">...</button>
</header>
<nav id="nav"></nav>

<section class="tab" id="t-scene">
 <div class="grid">
  <div>
   <div class="card"><h2>&#128269; The Scene</h2>
    <div class="scene"><img id="cam" alt="the scene"><canvas id="ov"></canvas></div>
    <div class="row">
     <label><input type="checkbox" id="showGrid" checked> show what changed</label>
     <label><input type="checkbox" id="live" checked onchange="snap()"> live view</label> <span class="small" id="fps"></span>
     <button class="btn" onclick="drawMode()">&#9998; Mark a place of interest</button>
     <label><input type="checkbox" id="editZ" onchange="editMode(this.checked)"> move / resize places</label>
     <button class="btn red" onclick="test()">Summon Lestrade (test)</button>
    </div>
    <div class="small" id="drawHint">Places of interest are motion zones: a door, a window, the biscuit tin. Tap "Mark", then drag a box over the picture with mouse or finger. Tick "move / resize" to drag a box, or its corner handle.</div>
   </div>
   <div class="card"><h2>Places of Interest (motion zones)</h2>
    <table class="zones"><thead><tr><th>On</th><th>Name</th><th>Sensitivity: trigger at % changed</th><th>Now</th><th></th></tr></thead><tbody id="zones"></tbody></table>
    <div class="row"><button class="btn" onclick="saveZones()">Save places</button>
     <span class="small" id="motionNote"></span></div>
   </div>
  </div>
  <div>
   <div class="card inst"><h2>The Instruments</h2>
    <div><b>The Listening Post</b> <span id="micTxt"></span></div>
    <div class="meter" id="micMeter"><i id="micBar"></i><b id="micThr"></b></div>
    <div class="small">Red line = threshold: drag it to change it (saved at once). Clap, cough, or accuse someone loudly to test.</div>
    <div style="margin-top:8px"><span class="lamp" id="recA"></span><span id="recATxt">Watson's pencil is idle</span></div>
    <div><span class="lamp" id="recV"></span><span id="recVTxt">The camera obscura rests</span></div>
    <div style="margin-top:8px"><b>Sensors</b> <button class="btn plain" id="allSens" onclick="allSensors()">All off</button>
     <div id="sensChips" style="display:flex;flex-wrap:wrap;gap:6px;margin-top:4px"></div></div>
    <div style="margin-top:8px"><b>Tripwires</b><div id="inputs"></div></div>
    <div id="sonarBox"></div>
    <div style="margin-top:8px"><span class="lamp" id="nightL"></span><b>The Night Watch</b> <span id="nightTxt"></span></div>
    <div><span class="lamp" id="bellL"></span><b>The Alarm Bell</b> <span id="bellTxt"></span>
     <button class="btn plain" id="bellBtn" onclick="bell()">Ring it</button>
     <div class="row" style="align-items:center;gap:8px;margin-top:4px"><label class="sw" title="Alarm bell on or off"><input type="checkbox" id="bellEn" onchange="bellCfg('en',this.checked)"><span></span></label>
      <span class="seg"><button type="button" id="bellHi" onclick="bellCfg('low',false)">Active HIGH</button><button type="button" id="bellLo" onclick="bellCfg('low',true)">Active LOW</button></span></div></div>
    <div id="warn" style="color:var(--red)"></div>
   </div>
   <div class="card"><h2>&#129658; The Doctor's Report</h2><table class="health" id="health"></table>
    <div class="small">Green: in good health. Amber: keep an eye on it. Red: send for Dr Watson.</div>
    <div class="row" id="campRow" style="display:none"><button class="btn plain" onclick="sleepNow()">&#128164; Send Holmes to sleep now</button></div></div>
   <div class="card"><h2>The Radar</h2><canvas id="radar" width="340" height="190"></canvas>
    <div class="small" id="radarTxt"></div>
    <div class="row" style="align-items:center;gap:8px;margin-top:6px"><label for="radarMm"><b>Trigger closer than</b></label>
     <input type="range" id="radarMm" min="100" max="6000" step="100" style="flex:1;accent-color:var(--red)"
      oninput="radarDrag=+this.value;$('radarMmTxt').textContent=(radarDrag/1000).toFixed(1)+' m';drawRadar()" onchange="radarSave()">
     <span id="radarMmTxt" style="min-width:48px"></span></div></div>
  </div>
 </div>
</section>

<section class="tab" id="t-log">
 <div class="card"><h2>The Case Log</h2>
  <div class="row"><a class="btn plain" href="/file?name=caselog.csv" target="_blank">Open the full case book (CSV)</a></div>
  <table class="log"><thead><tr><th>When</th><th>What</th><th>Who</th><th>Particulars</th></tr></thead><tbody id="log"></tbody></table>
 </div>
</section>

<section class="tab" id="t-files">
 <div class="card"><h2>&#128202; The Statistics</h2>
  <div class="row" id="sumRow"></div>
  <div class="small">Clues per day (last 30 days)</div><canvas id="chDays" class="chart" height="170"></canvas>
  <div class="grid2">
   <div><div class="small">By hour of day <select id="hDay" onchange="drawStats()"></select></div><canvas id="chHours" class="chart" height="140"></canvas></div>
   <div><div class="small">By witness (sensor)</div><table id="srcTab"></table></div>
  </div>
  <div class="row small"><span class="key" style="background:var(--c-jpg)"></span>Photographs <span class="key" style="background:var(--c-avi)"></span>Moving pictures <span class="key" style="background:var(--c-wav)"></span>Recordings</div>
 </div>
 <div class="card"><h2>&#128336; The Timeline</h2>
  <div class="row"><select id="tDay" onchange="loadTimeline()"></select><span class="small" id="tInfo">Tap a mark to open that clue.</span></div>
  <canvas id="chTime" class="chart" height="120"></canvas>
 </div>
 <div class="card"><h2>The Evidence Locker</h2>
  <div class="row">
   <select id="ftype" onchange="renderFiles()"><option value="">All evidence</option><option value=".jpg">Photographs</option><option value=".avi">Moving pictures</option><option value=".wav">Recordings</option><option value=".csv">Case books</option></select>
   <button class="btn plain" onclick="loadFiles()">Refresh</button>
   <span class="small" id="fcount"></span>
  </div>
  <div class="row">
   <select id="fday"></select><button class="btn red" onclick="burnDay()">&#128293; Burn this day</button>
   <button class="btn red" onclick="burnAll()">&#128293;&#128293; Burn everything</button>
   <label class="small"><input type="checkbox" id="keepLog" checked> keep the case log</label>
  </div>
  <div class="small">Click a photograph to enlarge. Videos are .avi (play in VLC or Windows Media Player). If Chrome says "insecure download", choose Keep - it is merely suspicious of plain HTTP, like Holmes of everyone.</div>
  <div class="files" id="files" style="margin-top:10px"></div>
 </div>
</section>

<section class="tab" id="t-settings">
 <div class="card"><h2>Instructions to Watson</h2>
  <div class="small">Every setting is kept on the device and survives a restart. Pins D8-D10 belong to the SD card and are not offered.</div></div>
 <div class="jump" id="jump"></div>
 <div class="row" style="margin:-4px 0 8px"><button class="btn plain" onclick="openAll(true)">Open all</button><button class="btn plain" onclick="openAll(false)">Close all</button>
  <span class="small">Tap a tile to jump to its section. Green dot = switched on.</span></div>
 <div id="form"></div>
 <button class="btn totop" onclick="$('jump').scrollIntoView({behavior:'smooth'})" title="Back to the tiles">&#11014;&#65039;</button>
 <div class="card row"><button class="btn" id="sealBtn" disabled onclick="saveSettings()">Seal and send the instructions</button><span class="small" id="saveMsg"></span></div>
 <div class="card"><h2>The Telegram Boy</h2>
  <div class="small">1. In Telegram open <b>@BotFather</b>, send <b>/newbot</b>, pick a name, paste the token into "The Telegram Boy" section above, tick <i>Send Telegram alerts</i>, and press Seal and send.<br>
  2. Open your new bot in Telegram and send it any message (e.g. <i>hello</i>).<br>
  3. Press <b>Find my chat ID</b>, then <b>Send a test</b>. Choose which sensors alert you with <i>Send an alert</i> in each section.</div>
  <div class="row"><button class="btn" onclick="tg('findchat')">Find my chat ID</button><button class="btn red" onclick="tg('test')">Send a test</button><span class="small" id="tgMsg"></span></div></div>
 <div class="card"><h2>&#128038; The Pigeon Post (Meshtastic)</h2>
  <div class="small">For the camp: no Wi-Fi, no phone signal, still alerts. Set up the Pigeon Post section above, press Seal and send, then send a test. It arrives in the Meshtastic app on any radio on the same channel.</div>
  <div class="row"><button class="btn red" onclick="meshTest()">Send a test pigeon</button><span class="small" id="meshMsg"></span></div></div>
</section>

<section class="tab" id="t-shop">
 <div class="card"><h2>The Workshop</h2><table id="sys"></table></div>
 <div class="card"><h2>New firmware by telegraph (OTA)</h2>
  <div class="small">Arduino IDE: Sketch &rarr; Export Compiled Binary, then pick SecurityCam.ino.bin here.</div>
  <div class="row"><input type="file" id="fw" accept=".bin"><button class="btn red" onclick="ota()">Upload</button><span id="otaMsg" class="small"></span></div>
 </div>
 <div class="card"><h2>&#128220; The Chronicles (version history)</h2><div id="chron"></div></div>
 <div class="card"><h2>Drastic measures</h2><button class="btn red" onclick="restart()">Restart the device</button></div>
</section>
</div><div class="toast" id="toast"></div>
<div class="modal" id="kino" onclick="if(event.target==this)closeKino()"><div class="kino">
 <h2>The Kinetoscope &mdash; <span id="kName"></span></h2>
 <img id="kImg" alt="film">
 <div class="row"><button class="btn" id="kPlay" onclick="kToggle()">Pause</button><input type="range" id="kPos" min="0" value="0" oninput="kSeek(+this.value)">
  <select id="kSpeed"><option value="0.5">0.5x</option><option value="1" selected>1x</option><option value="2">2x</option><option value="4">4x</option></select>
  <span class="small" id="kInfo"></span><a class="btn plain" id="kSave" download="frame.jpg">Save frame</a><button class="btn red" onclick="closeKino()">Close</button></div>
</div></div>

<script>
// ---------- flavour (add your own lines here) ----------
const QUIPS=["When you have eliminated the impossible, whatever remains is probably the cat.","The game is afoot. Also the dog.",
"Data! Data! Data! I cannot make bricks without clay.","It is a capital mistake to theorise before one has recorded audio.",
"You see, but you do not observe. The camera does both.","Mrs. Hudson has been informed. She is unimpressed.",
"Moriarty would never trip a reed switch. Probably.","Elementary, my dear Watson. Mostly firmware."];
const REMARKS={sound:["A noise! Possibly Moriarty, possibly the kettle.","Someone sneezed in the drawing room.","The walls have ears. Ours, specifically."],
motion:["Something moved. The plot thickens.","A shadow at the window!","Movement, Watson! Fetch my revolver. Or a biscuit."],
input1:["The tripwire has spoken.","A door opens; a mystery begins."],input2:["The tripwire has spoken."],input3:["The tripwire has spoken."],input4:["The tripwire has spoken."],
radar:["A figure approaches through the fog.","The radar sees what the eye cannot."],sonar:["The bat ears detect a visitor."],
test:["Inspector Lestrade blunders in, as expected."],saved:["Evidence secured."],start:["221B opens its doors."],config:["Watson takes notes."],
error:["Confound it!"],warning:["Hmm. Most irregular."],deleted:["Evidence destroyed. Very suspicious."]};
const pick=a=>a[Math.floor(Math.random()*a.length)];
const $=id=>document.getElementById(id);
function toast(t){$('toast').textContent=t;$('toast').classList.add('on');clearTimeout(toast.h);toast.h=setTimeout(()=>$('toast').classList.remove('on'),2500)}
async function api(u,opt){const r=await fetch(u,opt);if(!r.ok)throw new Error(await r.text());return r}
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

// ---------- tabs ----------
const TABS=[['scene','The Scene'],['log','Case Log'],['files','Evidence Locker'],['settings','Instructions to Watson'],['shop','The Workshop']];
$('nav').innerHTML=TABS.map(t=>`<button id="b-${t[0]}" onclick="show('${t[0]}')">${t[1]}</button>`).join('');
let tab='scene';
function show(t){tab=t;TABS.forEach(x=>{$('t-'+x[0]).classList.toggle('on',x[0]==t);$('b-'+x[0]).classList.toggle('on',x[0]==t)});
 if(t=='scene')setTimeout(snap,0);if(t=='log')loadLog();if(t=='files'){loadFiles();loadStats()}if(t=='settings')loadSettings();try{localStorage.tab=t}catch(e){}}
try{if(localStorage.tab)tab=localStorage.tab}catch(e){}
$('quip').textContent=pick(QUIPS);setInterval(()=>$('quip').textContent=pick(QUIPS),20000);

// ---------- status / instruments ----------
let st={},cfg={};
let stBusy=false,stLast=0;
function hdrInfo(){const m=/^(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)/.exec(st.time||'');
 const t=m?new Date(+m[1],m[2]-1,+m[3],+m[4],+m[5],+m[6]).toLocaleString(undefined,{weekday:'short',day:'numeric',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'}):'clock not set';
 $('hdrInfo').textContent='Firmware v'+(st.fw||'?')+'  \u00b7  '+t}
async function status(){if(stBusy)return;stBusy=true;stLast=Date.now();try{st=await (await api('/api/status')).json()}catch(e){return}finally{stBusy=false}
 hdrInfo();
 $('homeChip').textContent=st.phoneHome==1?'\u{1F4F1} Holmes is home':st.phoneHome==0?'\u{1F4F1} Holmes is out':'';
 const seal=$('seal');seal.textContent=st.armed?'The game is afoot!':'Holmes is off duty';seal.classList.toggle('off',!st.armed);
 const pct=v=>Math.max(0,Math.min(100,(v+90)/90*100));
 $('micBar').style.width=pct(st.mic)+'%';if(thrDrag==null)$('micThr').style.left=pct(st.soundDb)+'%';$('micTxt').textContent=st.mic+' dB';
 $('recA').className='lamp'+(st.audioRec?' rec':'');$('recATxt').textContent=st.audioRec?'Watson is scribbling (recording audio)':"Watson's pencil is idle";
 $('recV').className='lamp'+(st.videoRec?' rec':'');$('recVTxt').textContent=st.videoRec?'The camera obscura whirrs (recording video)':'The camera obscura rests';
 $('inputs').innerHTML=st.inputs.filter(i=>i.en).map(i=>`<div><span class="lamp ${i.level==1?'hi':i.level==0?'lo':''}"></span>${esc(i.name)} <span class="small">${esc(i.pin)}: ${i.level==1?'HIGH':i.level==0?'LOW':'?'}</span></div>`).join('')||'<div class="small">None strung. See Instructions to Watson.</div>';
 $('sonarBox').innerHTML=st.sonarEn?`<div style="margin-top:8px"><b>The Bat Ears</b> ${st.sonarCm<0?'nothing in range':st.sonarCm+' cm'} <span class="small">(trigger under ${st.sonarLimit} cm)</span></div>`:'';
 $('warn').textContent=st.warn||'';
 const NM=['off','when dark','by the clock','always'];
 $('nightL').className='lamp'+(st.night?' hi':'');$('nightTxt').innerHTML=(st.night?'on duty':'resting')+` <span class="small">(${NM[st.nightMode]||'?'}, light level ${st.light<0?'?':st.light}${st.irPin?', IR lamp on '+esc(st.irPin):''})</span>`;
 $('bellL').className='lamp'+(st.siren?' rec':'');$('bellTxt').innerHTML=(st.siren?'RINGING':'silent')+` <span class="small">${st.sirenPin?esc(st.sirenPin):'(no pin set)'}</span>`;
 $('bellBtn').textContent=st.siren?'Silence it':'Ring it';
 if(!bellBusy){$('bellEn').checked=!!st.sirenEn;$('bellHi').classList.toggle('on',!st.sirenLow);$('bellLo').classList.toggle('on',!!st.sirenLow);$('bellBtn').disabled=!st.sirenEn}
 drawRadar();health();syncTime();
 if(st.motionEn===false)$('motionNote').textContent='Motion detection is off - switch it on under Instructions to Watson.';else $('motionNote').textContent='Whole picture changed: '+st.motionGlobal+'%';
 zones.forEach((z,i)=>{const c=$('zp'+i),m=$('zm'+i),v=st.motion[i]||0;if(c)c.textContent=v+'%';
  if(m){m.style.width=Math.min(100,v)+'%';m.nextElementSibling.style.left=z.minPct+'%'}});
 if(tab=='shop')sys();
}
let bellBusy=false;
async function bellCfg(k,v){bellBusy=true;try{await api('/api/sirencfg?'+k+'='+(v?1:0),{method:'POST'});
 if(k=='en'){st.sirenEn=cfg.sirenEn=v;const e=$('f_sirenEn');if(e)e.checked=v}else{st.sirenLow=cfg.sirenActiveLow=v;const e=$('f_sirenActiveLow');if(e){e.checked=v;e.parentNode.querySelectorAll('button').forEach((b,i)=>b.classList.toggle('on',(i==1)==v))}}
 toast(k=='en'?(v?'Alarm bell on.':'Alarm bell off.'):(v?'Bell rings when the pin goes LOW.':'Bell rings when the pin goes HIGH.'))}catch(e){toast('Failed: '+e.message)}
 bellBusy=false;$('bellEn').checked=!!st.sirenEn;$('bellHi').classList.toggle('on',!st.sirenLow);$('bellLo').classList.toggle('on',!!st.sirenLow);$('bellBtn').disabled=!st.sirenEn}
// ---------- quick sensor switches (saved at once) ----------
const SENS=[['soundEn','&#128066; Sound'],['motionEn','&#128065;&#65039; Motion'],['in1_en',1],['in2_en',2],['in3_en',3],['in4_en',4],['radarEn','&#128225; Radar'],['sonarEn','&#129415; Sonar']];
function sensLabel(s){return typeof s[1]=='number'?'&#129700; '+esc(cfg['in'+s[1]+'_name']||('Input '+s[1])):s[1]}
function drawSens(){if(!cfg||cfg.soundEn===undefined)return;const any=SENS.some(s=>cfg[s[0]]);
 $('sensChips').innerHTML=SENS.map(s=>`<span class="chip ${cfg[s[0]]?'on':''}" onclick="sensSet({${s[0]}:${!cfg[s[0]]}})">${sensLabel(s)}</span>`).join('');
 $('allSens').textContent=any?'All off':'All back on'}
async function sensSet(o){try{await api('/api/config',{method:'POST',body:JSON.stringify(o)});Object.assign(cfg,o);
 Object.keys(o).forEach(k=>{const e=$('f_'+k);if(e)e.checked=o[k]});if(setLoaded)refreshOnOff();drawSens()}catch(e){toast('Failed: '+e.message)}}
function allSensors(){const on=SENS.filter(s=>cfg[s[0]]).map(s=>s[0]);let o={};
 if(on.length){try{localStorage.setItem('sensWereOn',JSON.stringify(on))}catch(e){}SENS.forEach(s=>o[s[0]]=false);toast('All sensors off. The camera still watches the live view.')}
 else{let was=['motionEn'];try{was=JSON.parse(localStorage.getItem('sensWereOn'))||was}catch(e){}was.forEach(k=>o[k]=true);toast('Sensors back on.')}
 sensSet(o)}
let radarDrag=null;
async function radarSave(){const v=radarDrag;try{const r=await (await api('/api/radarmm?mm='+v,{method:'POST'})).text();st.radarMaxMm=cfg.radarMaxMm=+r;
 const f=$('f_radarMaxMm');if(f)f.value=+r;toast('The radar now watches out to '+(r/1000).toFixed(1)+' m.')}catch(e){toast('Failed: '+e.message)}radarDrag=null}
function drawRadar(){if(radarDrag==null&&st.radarMaxMm){$('radarMm').value=st.radarMaxMm;$('radarMmTxt').textContent=(st.radarMaxMm/1000).toFixed(1)+' m'}
 const c=$('radar'),g=c.getContext('2d'),W=c.width,H=c.height,R=H-12,cx=W/2,cy=H-6,max=6000;
 g.clearRect(0,0,W,H);g.fillStyle='#16291e';g.beginPath();g.moveTo(cx,cy);g.arc(cx,cy,R,Math.PI*1.17,Math.PI*1.83);g.closePath();g.fill();
 g.strokeStyle='rgba(176,141,87,.6)';g.lineWidth=1;for(let d=1;d<=6;d++){g.beginPath();g.arc(cx,cy,R*d/6,Math.PI*1.17,Math.PI*1.83);g.stroke()}
 if(!st.radarEn){g.fillStyle='#d8c38f';g.font='14px serif';g.textAlign='center';g.fillText('Radar not enabled',cx,cy-R/2);$('radarTxt').textContent='Connect an HLK-LD2450 and enable it in the settings.';return}
 g.strokeStyle='#c62828';g.setLineDash([4,3]);g.beginPath();g.arc(cx,cy,R*Math.min(radarDrag??st.radarMaxMm,max)/max,Math.PI*1.17,Math.PI*1.83);g.stroke();g.setLineDash([]);
 (st.radar||[]).forEach((t,i)=>{const x=cx+t.x/max*R,y=cy-t.y/max*R;g.fillStyle='#f0c040';g.beginPath();g.arc(x,y,6,0,7);g.fill();g.fillStyle='#fff';g.font='11px serif';g.fillText('#'+(i+1),x+8,y)});
 $('radarTxt').textContent=st.radarSeen?(st.radar.length?st.radar.map((t,i)=>`#${i+1}: ${(t.x/1000).toFixed(1)} m across, ${(t.y/1000).toFixed(1)} m out, ${t.s} cm/s`).join(' | '):'The fog is empty.'):'No word from the radar - check wiring (TX/RX crossed?) and that it runs at 256000 baud.'}

// ---------- live picture, motion overlay, zone drawing ----------
let zones=[],grid=null,drawing=false,dragStart=null,dragNow=null;
const ov=$('ov'),img=$('cam');
// low-latency live view: ask for the next picture as soon as the last one arrived
let snapBusy=false,snapN=0;img.onload=paint;
async function snap(){if(snapBusy||!$('live').checked||tab!='scene'||document.hidden)return;snapBusy=true;let ok=0;
 try{const r=await fetch('/capture?t='+Date.now());if(r.ok){const u=URL.createObjectURL(await r.blob());if(img.dataset.u)URL.revokeObjectURL(img.dataset.u);img.dataset.u=u;img.src=u;snapN++;ok=1}}catch(e){}
 snapBusy=false;setTimeout(snap,ok?40:1000)}
setInterval(()=>{$('fps').textContent=$('live').checked&&tab=='scene'?snapN+' fps':'';snapN=0},1000);
document.addEventListener('visibilitychange',snap);
async function loadGrid(){if(!$('showGrid').checked||tab!='scene'||!st.motionEn){grid=null;return}try{grid=await (await api('/api/motiongrid')).json()}catch(e){}}
function paint(){const r=img.getBoundingClientRect();ov.width=r.width;ov.height=r.height;const g=ov.getContext('2d'),W=ov.width,H=ov.height;
 if(grid&&grid.w){const cw=W/grid.w,ch=H/grid.h;for(let i=0;i<grid.g.length;i++){const v=+grid.g[i];if(v>1){g.fillStyle=`rgba(220,40,40,${v/12})`;g.fillRect((i%grid.w)*cw,Math.floor(i/grid.w)*ch,cw,ch)}}}
 zones.forEach((z,i)=>{if(!z.w)return;const hot=st.motion&&st.motion[i]>=z.minPct&&z.en;g.strokeStyle=z.en?(hot?'#ff3030':'#f0c040'):'#999';g.lineWidth=2;g.setLineDash(z.en?[]:[5,4]);
  g.strokeRect(z.x*W,z.y*H,z.w*W,z.h*H);g.setLineDash([]);g.fillStyle='rgba(0,0,0,.6)';g.font='13px Georgia';const t=z.name+(st.motion?' '+(st.motion[i]||0)+'%':'');
  g.fillRect(z.x*W,z.y*H,g.measureText(t).width+8,17);g.fillStyle=g.strokeStyle;g.fillText(t,z.x*W+4,z.y*H+13);
  if($('editZ').checked){g.fillStyle='#f0c040';g.fillRect((z.x+z.w)*W-HS,(z.y+z.h)*H-HS,HS,HS)}});
 if(drawing&&dragStart&&dragNow){g.strokeStyle='#fff';g.setLineDash([6,4]);g.strokeRect(dragStart.x,dragStart.y,dragNow.x-dragStart.x,dragNow.y-dragStart.y);g.setLineDash([])}}
const HS=22;  // corner handle size in px (finger sized)
let edit=null;  // zone being moved or resized
function editMode(on){ov.classList.toggle('edit',on||drawing);paint()}
function drawMode(){if(zones.length>=6){toast('Six places is plenty, even for Holmes.');return}drawing=true;ov.classList.add('edit');$('drawHint').textContent='Drag a box over the place of interest...'}
const pos=e=>{const r=ov.getBoundingClientRect();return{x:e.clientX-r.left,y:e.clientY-r.top}};
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
ov.addEventListener('pointerdown',e=>{const p=pos(e),W=ov.width,H=ov.height;
 if(drawing){e.preventDefault();dragStart=p;dragNow=p;ov.setPointerCapture(e.pointerId);return}
 if(!$('editZ').checked)return;
 for(let i=zones.length-1;i>=0;i--){const z=zones[i],x1=(z.x+z.w)*W,y1=(z.y+z.h)*H;
  const mode=p.x>=x1-HS-6&&p.x<=x1+6&&p.y>=y1-HS-6&&p.y<=y1+6?'size':p.x>=z.x*W&&p.x<=x1&&p.y>=z.y*H&&p.y<=y1?'move':null;
  if(mode){e.preventDefault();edit={i,mode,p,z0:{...z}};ov.setPointerCapture(e.pointerId);return}}});
ov.addEventListener('pointermove',e=>{if(drawing&&dragStart){dragNow=pos(e);paint();return}
 if(!edit)return;const p=pos(e),dx=(p.x-edit.p.x)/ov.width,dy=(p.y-edit.p.y)/ov.height,z=zones[edit.i],o=edit.z0;
 if(edit.mode=='move'){z.x=clamp(o.x+dx,0,1-o.w);z.y=clamp(o.y+dy,0,1-o.h)}else{z.w=clamp(o.w+dx,.03,1-o.x);z.h=clamp(o.h+dy,.03,1-o.y)}paint()});
ov.addEventListener('pointerup',e=>{if(edit){edit=null;$('drawHint').textContent='Moved. Remember to Save places.';return}
 if(!drawing||!dragStart)return;const a=dragStart,b=pos(e),W=ov.width,H=ov.height;drawing=false;dragStart=null;ov.classList.toggle('edit',$('editZ').checked);
 const z={en:true,name:'',minPct:5,x:Math.min(a.x,b.x)/W,y:Math.min(a.y,b.y)/H,w:Math.abs(b.x-a.x)/W,h:Math.abs(b.y-a.y)/H};
 $('drawHint').textContent='Places of interest are motion zones. Remember to Save places.';
 if(z.w<.03||z.h<.03){toast('Too small. Even Holmes needs a bigger window.');return}
 z.name=prompt('Name this place of interest:',pick(['Front door','Window','Biscuit tin','Garden gate','Stairs']))||'Zone';zones.push(z);renderZones();paint()});
function renderZones(){$('zones').innerHTML=zones.map((z,i)=>`<tr><td><input type="checkbox" ${z.en?'checked':''} onchange="zones[${i}].en=this.checked;paint()"></td>
 <td><input value="${esc(z.name)}" maxlength="19" onchange="zones[${i}].name=this.value;paint()"></td>
 <td><div class="row" style="margin:0;flex-wrap:nowrap"><input type="range" min="1" max="100" value="${z.minPct}" style="flex:1;accent-color:var(--red)"
   oninput="zones[${i}].minPct=+this.value;$('zv${i}').textContent=this.value+'%';paint()"><b id="zv${i}" style="width:38px;text-align:right">${z.minPct}%</b></div>
  <div class="meter" style="height:6px;margin-right:42px" title="movement right now"><i id="zm${i}" style="width:0"></i><b style="left:${z.minPct}%"></b></div></td>
 <td id="zp${i}">-</td><td><button class="btn plain" onclick="zones.splice(${i},1);renderZones();paint()">&#10005;</button></td></tr>`).join('')||
 '<tr><td colspan="5" class="small">No places marked: the whole picture is watched (5% change).</td></tr>'}
async function saveZones(){try{await api('/api/config',{method:'POST',body:JSON.stringify({zones})});toast('Places of interest noted.')}catch(e){toast('Failed: '+e.message)}}

// ---------- arm / test ----------
async function toggleArm(){const on=st.armed?0:1;await api('/api/arm?on='+on,{method:'POST'});toast(on?'The game is afoot!':'Holmes retires to his violin.');status()}
async function sleepNow(){if(!confirm('Holmes sleeps until the wake pin or the BOOT button. The page goes dark. Proceed?'))return;try{await api('/api/sleep',{method:'POST'});toast('Good night, Holmes.')}catch(e){toast(e.message)}}
async function meshTest(){$('meshMsg').textContent='Releasing the pigeon...';try{$('meshMsg').textContent=await (await api('/api/mesh/test',{method:'POST'})).text()}catch(e){$('meshMsg').textContent=e.message}}
// camp mode has no internet clock: the visiting phone sets it (the house ignores it when its own clock is right)
let timeSent=false;function syncTime(){if(timeSent||st.timeOk!==false)return;timeSent=true;api('/api/settime?ms='+Date.now(),{method:'POST'}).catch(()=>{timeSent=false})}
async function bell(){try{await api('/api/siren?on='+(st.siren?0:1),{method:'POST'});toast(st.siren?'Silence in the house.':'Ring the bell, Watson!');status()}catch(e){toast(e.message)}}
async function test(){await api('/api/test',{method:'POST'});toast(pick(REMARKS.test))}

// ---------- case log ----------
async function loadLog(){let ev=[];try{ev=await (await api('/api/events')).json()}catch(e){return}
 $('log').innerHTML=ev.map(e=>`<tr><td>${esc(e.t)}</td><td><span class="kind k-${esc(e.k)}">${esc(e.k)}</span></td><td>${esc(e.w)}</td>
 <td>${esc(e.d)}<div class="quote">${REMARKS[e.k]?esc(pick(REMARKS[e.k])):''}</div></td></tr>`).join('')||'<tr><td colspan="4">Nothing yet. A quiet day at Baker Street.</td></tr>'}

// ---------- evidence locker ----------
let files=[],totalFiles=0;
async function loadFiles(){try{const r=await api('/api/files');totalFiles=+r.headers.get('X-Total-Files')||0;files=await r.json()}catch(e){toast('The locker is jammed: '+e.message);return}files.sort((a,b)=>a[0]<b[0]?1:-1);
 const days={};files.forEach(f=>{const m=f[0].match(/^(\d{8})_/);if(m)days[m[1]]=(days[m[1]]||0)+1});
 $('fday').innerHTML=Object.keys(days).sort().reverse().map(d=>`<option value="${d}">${d.slice(0,4)}-${d.slice(4,6)}-${d.slice(6)} (${days[d]} items)</option>`).join('')||'<option value="">no dated evidence</option>';
 renderFiles()}
async function burn(day,what){try{const n=await (await api('/api/deletebulk?day='+day+'&keeplog='+($('keepLog').checked?1:0),{method:'POST'})).text();
 toast(n+' item(s) burned. '+pick(['Very suspicious.','Moriarty would approve.','Mrs. Hudson will sweep up the ashes.']));loadFiles()}catch(e){toast(e.message)}}
function burnDay(){const d=$('fday').value;if(!d)return;if(confirm('Burn all evidence from '+$('fday').selectedOptions[0].text+'?'))burn(d)}
function burnAll(){if(confirm('Burn EVERYTHING in the locker?')&&confirm('Really? Holmes will be most displeased.'))burn('all')}
function renderFiles(){const t=$('ftype').value,l=files.filter(f=>!t||f[0].endsWith(t));let photos=0;
 $('fcount').textContent=l.length+' item(s)'+(totalFiles>files.length?` (newest ${files.length} of ${totalFiles} shown)`:'');
 $('files').innerHTML=l.slice(0,200).map(f=>{const n=encodeURIComponent(f[0]),u='/file?name='+n,kb=(f[1]/1024).toFixed(0)+' KB';let v;
  if(f[0].endsWith('.jpg'))v=photos++<40?`<img loading="lazy" src="${u}" onclick="window.open('${u}')">`:`<div class="ic" onclick="window.open('${u}')">&#128247;</div>`;
  else if(f[0].endsWith('.wav'))v=`<div class="ic">&#127897;</div><audio controls preload="none" src="${u}"></audio>`;
  else if(f[0].endsWith('.avi'))v=`<div class="ic" style="cursor:pointer" title="Play" onclick="play('${esc(f[0])}')">&#9654;&#65039;</div><button class="btn" onclick="play('${esc(f[0])}')">Play</button>`;else v=`<div class="ic">&#128220;</div>`;
  return `<div class="clue">${v}<div>${esc(f[0])}</div><div class="row"><span class="small">${kb}</span>
  <a class="btn plain" href="${u}&dl=1">Download</a><button class="btn red" onclick="del('${esc(f[0])}')">Burn</button></div></div>`}).join('')||'<div class="small">The locker is empty. Mrs. Hudson has been dusting.</div>'}
async function del(n){if(!confirm('Burn '+n+'? This cannot be undone.'))return;try{await api('/api/delete?name='+encodeURIComponent(n),{method:'POST'});toast('Evidence destroyed. Very suspicious.');loadFiles()}catch(e){toast(e.message)}}

// ---------- the loudness threshold: drag the red line on the meter, saved on release ----------
let thrDrag=null;
function thrAt(e){const r=$('micMeter').getBoundingClientRect();return Math.round(Math.max(0,Math.min(1,(e.clientX-r.left)/r.width))*90-90)}
function thrShow(v){$('micThr').style.left=(v+90)/90*100+'%';$('micTxt').textContent=(st.mic??'')+' dB, threshold '+v+' dB'}
$('micMeter').addEventListener('pointerdown',e=>{$('micMeter').setPointerCapture(e.pointerId);thrDrag=thrAt(e);thrShow(thrDrag)});
$('micMeter').addEventListener('pointermove',e=>{if(thrDrag==null)return;thrDrag=thrAt(e);thrShow(thrDrag)});
$('micMeter').addEventListener('pointerup',async()=>{if(thrDrag==null)return;const v=thrDrag;
 try{await api('/api/config',{method:'POST',body:JSON.stringify({soundDb:v})});st.soundDb=cfg.soundDb=v;
  const f=$('f_soundDb');if(f){f.value=v;if(f.previousElementSibling)f.previousElementSibling.value=v}toast('Threshold set to '+v+' dB.')}
 catch(e){toast('Failed: '+e.message)}thrDrag=null});
$('micMeter').addEventListener('pointercancel',()=>thrDrag=null);

// ---------- statistics and timeline ----------
let stats=null,tFiles=[],tDayShown='',tMarks=[];
const COL={jpg:'#a8792a',avi:'#8e2a2a',wav:'#5c3b7a'},TYPES=['jpg','avi','wav'];
const dayTxt=d=>d.slice(0,4)+'-'+d.slice(4,6)+'-'+d.slice(6);
async function loadStats(){try{stats=await (await api('/api/stats')).json()}catch(e){return}
 const ds=stats.days.map(x=>x.d).reverse(),opt=ds.map(d=>`<option value="${d}">${dayTxt(d)}</option>`).join('');
 const keep=$('hDay').value;$('hDay').innerHTML='<option value="">all days</option>'+opt;$('hDay').value=keep;
 const tk=$('tDay').value;$('tDay').innerHTML=opt||'<option value="">no dated evidence</option>';if(tk&&ds.includes(tk))$('tDay').value=tk;
 drawStats();loadTimeline()}
// a canvas sized to its box in CSS pixels, sharp on retina screens
function prep(c){const r=window.devicePixelRatio||1,w=c.clientWidth,h=+c.getAttribute('height');c.width=w*r;c.height=h*r;c.style.height=h+'px';
 const g=c.getContext('2d');g.setTransform(r,0,0,r,0,0);g.clearRect(0,0,w,h);return[g,w,h]}
// stacked bars: rows = [{label, jpg, avi, wav}], a label on every n-th bar
function bars(c,rows,every){const[g,W,H]=prep(c),L=30,B=18,T=6,n=Math.max(rows.length,1),bw=(W-L-4)/n,max=Math.max(1,...rows.map(r=>r.jpg+r.avi+r.wav));
 g.font='10px Georgia';g.fillStyle='#6b5536';g.strokeStyle='#e0d0a8';g.textAlign='right';
 for(let i=0;i<=4;i++){const v=Math.round(max*i/4),y=H-B-(H-B-T)*i/4;g.beginPath();g.moveTo(L,y);g.lineTo(W,y);g.stroke();g.fillText(v,L-4,y+3)}
 g.textAlign='center';rows.forEach((r,i)=>{let y=H-B;const x=L+i*bw;TYPES.forEach(t=>{const h=(H-B-T)*r[t]/max;g.fillStyle=COL[t];g.fillRect(x+bw*.12,y-h,bw*.76,h);y-=h});
  if(i%every==0){g.fillStyle='#6b5536';g.fillText(r.label,x+bw/2,H-5)}})}
function drawStats(){if(!stats)return;const days=stats.days.slice(-30),tot=t=>stats.days.reduce((a,x)=>a+x[t],0);
 $('sumRow').innerHTML=[['Clues',stats.total],['Photographs',tot('jpg')],['Moving pictures',tot('avi')],['Recordings',tot('wav')],['Days',stats.days.length],
  ['Card used',(stats.mb/1024).toFixed(1)+' GB'],['Card free',(stats.freeMB/1024).toFixed(1)+' GB']].map(s=>`<div class="stat"><b>${s[1]}</b><span class="small">${s[0]}</span></div>`).join('');
 bars($('chDays'),days.map(x=>({label:x.d.slice(6)+'/'+x.d.slice(4,6),jpg:x.jpg,avi:x.avi,wav:x.wav})),Math.ceil(days.length/10));
 const hd=$('hDay').value,sel=hd?stats.days.filter(x=>x.d==hd):stats.days,hours=Array(24).fill(0),src={};
 sel.forEach(x=>{x.h.forEach((v,i)=>hours[i]+=v);for(const k in x.s)src[k]=(src[k]||0)+x.s[k]});
 // the hour counts cover every file, so split each bar by the share of each type
 const all=sel.reduce((s,x)=>s+x.jpg+x.avi+x.wav,0)||1,share=t=>sel.reduce((s,x)=>s+x[t],0)/all;
 bars($('chHours'),hours.map((v,i)=>({label:i,jpg:v*share('jpg'),avi:v*share('avi'),wav:v*share('wav')})),3);
 const sum=Object.values(src).reduce((a,b)=>a+b,0)||1;
 $('srcTab').innerHTML=Object.entries(src).sort((a,b)=>b[1]-a[1]).map(([k,v])=>`<tr><td><span class="kind k-${esc(k)}">${esc(k)}</span></td>
  <td style="width:50%"><div class="meter" style="height:10px"><i style="width:${v/sum*100}%"></i></div></td><td>${v}</td></tr>`).join('')||'<tr><td class="small">No clues yet.</td></tr>'}
async function loadTimeline(){const d=$('tDay').value;if(!d){tFiles=[];drawTimeline();return}
 if(d!=tDayShown){try{tFiles=await (await api('/api/files?day='+d)).json();tDayShown=d}catch(e){$('tInfo').textContent='Could not load: '+e.message;return}}
 drawTimeline()}
const tSec=n=>(+n.slice(9,11))*3600+(+n.slice(11,13))*60+(+n.slice(13,15)),tType=n=>n.endsWith('.jpg')?'jpg':n.endsWith('.avi')?'avi':n.endsWith('.wav')?'wav':null;
// one lane per type across 24 hours; every clue is a tick at its time
function drawTimeline(){const[g,W,H]=prep($('chTime')),L=8,R=8,lane=(H-24)/3;tMarks=[];g.font='10px Georgia';g.textAlign='center';
 for(let h=0;h<=24;h+=3){const x=L+(W-L-R)*h/24;g.strokeStyle='#e0d0a8';g.beginPath();g.moveTo(x,0);g.lineTo(x,H-16);g.stroke();g.fillStyle='#6b5536';g.fillText(h+':00',Math.min(W-16,Math.max(14,x)),H-4)}
 tFiles.forEach(f=>{const t=tType(f[0]);if(!t||f[0][8]!='_')return;const x=L+(W-L-R)*tSec(f[0])/86400,y=4+TYPES.indexOf(t)*lane;
  g.fillStyle=COL[t];g.fillRect(x-1,y,3,lane-4);tMarks.push([x,y+lane/2,f[0]])});
 $('tInfo').textContent=tFiles.length+' clue(s) on this day. Tap a mark to open it.'}
$('chTime').addEventListener('click',e=>{const r=$('chTime').getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top;let best=null,bd=14;
 tMarks.forEach(m=>{const dd=Math.hypot(m[0]-x,(m[1]-y)/3);if(dd<bd){bd=dd;best=m[2]}});if(!best)return;
 $('tInfo').textContent=best;const u='/file?name='+encodeURIComponent(best);
 if(best.endsWith('.avi'))play(best);else if(best.endsWith('.wav'))new Audio(u).play();else window.open(u)});
window.addEventListener('resize',()=>{if(tab=='files'){drawStats();drawTimeline()}});

// ---------- settings (built from the schema) ----------
let schema=[],pins=[];
// Saving is only allowed once schema and config have both loaded, or empty fields could overwrite the real settings
let setLoaded=false;
async function loadSettings(){setLoaded=false;$('sealBtn').disabled=true;let s,c;
 for(let i=1;i<=4&&!c;i++){$('saveMsg').textContent=i>1?'Fetching the instructions again (try '+i+')...':'Fetching the instructions...';
  try{s=await (await api('/api/schema')).json();c=await (await api('/api/config')).json()}catch(e){c=null;await new Promise(r=>setTimeout(r,1000*i))}}
 if(!c||!s){$('saveMsg').innerHTML='Could not fetch the settings. <button class="btn plain" onclick="loadSettings()">Try again</button>';return}
 pins=s.find(x=>x.pins).pins;schema=s.filter(x=>x.k);cfg=c;zones=cfg.zones.filter(z=>z.w>0);
 const groups={};schema.forEach(f=>(groups[f.g]=groups[f.g]||[]).push(f));
 const keys=Object.keys(groups),secOf=g=>SECTION.find(x=>g.includes(x[0]))||['','&#128269;','','More','#6b5536'];
 // a section counts as "on" when its first tick box is ticked (Detect motion, Radar enabled, ...); "Armed" for HQ
 const onOf=g=>{const b=groups[g].find(f=>f.t=='b');return b?!!cfg[b.k]:null};
 $('jump').innerHTML=keys.map((g,i)=>{const s=secOf(g),on=onOf(g),n=(g.match(/Tripwire (\d)/)||[])[1];
  return `<button style="--sc:${s[4]}" onclick="jumpTo(${i})"><span class="ico">${s[1]}</span>${s[3]}${n?' '+n:''}${on==null?'':`<i class="dot ${on?'on':''}"></i>`}</button>`}).join('');
 $('form').innerHTML=keys.map((g,i)=>{const s=secOf(g),on=onOf(g);
  return `<fieldset id="sec${i}" style="--sc:${s[4]}"><details class="sec"${i==0?' open':''}><summary><span class="ico">${s[1]}</span><span class="ttl">${esc(g)}</span>${on==null?'':`<span class="onoff ${on?'on':''}">${on?'ON':'off'}</span>`}<span class="arw">&#9654;</span></summary>
   <div class="body"><div class="help">${s[2]}</div>${groups[g].map(field).join('')}
   <div class="row" style="margin-top:8px"><button class="btn seal" onclick="saveSettings()">&#128367;&#65039; Seal and send</button><span class="small secmsg"></span></div></div></details></fieldset>`}).join('');
 const w=$('f_wifiSsid');if(w&&!w.value&&st.ssid)w.placeholder='now: '+st.ssid+' (built in at compile time)';
 setLoaded=true;$('sealBtn').disabled=false;$('saveMsg').textContent=''}
// after saving, refresh the ON/off badges and the green dots on the tiles
function refreshOnOff(){const groups={};schema.forEach(f=>(groups[f.g]=groups[f.g]||[]).push(f));
 Object.keys(groups).forEach((g,i)=>{const b=groups[g].find(f=>f.t=='b');if(!b)return;const on=$('f_'+b.k).checked,sec=$('sec'+i);
  const o=sec&&sec.querySelector('.onoff');if(o){o.textContent=on?'ON':'off';o.classList.toggle('on',on)}
  const d=$('jump').children[i]&&$('jump').children[i].querySelector('.dot');if(d)d.classList.toggle('on',on)})}
function jumpTo(i){const f=$('sec'+i);f.querySelector('details').open=true;f.scrollIntoView({behavior:'smooth',block:'start'})}
function openAll(o){document.querySelectorAll('#form details').forEach(d=>d.open=o)}
// icon, tagline, tile label and colour per settings section (matched by a word in the section title)
const SECTION=[['Baker Street','&#127913;&#127931;','Where Holmes keeps his slippers, his violin and the master switch.','HQ','#7b1e1e'],
 ['Second Witness','&#128373;&#65039;&#129309;','Two sensors must agree before the alarm sounds. One startled cat is not a burglar.','Witness','#4a6b2e'],
 ['Timetable','&#128197;&#128373;&#65039;','Mrs. Hudson locks up at night and unlocks when you come home. She notices your phone.','Timetable','#8e5a1e'],
 ['Telegraph Office','&#128225;&#9749;','How the house talks to the outside world. Leave Wi-Fi blank to keep the built-in network.','Wi-Fi','#1f5f8b'],
 ['Telegram Boy','&#128692;&#128232;','A lad on a bicycle who rushes news to your pocket. Faster than Royal Mail.','Telegram','#2a8bc9'],
 ['Camera Obscura','&#128247;&#127913;','The all-seeing eye. Does not blink, does not sleep, does not take biscuits.','Camera','#5b3f8c'],
 ['Listening Post','&#128066;&#127908;','Hears coughs, creaks and confessions.','Sound','#b5651d'],
 ['Watching Brief','&#128065;&#65039;&#128373;&#65039;','Spots movement in the places of interest you marked on The Scene.','Motion','#2e7d32'],
 ['Tripwire','&#129700;&#128682;','Doors, PIRs and buttons. One wire, one suspect.','Tripwire','#a33b3b'],
 ['Radar','&#128752;&#65039;&#127787;&#65039;','Sees people through the London fog (and thin walls).','Radar','#00796b'],
 ['Bat Ears','&#129415;&#128266;','Squeaks at things and listens for the echo. The bat insists on a 5V-to-3.3V divider.','Ultrasonic','#6d4c41'],
 ['Night Watch','&#127769;&#128294;','When the gas lamps go out, Holmes lights his secret lantern and sees in black and white.','Night','#283593'],
 ['Alarm Bell','&#128680;&#128276;','A bell loud enough to wake Mycroft. Rings when a sensor with Ring the alarm bell ticked fires.','Alarm bell','#c62828'],
 ['Pigeon Post','&#128038;&#128225;','Carrier pigeons with LoRa radios. Flies miles over the woods with no Wi-Fi and no phone signal.','LoRa','#558b2f'],
 ['Camp','&#9978;&#65039;&#128164;','Holmes on holiday: sleeps on a battery, wakes for intruders, opens the hotspot when you press BOOT.','Camp','#33691e'],
 ['Diogenes Club','&#127963;&#65039;&#129323;','Mycroft never speaks, but he reads every report: MQTT and Home Assistant.','MQTT / HA','#455a64']];
const ACTS=[['photo','&#128247;','Photos'],['video','&#127909;','Video'],['audio','&#127908;','Audio'],['notify','&#128232;','Alert'],['siren','&#128276;','Bell']];
function segPick(b,id,val){const e=$(id);if(e.type=='checkbox')e.checked=val=='1';else e.value=val;
 b.parentNode.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x==b))}
function segOf(f,id,v,isBool){const opts=f.o.split('|').map(o=>{const i=o.indexOf(':');return [o.slice(0,i),o.slice(i+1)]});
 const cur=isBool?(v?'1':'0'):String(v);
 return `<span class="seg">${isBool?`<input type="checkbox" id="${id}" hidden ${v?'checked':''}>`:`<input type="hidden" id="${id}" value="${esc(cur)}">`}${opts.map(([k,l])=>`<button type="button" class="${k==cur?'on':''}" onclick="segPick(this,'${id}','${k}')">${esc(l)}</button>`).join('')}</span>`}
function field(f){const v=cfg[f.k],id='f_'+f.k;let input;
 const am=f.t=='b'&&f.k.match(/^(.*)_(photo|video|audio|notify|siren)$/);
 if(am){if(am[2]!='photo')return '';  // the five actions share one row of chips
  return `<div class="acts"><span class="small" style="margin-right:4px">When it fires:</span>${ACTS.map(a=>{const k=am[1]+'_'+a[0];
   return `<label class="chip" title="${a[2]}"><input type="checkbox" id="f_${k}" ${cfg[k]?'checked':''}>${a[1]} ${a[2]}</label>`}).join('')}</div>`}
 if(f.t=='b'&&f.o)input=segOf(f,id,v,true);
 else if(f.t=='b')input=`<label class="sw"><input type="checkbox" id="${id}" ${v?'checked':''}><span></span></label>`;
 else if(f.t=='e'&&f.o.split('|').length<=3)input=segOf(f,id,v,false);
 else if(f.t=='i'&&f.max-f.min<=200)input=`<span style="display:flex;gap:6px;align-items:center"><input type="range" value="${v}" min="${f.min}" max="${f.max}" style="width:130px;accent-color:var(--red)"
  oninput="$('${id}').value=this.value"><input type="number" id="${id}" value="${v}" min="${f.min}" max="${f.max}" style="width:70px" oninput="this.previousElementSibling.value=this.value"></span>`;
 else if(f.t=='i')input=`<input type="number" id="${id}" value="${v}" min="${f.min}" max="${f.max}">`;
 else if(f.t=='p')input=`<select id="${id}"><option value="-1">none</option>${pins.map(p=>`<option value="${p[0]}" ${p[0]==v?'selected':''}>${esc(p[1])}</option>`).join('')}</select>`;
 else if(f.t=='e')input=`<select id="${id}">${f.o.split('|').map(o=>{const[k,l]=o.split(':');return `<option value="${k}" ${k==v?'selected':''}>${esc(l)}</option>`}).join('')}</select>`;
 else input=`<input type="${f.t=='w'?'password':'text'}" id="${id}" value="${esc(v)}">`;
 return `<div class="f"><label for="${id}">${esc(f.l)}</label>${input}</div>${f.h?`<div class="help">${esc(f.h)}</div>`:''}`}
async function saveSettings(){if(!setLoaded||!schema.length){toast('The settings have not loaded yet.');return}
 const o={};schema.forEach(f=>{const e=$('f_'+f.k);if(!e)return;o[f.k]=f.t=='b'?e.checked:(f.t=='s'||f.t=='w')?e.value:+e.value});
 try{const r=await (await api('/api/config',{method:'POST',body:JSON.stringify(o)})).text();$('saveMsg').textContent='Sealed and delivered.';toast('Watson has the instructions.');refreshOnOff();
  document.querySelectorAll('.secmsg').forEach(e=>{e.textContent='Sealed and delivered.';setTimeout(()=>e.textContent='',4000)});
  if(r=='restart'&&confirm('Network settings changed. Restart now to use them?'))restart();status()}catch(e){$('saveMsg').textContent='Failed: '+e.message}}

// ---------- telegram ----------
async function tg(what){$('tgMsg').textContent='Telegraphing...';try{let r=await (await api('/api/tg/'+what,{method:'POST'})).text();
 if(r.startsWith('OK:')){const p=r.split(':');r='Found chat '+p[1]+' ('+p[2]+') and saved it.';const e=$('f_tgChat');if(e)e.value=p[1]}$('tgMsg').textContent=r}catch(e){$('tgMsg').textContent=e.message}}

// ---------- the kinetoscope: plays our MJPEG .avi files right in the browser ----------
let K={frames:[],i:0,timer:null,url:null,data:null,us:100000,playing:true};
async function play(name){$('kName').textContent=name;$('kInfo').textContent='Fetching the reel...';$('kino').classList.add('on');$('kImg').removeAttribute('src');
 let buf;try{buf=await (await api('/file?name='+encodeURIComponent(name))).arrayBuffer()}catch(e){$('kInfo').textContent='Could not load: '+e.message;return}
 const v=new DataView(buf),u8=new Uint8Array(buf),tag=o=>String.fromCharCode(u8[o],u8[o+1],u8[o+2],u8[o+3]);
 if(buf.byteLength<240||tag(0)!='RIFF'||tag(8)!='AVI '){$('kInfo').textContent='Not an AVI reel.';return}
 K.us=v.getUint32(32,true)||100000;K.frames=[];
 // walk the chunks, descending into LIST movi, and collect every 00dc (JPEG frame)
 const walk=(o,end)=>{while(o+8<=end){const id=tag(o),sz=v.getUint32(o+4,true);
  if(id=='LIST'){if(tag(o+8)=='movi')walk(o+12,Math.min(o+8+sz,end))}else if(id=='00dc'&&sz)K.frames.push([o+8,sz]);o+=8+sz+(sz&1)}};
 walk(12,buf.byteLength);K.data=u8;
 if(!K.frames.length){$('kInfo').textContent='The reel is empty.';return}
 $('kPos').max=K.frames.length-1;K.i=0;K.playing=true;$('kPlay').textContent='Pause';kShow();kRun()}
function kShow(){const f=K.frames[K.i];if(K.url)URL.revokeObjectURL(K.url);K.url=URL.createObjectURL(new Blob([K.data.subarray(f[0],f[0]+f[1])],{type:'image/jpeg'}));
 $('kImg').src=K.url;$('kSave').href=K.url;$('kPos').value=K.i;const s=K.us/1e6;
 $('kInfo').textContent=(K.i*s).toFixed(1)+' / '+(K.frames.length*s).toFixed(1)+' s ('+(1e6/K.us).toFixed(1)+' fps)'}
function kRun(){clearTimeout(K.timer);if(!K.playing)return;K.timer=setTimeout(()=>{if(K.i<K.frames.length-1){K.i++;kShow();kRun()}else{K.playing=false;$('kPlay').textContent='Play'}},K.us/1000/+$('kSpeed').value)}
function kToggle(){if(!K.frames.length)return;K.playing=!K.playing;if(K.playing&&K.i>=K.frames.length-1)K.i=0;$('kPlay').textContent=K.playing?'Pause':'Play';kRun()}
function kSeek(i){K.i=i;kShow()}
function closeKino(){clearTimeout(K.timer);K.playing=false;$('kino').classList.remove('on');if(K.url)URL.revokeObjectURL(K.url);K.url=null;K.data=null;K.frames=[]}

// ---------- the chronicles: newest first; add an entry for every new version ----------
const CHRONICLES=[
 ['3.8','2026-11-07','The Adventure of the Second Witness',['The Second Witness: optionally only alarm when two different sensors fire within a few seconds (fewer false alarms from animals and wind)', 'Tripwire 4 defaults to an RCWL-0516 microwave sensor (pull-down, goes HIGH)']],
 ['3.7','2026-11-07','The Tidy Study',['Cleaner settings: ON/OFF switches, HIGH/LOW buttons instead of drop-downs, and the five actions as one row of chips', 'Alarm bell: ON/OFF switch and Active HIGH / Active LOW buttons, also on The Scene (applied at once)', 'Sensors row on The Scene: switch each sensor on or off, or all at once (All back on restores the ones you had)']],
 ['3.6','2026-11-07','The Problem of the Vanishing Card',['Radar trigger distance slider on The Scene, saved at once without restarting the radar', 'Fix: the piezo tone could take the camera clock (LEDC timer 0) and stop the pictures', 'The camera restarts itself if no pictures arrive, and a missing SD card is retried every minute', 'Restarts say which watchdog fired; a normal software restart is no longer shown in red']],
 ['3.5','2026-11-06','The Adventure of the Singing Piezo',['Alarm bell: new Piezo wail and Piezo beeps sounds, so a passive piezo really sounds instead of clicking','Tripwire debounce is now a setting (Baker Street HQ), and pins are read every 5 ms','The Scene refreshes its lamps twice a second']],
 ['3.4','2026-11-06','The Case of the Silent Bell',['Fix: the alarm bell on D1 did nothing, because the camp wake pin (also D1) was reset to an input even with camp mode off','Fix: a tripwire showed ? after saving settings until the pin changed','Warning when the camp wake pin is also the alarm bell or IR lamp pin']],
 ['3.3','2026-11-06','The Page That Would Not Starve',['The web page runs in its own task on the other processor core, so it stays quick while the camera records video and photos non-stop']],
 ['3.2','2026-11-06','The Return of the Wireless',['On the hotspot, the house keeps trying your Wi-Fi once a minute and switches back by itself (never while a phone is on the hotspot)','The firmware upload refuses the 8 MB full USB image and stops at the first bad block instead of hanging']],
 ['3.1','2026-11-06','A Study in Scarlet (and Green, and Blue)',['Instructions to Watson: a big coloured tile per section to jump straight to it, with a green dot when it is switched on','Each section has its own colour, a big icon, an ON/off badge, opens and closes with a tap, and has its own Seal and send button','Back-to-the-tiles button in the corner']],
 ['3.0','2026-11-06','The Adventure of the Sleeping Detective',['Camp mode: deep sleep on a battery, wakes on the PIR, records with no Wi-Fi and sleeps again when quiet','Press BOOT to wake it with the hotspot Baker-Street-221B (or your Wi-Fi) and look at the evidence','The Pigeon Post: alerts over a Meshtastic LoRa radio on a UART, with a test button','Optional heartbeat over LoRa with battery voltage and card space','Battery voltage in the health panel and a Send Holmes to sleep now button','The visiting phone sets the clock when there is no internet']],
 ['2.9','2026-11-06','The Hound in the Dark',['The Night Watch: switches to black and white and lights an IR lamp when it gets dark, by the clock, or always','Light level shown live on The Scene and written to the case log','The Alarm Bell: a siren, buzzer or LED output, steady or beeping, rung by any sensor you choose; silenced when disarmed','The Diogenes Club: MQTT with Home Assistant auto discovery (armed switch, alarm bell, test button, clue, loudness, light, temperature and more)']],
 ['2.8','2026-11-05',"The Doctor's Report",['Health panel on The Scene: Wi-Fi, chip temperature, uptime, memory, PSRAM, card and restarts, each with a lamp','Restarts are counted and their reason is shown and written to the case log']],
 ['2.7','2026-11-05','The Tidy Lodger',['When the card is over 90% full (adjustable) the oldest days are deleted until there is room','Today and the case log are never touched; each clean-up is written in the case log']],
 ['2.6','2026-11-05',"Mrs Hudson's Timetable",['Arm and disarm on a daily schedule; the button still wins until the next scheduled minute','Watches for your phone on the Wi-Fi: arms when it leaves, disarms when it comes back','The morning disarm is skipped while the phone is out']],
 ['2.5','2026-11-05','The Adventure of the Red Line',['Drag the red threshold line on the Listening Post meter; it is saved at once','Settings retry loading; "Seal and send" waits until they have loaded, so empty settings can never be sent','The time zone is set at boot, before the first file is named']],
 ['2.4','2026-11-05','The Statistician\'s Notebook',['Evidence Locker statistics: totals, clues per day, by hour and by sensor','The Timeline: a whole day at a glance; tap a mark to open the clue','Fix: files saved right after a restart were named in UTC']],
 ['2.3','2026-11-05','The Sign of the Swift Lens',['Live view asks for the next picture as soon as the last one arrives (shows fps)',
  'Places of interest can be drawn, moved and resized with a finger on iPhone','Sliders for sensitivity and number settings','Fix: network timeouts were 1000x too short (downloads cut off, Telegram could fail)','Fix: a locker with thousands of files no longer freezes the web page','Fix: Telegram can no longer trip the watchdog and reboot the board','Fix: video no longer starves the web page; a web watchdog restarts a stuck page','Evidence is kept in one folder per day, so saving stays fast with thousands of clues; SD bus 5x faster']],
 ['2.2','2026-11-05','The Adventure of the Pocket Telegraph',['Telegram alerts with photo, per sensor, with rate limit and "Find my chat ID"',
  'The Kinetoscope: play .avi clips in the browser (iPhone too)','Burn a whole day or everything in the Evidence Locker',
  'Icons and taglines for every settings section','Wi-Fi field shows the network in use','Version history (you are reading it)']],
 ['2.1','2026-11-05','The Case of the Frozen Safari',['Files are sent in small pieces with a timeout, so a paused iPhone download cannot freeze the house',
  'Partial downloads (HTTP range) so audio and video play in Safari','SD card is retried and remounted automatically']],
 ['2.0','2026-11-05','A Study in Sensors',['221B Baker Street web GUI','Motion zones drawn on the live picture','AVI video clips',
  'Up to 4 GPIO tripwires, HLK-LD2450 radar, HC-SR04 ultrasonic','Detailed case log (caselog.csv)','All settings in the web page, OTA firmware upload','No secrets in the source code']],
 ['1.1','2026-11-05','The Quiet Microphone',['Recording volume boost']],
 ['1.0','2026-11-05','The First Case',['Sound trigger with WAV clips and photos','One GPIO trigger','Simple settings page']]];
$('chron').innerHTML=CHRONICLES.map(c=>`<div style="margin-bottom:10px"><b>v${c[0]}</b> <span class="small">${c[1]}</span> &mdash; <i>${c[2]}</i>
 <ul style="margin:4px 0 0 18px;padding:0">${c[3].map(x=>`<li>${esc(x)}</li>`).join('')}</ul></div>`).join('');

// ---------- workshop ----------
// ---------- the Doctor's Report: each line is [name, value, 0 good / 1 watch / 2 trouble] ----------
function health(){if(!$('health')||st.tempC==null)return;const up=st.uptime,d=Math.floor(up/86400),h=Math.floor(up%86400/3600),m=Math.floor(up%3600/60);
 const usedPct=st.totalMB?Math.round(100-st.freeMB*100/st.totalMB):0;
 const rows=[['Wi-Fi signal',st.rssi+' dBm'+(st.rssi>-60?' (strong)':st.rssi>-72?' (fair)':' (weak)'),st.rssi>-67?0:st.rssi>-78?1:2],
  ['Chip temperature',st.tempC+' &deg;C',st.tempC<65?0:st.tempC<80?1:2],
  ['Awake for',`${d}d ${h}h ${m}m`,up>600?0:1],
  ['Free memory',`${st.heap} KB (lowest ${st.minHeap} KB, largest piece ${st.maxBlock} KB)`,st.minHeap>40?0:st.minHeap>20?1:2],
  ['PSRAM free',(st.psram/1024).toFixed(1)+' MB',st.psram>1024?0:st.psram>256?1:2],
  ['SD card',st.sd?(st.totalMB?`${usedPct}% used, ${(st.freeMB/1024).toFixed(1)} GB free`:st.freeMB+' MB free'):'missing!',!st.sd?2:usedPct<85?0:usedPct<95?1:2],
  ['Restarts',`#${st.boots}, last: ${esc(st.reset)}`,/crash|watchdog|brownout/.test(st.reset)?2:0]];
 if(st.mqtt>=0)rows.push(['MQTT broker',st.mqtt==1?'connected':'not reachable',st.mqtt==1?0:2]);
 if(st.battMv>=0)rows.push(['Battery',(st.battMv/1000).toFixed(2)+' V',st.battMv>3700?0:st.battMv>3400?1:2]);
 if(st.camp)rows.push(['Camp mode',`sleeps in ${Math.floor(st.sleepIn/60)}m ${st.sleepIn%60}s (woken by ${esc(st.wakeWhy)}, wake #${st.wakes})`,st.sleepIn>60?0:1]);
 if(st.mesh)rows.push(['Pigeon Post','LoRa radio wired',0]);
 if(!st.timeOk)rows.push(['Clock','not set (no internet)',1]);
 $('campRow').style.display=st.camp?'':'none';
 $('health').innerHTML=rows.map(r=>`<tr><td><span class="lamp ${['lo','hi','rec'][r[2]]}"></span>${r[0]}</td><td>${r[1]}</td></tr>`).join('')}
function sys(){const up=st.uptime,d=Math.floor(up/86400),h=Math.floor(up%86400/3600),m=Math.floor(up%3600/60);
 $('sys').innerHTML=[['Firmware',st.fw],['Clock',st.time],['Awake for',`${d}d ${h}h ${m}m`],['SD card',st.sd?st.freeMB+' MB free':'missing!'],
 ['Wi-Fi signal',st.rssi+' dBm'],['Free memory',st.heap+' KB RAM, '+st.psram+' KB PSRAM']].map(r=>`<tr><td>${r[0]}</td><td>${esc(r[1])}</td></tr>`).join('')}
function ota(){const f=$('fw').files[0];if(!f){toast('Choose a .bin first.');return}
 if(/full/i.test(f.name)||f.size>3000000){$('otaMsg').textContent='That is the full USB image ('+(f.size/1048576).toFixed(1)+' MB). Here use xiao-sherlock-ota.bin.';return}const x=new XMLHttpRequest(),fd=new FormData();fd.append('fw',f);
 x.upload.onprogress=e=>$('otaMsg').textContent=Math.round(e.loaded/e.total*100)+'%';x.onload=()=>{$('otaMsg').textContent=x.responseText;setTimeout(()=>location.reload(),8000)};
 x.open('POST','/api/update');x.send(fd)}
async function restart(){if(!confirm('Restart the device?'))return;try{await api('/api/restart',{method:'POST'})}catch(e){}toast('Restarting... back in 15 seconds.');setTimeout(()=>location.reload(),15000)}

// ---------- go ----------
(async()=>{show(tab);await status();try{cfg=await (await api('/api/config')).json();zones=cfg.zones.filter(z=>z.w>0);renderZones();drawSens()}catch(e){}
 snap();setInterval(()=>{if(tab=='scene'||Date.now()-stLast>=1000)status()},500);setInterval(snap,3000);setInterval(()=>{loadGrid().then(paint)},1000);setInterval(()=>{if(tab=='log')loadLog()},4000);
 window.addEventListener('resize',paint)})();
</script></body></html>)rawliteral";
