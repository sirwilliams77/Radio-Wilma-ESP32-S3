#pragma once
// Web panel page (HTML + CSS + JS in one file, served from flash).
// Amber theme; logo served from /logo.jpg (WebLogo.h). Author: Sir Williams.
#include <Arduino.h>

static const char WEB_PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b0b0d">
<title>Radio Wilma</title>
<style>
:root{--bg:#0b0b0d;--panel:#151518;--panel2:#1d1d22;--line:#2a2a31;--text:#ece7df;--dim:#9b958b;--amb:#ffb300;--amb2:#ff8f00;--ambsoft:rgba(255,179,0,.12);--red:#ff5a4f;--ok:#3ddc84}
*{box-sizing:border-box}
[hidden]{display:none!important}
html,body{margin:0}
html{background:var(--bg)}
body{background:radial-gradient(1200px 500px at 50% -200px,#2a1d05 0,var(--bg) 60%) fixed;color:var(--text);font:15px/1.4 Roboto,system-ui,-apple-system,Segoe UI,sans-serif;min-height:100vh;display:flex;flex-direction:column}
button,input{font:inherit;color:inherit}
svg{display:block}
header{display:flex;align-items:center;gap:14px;padding:12px 24px;border-bottom:1px solid var(--line);background:rgba(11,11,13,.75);position:sticky;top:0;z-index:5;backdrop-filter:blur(6px)}
.brand{display:flex;align-items:center;gap:14px}
.brand img{height:64px;border-radius:10px;box-shadow:0 0 0 1px var(--line)}
.brand b{font-size:20px;letter-spacing:.5px;display:block}.brand span{display:block;font-size:11px;color:var(--dim);letter-spacing:3px;text-transform:uppercase}
.stat{display:flex;gap:16px;align-items:center;color:var(--dim);font-size:13px;margin-left:18px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--dim);display:inline-block;margin-right:6px}.dot.on{background:var(--ok)}
nav{margin-left:auto;display:flex;gap:6px}
nav button{display:flex;align-items:center;gap:8px;padding:10px 14px;border-radius:10px;color:var(--dim);background:none;border:0;cursor:pointer;font-weight:500}
nav button.on{background:var(--ambsoft);color:var(--amb)}nav svg{width:18px;height:18px}
main{flex:1;max-width:1180px;width:100%;margin:0 auto;padding:22px}
.view{display:grid;gap:22px}
#play{grid-template-columns:420px minmax(0,1fr);align-items:start}
.card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);border-radius:18px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35);min-width:0}
.lbl{color:var(--dim);font-size:11px;letter-spacing:3px;text-transform:uppercase}
.player{text-align:center}
.player canvas{width:176px;height:132px;border-radius:14px;margin:4px auto 16px;display:block;background:#101014;box-shadow:0 0 0 1px var(--line),0 12px 30px rgba(255,143,0,.15)}
.player h2{margin:4px 0 2px;font-size:28px;color:var(--amb);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.player .desc{color:var(--dim);font-size:13px;min-height:18px}
.track{margin:18px 0 6px;padding:14px;border-radius:12px;background:#111114;border:1px solid var(--line)}
.track .a{font-weight:700;font-size:18px;margin-top:4px;min-height:24px}.track .t{color:var(--amb);min-height:21px}
.chips{display:flex;justify-content:center;gap:8px;margin-top:12px;flex-wrap:wrap}
.chip{display:flex;gap:6px;align-items:center;padding:5px 10px;border-radius:999px;background:var(--panel2);border:1px solid var(--line);font-size:12px;color:var(--dim)}.chip svg{width:14px;height:14px;color:var(--amb)}
.ctrl{display:flex;justify-content:center;align-items:center;gap:14px;margin:20px 0 14px}
.rb{width:52px;height:52px;border-radius:50%;border:1px solid var(--line);background:var(--panel2);display:grid;place-items:center;cursor:pointer;transition:.15s}
.rb:hover{border-color:var(--amb)}.rb svg{width:22px;height:22px}.rb.on{color:var(--red)}
.rb.big{width:74px;height:74px;background:radial-gradient(circle at 35% 30%,#ffd54f,var(--amb) 45%,var(--amb2));border:0;color:#1a1200;box-shadow:0 8px 24px rgba(255,160,0,.35)}.rb.big svg{width:30px;height:30px}
.rb.num{font-weight:700;font-size:12px;color:var(--dim);cursor:default}
.vol{display:flex;align-items:center;gap:12px}.vol svg{width:20px;height:20px;color:var(--dim);flex:none}.vol b{width:34px;text-align:right}
input[type=range]{-webkit-appearance:none;appearance:none;flex:1;height:8px;border-radius:8px;background:linear-gradient(90deg,var(--amb2),var(--amb)) 0/var(--p,50%) 100% no-repeat,#26262c;outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;box-shadow:0 0 0 4px rgba(255,179,0,.35);cursor:pointer}
input[type=range]::-moz-range-thumb{width:20px;height:20px;border:0;border-radius:50%;background:#fff;box-shadow:0 0 0 4px rgba(255,179,0,.35)}
.lh{display:flex;align-items:center;gap:10px;margin-bottom:14px;flex-wrap:wrap}.lh h3{margin:0;font-size:13px;letter-spacing:3px;color:var(--dim);font-weight:600}
.badge{color:var(--amb);font-size:12px;background:var(--ambsoft);padding:2px 8px;border-radius:999px}
.search{margin-left:auto;display:flex;align-items:center;gap:8px;background:#111114;border:1px solid var(--line);border-radius:10px;padding:0 10px;color:var(--dim);width:240px}.search svg{width:16px;height:16px;flex:none}
.search input{background:none;border:0;outline:none;padding:8px 0;width:100%}
.list{display:grid;gap:8px}
.it{display:flex;align-items:center;gap:14px;padding:10px 12px;border-radius:12px;border:1px solid transparent;background:#121215;cursor:pointer;transition:.15s;min-width:0}
.it:hover{border-color:var(--line)}.it.drop{border-color:var(--amb);border-style:dashed}
.it .no{width:26px;color:var(--dim);font-weight:700;text-align:right;flex:none}
.it canvas{width:64px;height:48px;border-radius:8px;background:#101014;flex:none}
.it .tx{min-width:0;flex:1}.it .nm{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.it .ds{font-size:12px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.it .pl{width:38px;height:38px;border-radius:50%;display:grid;place-items:center;border:1px solid var(--line);color:var(--dim);flex:none}.it .pl svg{width:16px;height:16px}
.it.cur{background:linear-gradient(90deg,rgba(255,179,0,.16),rgba(255,179,0,.03));border-color:rgba(255,179,0,.45)}.it.cur .nm,.it.cur .no{color:var(--amb)}
.eqi{display:flex;align-items:flex-end;gap:3px;height:18px;width:38px;justify-content:center;flex:none}.eqi s{width:4px;background:var(--amb);border-radius:2px;animation:eq 1s ease-in-out infinite}
.eqi s:nth-child(2){animation-delay:-.4s}.eqi s:nth-child(3){animation-delay:-.7s}.eqi s:nth-child(4){animation-delay:-.2s}.eqi.stop s{animation:none;height:25%}
@keyframes eq{0%,100%{height:30%}50%{height:100%}}
.hint{font-size:12px;color:var(--dim);margin-top:12px}
/* settings */
.tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:16px}
.tile{display:flex;gap:14px;align-items:center;padding:18px;border-radius:16px;background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);cursor:pointer;text-align:left;transition:.15s;min-width:0}
.tile:hover{border-color:#4a3a14}.tile.on{border-color:rgba(255,179,0,.55);box-shadow:0 0 0 1px rgba(255,179,0,.25) inset}
.tile .ic{width:54px;height:54px;border-radius:14px;display:grid;place-items:center;background:var(--ambsoft);color:var(--amb);flex:none}.tile .ic svg{width:28px;height:28px}
.tile b{display:block;font-size:16px}.tile span{display:block;font-size:12px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile>div:last-child{min-width:0}
.two{display:grid;grid-template-columns:1fr 1fr;gap:24px}
label{display:block;font-size:12px;color:var(--dim);margin:12px 0 6px;letter-spacing:1px;text-transform:uppercase}
.in{width:100%;background:#111114;border:1px solid var(--line);border-radius:10px;padding:11px 12px;outline:none}
.in:focus{border-color:var(--amb)}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}
.btn{display:inline-flex;align-items:center;gap:8px;padding:11px 18px;border-radius:10px;background:linear-gradient(180deg,#ffc107,var(--amb2));color:#1a1200;font-weight:700;border:0;cursor:pointer}
.btn.g{background:var(--panel2);color:var(--text);border:1px solid var(--line);font-weight:500}.btn.g:hover{border-color:var(--amb)}
.btn.d{background:var(--panel2);color:var(--red);border:1px solid var(--line);font-weight:500}
.btn svg{width:16px;height:16px}
.net{display:flex;align-items:center;gap:12px;padding:11px 12px;border:1px solid var(--line);border-radius:10px;margin-bottom:8px;background:#121215;cursor:pointer}
.net:hover,.net.cur{border-color:rgba(255,179,0,.45)}.net svg{width:18px;height:18px;color:var(--amb);flex:none}.net small{color:var(--dim)}
.sg{margin-left:auto;display:flex;gap:2px;align-items:flex-end}.sg i{width:4px;background:#3a3a42;border-radius:1px}.sg i.on{background:var(--amb)}
.gauges{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin-top:16px}
.g1{padding:14px;border-radius:12px;background:#111114;border:1px solid var(--line);min-width:0}
.g1 b{display:block;font-size:20px;color:var(--amb);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.g1 span{display:block;font-size:12px;color:var(--dim)}
.pb{height:6px;border-radius:6px;background:#26262c;margin-top:8px;overflow:hidden}.pb i{display:block;height:100%;border-radius:6px;background:linear-gradient(90deg,var(--amb2),var(--amb))}
.mrow{display:flex;align-items:center;gap:12px;padding:10px 12px;border-radius:12px;background:#121215;border:1px solid var(--line);margin-bottom:8px;min-width:0}
.mrow canvas{width:58px;height:44px;border-radius:7px;background:#101014;flex:none}
.mrow .tx{flex:1;min-width:0}.mrow .nm{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.mrow .ds{font-size:12px;color:var(--dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ib{width:36px;height:36px;border-radius:9px;border:1px solid var(--line);background:var(--panel2);display:grid;place-items:center;cursor:pointer;flex:none}.ib:hover{border-color:var(--amb)}.ib svg{width:16px;height:16px}.ib.d{color:var(--red)}
.drop{border:2px dashed var(--line);border-radius:14px;padding:26px;text-align:center;color:var(--dim);cursor:pointer}.drop.over{border-color:var(--amb);color:var(--amb)}
.drop svg{width:34px;height:34px;margin:0 auto 8px;color:var(--amb)}
progress{width:100%;height:8px;accent-color:var(--amb);margin-top:12px}
.wxbig{display:flex;align-items:center;gap:18px;padding:16px;border-radius:14px;background:#111114;border:1px solid var(--line)}
.wxbig b{font-size:40px;color:var(--amb);line-height:1}.wxbig svg{width:54px;height:54px;color:var(--amb)}
dialog{background:var(--panel);color:var(--text);border:1px solid var(--line);border-radius:18px;width:min(560px,94vw);padding:22px}
dialog::backdrop{background:rgba(0,0,0,.7)}
dialog h3{margin:0 0 6px;color:var(--amb)}
.logoedit{display:flex;gap:16px;align-items:center;flex-wrap:wrap}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:10px;overflow:hidden;margin:0 8px 8px 0}
.seg button{background:#111114;border:0;padding:7px 11px;cursor:pointer;color:var(--dim);font-size:13px}
.seg button.on{background:var(--ambsoft);color:var(--amb)}
.opts .k{font-size:11px;color:var(--dim);letter-spacing:1px;text-transform:uppercase;margin:6px 0 4px}
.bulk{display:grid;gap:8px;max-height:55vh;overflow:auto;margin:10px 0}
.brow{display:flex;align-items:center;gap:12px;padding:8px 10px;border:1px solid var(--line);border-radius:10px;background:#121215;min-width:0}
.brow canvas{width:58px;height:44px;border-radius:7px;flex:none}.brow .fn{flex:1;min-width:0;font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.brow select{background:#111114;color:var(--text);border:1px solid var(--line);border-radius:8px;padding:7px;max-width:52%}
.brow.skip{opacity:.5}
.cgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:10px;margin-top:16px}
.cgrp{grid-column:1/-1;font-size:11px;color:var(--amb);letter-spacing:2px;text-transform:uppercase;margin-top:8px}
.crow{display:flex;align-items:center;gap:10px;padding:8px 10px;border:1px solid var(--line);border-radius:10px;background:#121215}
.crow input[type=color]{width:42px;height:32px;border:0;padding:0;background:none;cursor:pointer;flex:none}
.crow span{flex:1;font-size:14px}.crow code{font-size:12px;color:var(--dim)}
.th{display:inline-flex;align-items:center;gap:8px}.th i{width:14px;height:14px;border-radius:50%;display:inline-block;box-shadow:0 0 0 1px #0006}
.logoedit canvas{width:132px;height:99px;border-radius:10px;background:#101014;box-shadow:0 0 0 1px var(--line);flex:none}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:var(--panel2);border:1px solid var(--amb);padding:11px 18px;border-radius:12px;display:none;max-width:90vw;z-index:20;box-shadow:0 10px 30px rgba(0,0,0,.5)}
#toast.err{border-color:var(--red);color:#ffc4bf}
footer{border-top:1px solid var(--line);padding:14px 24px;display:flex;justify-content:space-between;align-items:center;gap:8px;color:var(--dim);font-size:12px}
footer b{color:var(--amb);font-weight:600}footer img{height:22px;border-radius:4px}
@media(max-width:900px){.tiles{grid-template-columns:1fr 1fr}.two{grid-template-columns:1fr}}
@media(max-width:800px){#play{grid-template-columns:minmax(0,1fr)}main{padding:14px}header{padding:10px 14px}.stat,.search,nav span{display:none}.brand img{height:48px}footer{flex-direction:column}}
@media(max-width:480px){.tiles{grid-template-columns:1fr}.ctrl{gap:8px}.rb{width:46px;height:46px}.rb.big{width:64px;height:64px}}
.wxg{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;margin-top:12px}
.wxi{background:#111114;border:1px solid var(--line);border-radius:12px;padding:10px;display:flex;flex-direction:column;align-items:center;gap:4px;text-align:center;min-width:0}
.wxi canvas{width:117px;height:90px;background:#000;border-radius:8px}.wxi b{font-size:13px}.wxi span{font-size:11px;color:var(--dim)}
.wxi .btns{margin-top:6px;justify-content:center;gap:6px}.wxi .btn{padding:7px 12px;font-size:13px}
.sbar{display:flex;gap:8px}.sbar .in{flex:1}
.srl{display:flex;flex-direction:column;gap:8px;margin-top:10px}
.sr{display:flex;align-items:center;gap:12px;padding:12px 14px;border-radius:12px;background:#111114;border:1px solid var(--line)}.sr.off{opacity:.55}
.sri{flex:1;min-width:0;display:flex;flex-direction:column;gap:2px}.sri b,.sri span{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.sri span{font-size:12px;color:var(--dim)}.sri small{font-size:11px;color:var(--amb)}
.srb{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}.srb .btn{padding:8px 12px;font-size:13px}
@media(max-width:800px){.sr{flex-direction:column;align-items:stretch}.srb{justify-content:flex-start}}
</style>
</head>
<body>
<header>
 <div class="brand"><img src="/logo.jpg?v=wilma" alt="Radio Wilma"><div><b style="font-family:Georgia,'Times New Roman',serif;letter-spacing:2px;color:#e2b45c">RADIO WILMA</b><span>radio internetowe</span></div></div>
 <div class="stat"><span id="hIp"><span class="dot"></span>-</span><span id="hWx"></span></div>
 <nav>
  <button data-v="play" class="on"><svg viewBox="0 0 24 24" fill="currentColor"><path d="M8 5v14l11-7z"/></svg><span>Odtwarzacz</span></button>
  <button data-v="set"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></svg><span>Ustawienia</span></button>
 </nav>
</header>

<main>
<section id="play" class="view">
 <div class="card player">
  <canvas id="pLogo" width="176" height="132"></canvas>
  <div class="lbl" id="pNum">STACJA RADIOWA</div>
  <h2 id="pSt">-</h2>
  <div class="desc" id="pDesc"></div>
  <div class="track"><div class="lbl" id="pState">AKTUALNIE GRA</div><div class="a" id="pAr"></div><div class="t" id="pTi"></div></div>
  <div class="chips" id="pChips"></div>
  <div class="ctrl">
   <button class="rb" id="bMute" title="Wycisz"></button>
   <button class="rb" title="Poprzednia" onclick="post('/api/step?d=-1')"></button>
   <button class="rb big" id="bPause" title="Pauza / graj" onclick="post('/api/pause')"></button>
   <button class="rb" title="Następna" onclick="post('/api/step?d=1')"></button>
   <div class="rb num" id="bNum">-</div>
  </div>
  <div class="vol"><span id="vIc"></span><input type="range" id="vol" min="0" max="21" value="0"><b id="vTx">-</b></div>
 </div>
 <div class="card">
  <div class="lh"><h3>LISTA STACJI</h3><span class="badge" id="lCount">0</span>
   <div class="search"><span id="sIc"></span><input id="q" placeholder="Szukaj stacji..." autocomplete="off"></div></div>
  <div class="list" id="list"></div>
  <div class="hint">Kliknij stację, aby ją włączyć. Przeciągnij obrazek na stację, aby ustawić jej logo.</div>
 </div>
</section>

<section id="set" class="view" hidden>
 <div class="tiles" id="tiles"></div>
 <div class="card" id="pane"></div>
</section>
</main>

<footer>
 <span>Radio internetowe ESP32-S3 &middot; wersja <span id="fVer">-</span></span>
 <span>&copy; 2026 <b>Sir Williams</b></span>
</footer>

<dialog id="dlg">
 <h3 id="dTitle">Stacja</h3>
 <label>Nazwa</label><input class="in" id="fName" maxlength="63">
 <label>Adres strumienia (http:// lub https://, także .pls / .m3u)</label><input class="in" id="fUrl" maxlength="255">
 <label>Opis (pod nazwą, gdy stacja nie wysyła własnego)</label><input class="in" id="fDesc" maxlength="95">
 <label>Logo (pole 88&times;66 &ndash; tak wygląda na ekranie radia)</label>
 <div class="logoedit"><canvas id="fLogo" width="88" height="66"></canvas>
  <div class="opts"><input type="file" id="fFile" accept="image/*" hidden onchange="pickLogo(this.files[0])">
   <div class="k">Dopasowanie</div><div class="seg" id="segMode"><button data-v="fit">Dopasuj</button><button data-v="fill">Wypełnij</button><button data-v="stretch">Rozciągnij</button></div>
   <div class="k">Tło</div><div class="seg" id="segBg"><button data-v="auto">Auto</button><button data-v="black">Czarne</button><button data-v="gray">Ciemnoszare</button></div>
   <div><button class="btn g" onclick="$('fFile').click()">Wybierz obraz</button> <button class="btn d" onclick="dropLogo()">Usuń logo</button></div></div></div>
 <div class="btns" style="justify-content:flex-end"><button class="btn g" onclick="$('dlg').close()">Anuluj</button><button class="btn" onclick="saveStation()">Zapisz</button></div>
</dialog>
<dialog id="bdlg">
 <h3>Wgrywanie logo</h3>
 <div class="hint" style="margin-top:0">Sprawdź przypisanie plików do stacji. Logo zostaną dopasowane do pola 88&times;66 bez obcinania (tło auto).</div>
 <div class="bulk" id="bulkRows"></div>
 <div class="btns" style="justify-content:flex-end"><button class="btn g" onclick="$('bdlg').close()">Anuluj</button><button class="btn" id="bulkGo" onclick="bulkSave()">Zapisz logo</button></div>
</dialog>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const LW=88,LH=66;
const SV=(p,f)=>'<svg viewBox="0 0 24 24" '+(f?'fill="currentColor"':'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"')+'>'+p+'</svg>';
const I={
 prev:SV('<path d="M6 6h2v12H6zM9.5 12 18 6v12z"/>',1),next:SV('<path d="M16 6h2v12h-2zM6 18l8.5-6L6 6z"/>',1),
 play:SV('<path d="M8 5v14l11-7z"/>',1),pause:SV('<path d="M6 5h4v14H6zM14 5h4v14h-4z"/>',1),
 vol:SV('<path d="M11 5 6 9H2v6h4l5 4z" fill="currentColor"/><path d="M15.5 8.5a5 5 0 0 1 0 7M19 5a10 10 0 0 1 0 14"/>'),
 muted:SV('<path d="M11 5 6 9H2v6h4l5 4z" fill="currentColor"/><path d="m16 9 6 6M22 9l-6 6"/>'),
 search:SV('<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>'),
 wifi:SV('<path d="M2 8.8a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16.1a5 5 0 0 1 7 0"/><circle cx="12" cy="19.5" r="1.3" fill="currentColor"/>'),
 list:SV('<path d="M8 6h13M8 12h13M8 18h13"/><circle cx="3.5" cy="6" r="1.2" fill="currentColor"/><circle cx="3.5" cy="12" r="1.2" fill="currentColor"/><circle cx="3.5" cy="18" r="1.2" fill="currentColor"/>'),
 sun:SV('<circle cx="9" cy="9" r="3.2"/><path d="M9 2v1.5M2 9h1.5M4 4l1 1M14 4l-1 1"/><path d="M8 20h9a4 4 0 0 0 .5-8 5 5 0 0 0-9.5 1.5A3.3 3.3 0 0 0 8 20z" fill="currentColor" stroke="none"/>'),
 lock:SV('<rect x="4" y="10" width="16" height="11" rx="2.5"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>'),
 up:SV('<path d="M12 16V4M6 10l6-6 6 6M4 20h16"/>'),
 chip:SV('<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>'),
 bars:SV('<rect x="3" y="12" width="3" height="9"/><rect x="9" y="6" width="3" height="15"/><rect x="15" y="9" width="3" height="12"/>',1),
 tag:SV('<path d="M3 12V4h8l10 10-8 8z"/><circle cx="7.5" cy="8.5" r="1.3" fill="currentColor"/>'),
 edit:SV('<path d="M4 20h4L19 9l-4-4L4 16z"/><path d="m13.5 6.5 4 4"/>'),
 trash:SV('<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>'),
 upA:SV('<path d="m6 15 6-6 6 6"/>'),downA:SV('<path d="m6 9 6 6 6-6"/>'),
 plus:SV('<path d="M12 5v14M5 12h14"/>'),dl:SV('<path d="M12 4v12M6 10l6 6 6-6M4 20h16"/>'),
 restart:SV('<path d="M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7"/>'),
 palette:SV('<path d="M12 3a9 9 0 1 0 0 18c1.1 0 1.8-.9 1.8-1.9 0-.5-.2-.9-.5-1.3-.3-.3-.5-.8-.5-1.2 0-1 .8-1.8 1.8-1.8H17a4 4 0 0 0 4-4c0-4.3-4-7.8-9-7.8z"/><circle cx="7.5" cy="11" r="1.2" fill="currentColor"/><circle cx="10" cy="7.3" r="1.2" fill="currentColor"/><circle cx="14.5" cy="7.3" r="1.2" fill="currentColor"/>')};

let stations=[],status={},view='play',pane='wifi',settings=null,volTimer=null,volBusy=0,editIdx=-1,logoBytes=null,logoDrop=false;
const logoCache=new Map();

function toast(m,err){const t=$('toast');t.textContent=m;t.className=err?'err':'';t.style.display='block';clearTimeout(t._h);t._h=setTimeout(()=>t.style.display='none',err?6000:2500);}
async function api(path,opt){const r=await fetch(path,opt);let j=null;try{j=await r.json();}catch(e){}
 if(!r.ok||(j&&j.ok===false))throw new Error(j&&j.error?j.error:('HTTP '+r.status));return j;}
async function post(path,body){try{const j=await api(path,{method:'POST',body});setTimeout(poll,250);return j;}catch(e){toast(e.message,1);}}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}

// ---------- RGB565 logos ----------
function rawToCanvas(buf){const c=document.createElement('canvas');c.width=LW;c.height=LH;const x=c.getContext('2d'),img=x.createImageData(LW,LH),px=new Uint16Array(buf);
 for(let i=0;i<px.length;i++){const v=px[i];img.data[i*4]=(v>>11&31)*255/31;img.data[i*4+1]=(v>>5&63)*255/63;img.data[i*4+2]=(v&31)*255/31;img.data[i*4+3]=255;}x.putImageData(img,0,0);return c;}
function paint(canvas,src){const c=canvas.getContext('2d');c.imageSmoothingQuality='high';c.clearRect(0,0,canvas.width,canvas.height);
 if(src){c.drawImage(src,0,0,canvas.width,canvas.height);return;}
 c.fillStyle='#101014';c.fillRect(0,0,canvas.width,canvas.height);const w=canvas.width,h=canvas.height;
 c.strokeStyle='#ffb300';c.lineWidth=Math.max(1.5,w/40);c.lineCap='round';c.beginPath();c.roundRect(w*.25,h*.4,w*.5,h*.38,w*.04);c.stroke();
 c.beginPath();c.moveTo(w*.33,h*.38);c.lineTo(w*.6,h*.2);c.stroke();c.beginPath();c.arc(w*.37,h*.59,h*.09,0,7);c.stroke();
 for(const y of[.52,.6,.68]){c.beginPath();c.moveTo(w*.52,h*y);c.lineTo(w*.66,h*y);c.stroke();}}
async function getLogo(id,has){if(!has)return null;if(logoCache.has(id))return logoCache.get(id);
 try{const r=await fetch('/api/logo?id='+id+'&t='+Date.now());if(!r.ok)throw 0;const c=rawToCanvas(await r.arrayBuffer());logoCache.set(id,c);return c;}catch(e){return null;}}
async function showLogo(canvas,id,has){paint(canvas,null);const c=await getLogo(id,has);if(c)paint(canvas,c);}
// Converts any image to the 88x66 RGB565 logo used by the radio.
// mode: fit (whole logo visible, never enlarged), fill (cover the field, crop), stretch.
// bg: auto (colour taken from the logo edges), black, gray.
let logoOpts={mode:'fit',bg:'auto'};
try{const o=JSON.parse(localStorage.getItem('logoOpts')||'null');if(o&&o.mode&&o.bg)logoOpts=o;}catch(e){}
function saveLogoOpts(){try{localStorage.setItem('logoOpts',JSON.stringify(logoOpts));}catch(e){}}
function edgeColor(img){
 const t=document.createElement('canvas'),w=Math.max(1,Math.min(img.width,200)),h=Math.max(1,Math.round(img.height*w/img.width));
 t.width=w;t.height=h;const c=t.getContext('2d');c.drawImage(img,0,0,w,h);const d=c.getImageData(0,0,w,h).data,count={};let best=null,bn=0,transparent=0,total=0;
 const add=(x,y)=>{const i=(y*w+x)*4;total++;if(d[i+3]<128){transparent++;return;}const k=(d[i]>>4)+','+(d[i+1]>>4)+','+(d[i+2]>>4);
  const e=count[k]||(count[k]={n:0,r:0,g:0,b:0});e.n++;e.r+=d[i];e.g+=d[i+1];e.b+=d[i+2];if(e.n>bn){bn=e.n;best=e;}};
 for(let x=0;x<w;x++){add(x,0);add(x,h-1);}for(let y=1;y<h-1;y++){add(0,y);add(w-1,y);}
 if(!best||transparent>total/2)return'#000';return'rgb('+Math.round(best.r/best.n)+','+Math.round(best.g/best.n)+','+Math.round(best.b/best.n)+')';}
function imageToLogo(img,opts){opts=opts||logoOpts;const cv=document.createElement('canvas');cv.width=LW;cv.height=LH;const c=cv.getContext('2d');
 c.fillStyle='#000';c.fillRect(0,0,LW,LH);c.save();c.beginPath();c.roundRect(0,0,LW,LH,6);c.clip();
 c.fillStyle=opts.bg==='auto'?edgeColor(img):(opts.bg==='gray'?'#16161a':'#000');c.fillRect(0,0,LW,LH);
 let w=img.width,h=img.height;
 if(opts.mode==='fill'){const s=Math.max(LW/w,LH/h);w*=s;h*=s;}
 else if(opts.mode==='stretch'){w=LW;h=LH;}
 else{const s=Math.min(LW/w,LH/h,1);w=Math.round(w*s);h=Math.round(h*s);}
 c.imageSmoothingEnabled=!(opts.mode==='fit'&&w===img.width&&h===img.height);c.imageSmoothingQuality='high';
 c.drawImage(img,Math.round((LW-w)/2),Math.round((LH-h)/2),w,h);c.restore();
 c.strokeStyle='rgb(70,76,86)';c.lineWidth=1.3;c.beginPath();c.roundRect(.65,.65,LW-1.3,LH-1.3,5.4);c.stroke();
 const d=c.getImageData(0,0,LW,LH).data,out=new Uint8Array(LW*LH*2);
 for(let i=0;i<LW*LH;i++){const v=((d[i*4]&0xF8)<<8)|((d[i*4+1]&0xFC)<<3)|(d[i*4+2]>>3);out[i*2]=v&255;out[i*2+1]=v>>8;}return out;}
function loadImage(file){return new Promise((ok,bad)=>{const img=new Image();img.onload=()=>{URL.revokeObjectURL(img.src);ok(img);};img.onerror=()=>bad(new Error('Nie można odczytać obrazu'));img.src=URL.createObjectURL(file);});}
// ---------- weather icons (RGB565 78x60, composed on black like on the screen) ----------
const WXK=['Bezchmurnie (dzień)','Bezchmurnie (noc)','Małe zachmurzenie (dzień)','Małe zachmurzenie (noc)','Pochmurno','Mgła','Mżawka','Deszcz','Śnieg','Burza'],WIW=78,WIH=60;
function raw565ToCanvas(buf,w,h){const c=document.createElement('canvas');c.width=w;c.height=h;const x=c.getContext('2d'),img=x.createImageData(w,h),px=new Uint16Array(buf);
 for(let i=0;i<w*h&&i<px.length;i++){const v=px[i];img.data[i*4]=(v>>11&31)*255/31;img.data[i*4+1]=(v>>5&63)*255/63;img.data[i*4+2]=(v&31)*255/31;img.data[i*4+3]=255;}x.putImageData(img,0,0);return c;}
function imageToWxIcon(img){const cv=document.createElement('canvas');cv.width=WIW;cv.height=WIH;const c=cv.getContext('2d');c.fillStyle='#000';c.fillRect(0,0,WIW,WIH);
 const s=Math.min(WIW/img.width,WIH/img.height),w=Math.round(img.width*s),h=Math.round(img.height*s);c.imageSmoothingQuality='high';
 c.drawImage(img,Math.round((WIW-w)/2),Math.round((WIH-h)/2),w,h);const d=c.getImageData(0,0,WIW,WIH).data,out=new Uint8Array(WIW*WIH*2);
 for(let i=0;i<WIW*WIH;i++){const v=((d[i*4]&0xF8)<<8)|((d[i*4+1]&0xFC)<<3)|(d[i*4+2]>>3);out[i*2]=v&255;out[i*2+1]=v>>8;}return out;}
async function loadWxIcons(){const g=$('wxIcons');if(!g)return;
 g.innerHTML=WXK.map((n,k)=>'<div class="wxi"><canvas id="wxc'+k+'" width="'+WIW+'" height="'+WIH+'"></canvas><b>'+n+'</b><span id="wxs'+k+'">&nbsp;</span><div class="btns"><button class="btn g" onclick="pickWxIcon('+k+')">Zmień</button><button class="btn g" id="wxd'+k+'" onclick="resetWxIcon('+k+')" style="display:none">Przywróć</button></div></div>').join('');
 for(let k=0;k<WXK.length;k++){try{const r=await fetch('/api/wxicon?kind='+k+'&t='+Date.now());if(!r.ok)throw 0;const own=r.headers.get('X-Own-Icon')==='1',buf=await r.arrayBuffer(),c=$('wxc'+k);if(!c)return;
  c.getContext('2d').drawImage(raw565ToCanvas(buf,WIW,WIH),0,0);$('wxs'+k).textContent=own?'własna':'wbudowana';$('wxd'+k).style.display=own?'':'none';}catch(e){}}}
function pickWxIcon(k){const i=document.createElement('input');i.type='file';i.accept='image/*';
 i.onchange=async()=>{const f=i.files[0];if(!f)return;try{const img=await loadImage(f),fd=new FormData();fd.append('file',new Blob([imageToWxIcon(img)],{type:'application/octet-stream'}),'icon.565');
  await api('/api/wxicon?kind='+k,{method:'POST',body:fd});toast('Ikona zapisana');loadWxIcons();}catch(e){toast(e.message,1);}};i.click();}
async function resetWxIcon(k){try{await api('/api/wxicon/delete?kind='+k,{method:'POST'});toast('Przywrócono wbudowaną ikonę');loadWxIcons();}catch(e){toast(e.message,1);}}
async function uploadLogo(id,bytes){const fd=new FormData();fd.append('file',new Blob([bytes],{type:'application/octet-stream'}),'logo.565');
 await api('/api/logo?id='+id,{method:'POST',body:fd});logoCache.delete(id);}

// ---------- views ----------
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>setView(b.dataset.v));
function setView(v){view=v;document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x.dataset.v===v));
 $('play').hidden=v!=='play';$('set').hidden=v!=='set';if(v==='set'){loadSettings();showPane(pane);}else loadStations();}

// ---------- player ----------
$('bMute').onclick=()=>post('/api/mute');$('sIc').innerHTML=I.search;
document.querySelectorAll('.ctrl .rb')[1].innerHTML=I.prev;document.querySelectorAll('.ctrl .rb')[3].innerHTML=I.next;
function setRange(r){r.style.setProperty('--p',(r.value-r.min)*100/(r.max-r.min)+'%');}
$('vol').oninput=e=>{volBusy=Date.now();setRange(e.target);$('vTx').textContent=e.target.value;clearTimeout(volTimer);
 volTimer=setTimeout(async()=>{await post('/api/volume?v='+e.target.value);volBusy=0;},150);};
function wxText(w){return w&&w.valid?Math.round(w.temp)+'&deg;C '+esc(w.place):'';}
let lastLogoKey='',lastListKey='';
async function poll(){
 try{status=await api('/api/status');}catch(e){$('hIp').innerHTML='<span class="dot"></span>brak połączenia';return;}
 const s=status;
 $('hIp').innerHTML='<span class="dot'+(s.wifi.connected?' on':'')+'"></span>'+esc(s.wifi.ip);$('hWx').innerHTML=wxText(s.weather);
 $('fVer').textContent=s.version||'-';
 $('pNum').textContent=s.count?'STACJA RADIOWA':'BRAK STACJI';$('pSt').textContent=s.station||'-';$('pDesc').textContent=s.desc||'';
 $('pState').textContent=s.paused?'PAUZA':'AKTUALNIE GRA';$('pAr').textContent=s.artist||'';$('pTi').textContent=s.title||'';
 const chips=[];if(s.kbps)chips.push(I.bars+s.kbps+' kbps');if(s.genre)chips.push(I.tag+esc(s.genre));if(s.weather&&s.weather.valid)chips.push(I.sun+wxText(s.weather));
 $('pChips').innerHTML=chips.map(c=>'<span class="chip">'+c+'</span>').join('');
 $('bPause').innerHTML=s.paused?I.play:I.pause;$('bMute').innerHTML=s.muted?I.muted:I.vol;$('bMute').classList.toggle('on',s.muted);
 $('bNum').textContent=s.count?(s.index+1)+'/'+s.count:'-';$('vIc').innerHTML=s.muted?I.muted:I.vol;
 const v=$('vol');v.max=s.maxVolume;if(!volBusy)v.value=s.volume;setRange(v);$('vTx').textContent=s.muted?'MUTE':v.value;
 const lk=s.id+'/'+s.logoRev+'/'+s.logo;if(lk!==lastLogoKey){lastLogoKey=lk;logoCache.delete(s.id);showLogo($('pLogo'),s.id,s.logo);}
 const k=s.id+'/'+s.paused+'/'+s.count;if(k!==lastListKey){lastListKey=k;if(view==='play')renderList();}
}

// ---------- station list (player) ----------
async function loadStations(){try{stations=await api('/api/stations');}catch(e){toast(e.message,1);return;}renderList();if(view==='set'&&pane==='stations')renderManage();}
$('q').oninput=renderList;
function renderList(){
 const q=$('q').value.trim().toLowerCase();$('lCount').textContent=stations.length;
 const el=$('list');
 if(!stations.length){el.innerHTML='<p class="hint">Lista jest pusta. Dodaj stacje w Ustawienia &rarr; Stacje.</p>';return;}
 el.innerHTML=stations.map((s,i)=>(q&&!(s.name+' '+(s.desc||'')).toLowerCase().includes(q))?'':
  '<div class="it'+(s.id===status.id?' cur':'')+'" data-i="'+i+'"><span class="no">'+(i+1)+'</span><canvas width="88" height="66" id="lc'+i+'"></canvas>'+
  '<div class="tx"><div class="nm">'+esc(s.name)+'</div><div class="ds">'+esc(s.desc||s.url)+'</div></div>'+
  (s.id===status.id?'<div class="eqi'+(status.paused?' stop':'')+'"><s></s><s></s><s></s><s></s></div>':'<div class="pl">'+I.play+'</div>')+'</div>').join('');
 el.querySelectorAll('.it').forEach(row=>{const i=+row.dataset.i,s=stations[i];
  showLogo($('lc'+i),s.id,s.logo);
  row.onclick=()=>post('/api/play?i='+i);
  row.ondragover=e=>{e.preventDefault();row.classList.add('drop');};
  row.ondragleave=()=>row.classList.remove('drop');
  row.ondrop=async e=>{e.preventDefault();row.classList.remove('drop');const f=e.dataTransfer.files[0];if(!f)return;
   try{const bytes=imageToLogo(await loadImage(f),{mode:'fit',bg:'auto'});await uploadLogo(s.id,bytes);s.logo=true;toast('Logo zapisane: '+s.name);renderList();lastLogoKey='';poll();}catch(err){toast(err.message,1);}};});
}

// ---------- settings ----------
const TILES=[['wifi','wifi','Wi-Fi'],['stations','list','Stacje'],['search','search','Szukaj stacji'],['weather','sun','Pogoda'],['password','lock','Hasło panelu'],['update','up','Aktualizacja'],['look','palette','Wygląd'],['system','chip','System']];
function tileInfo(id){const s=settings;if(!s)return'';
 switch(id){case'wifi':return s.connected?'Połączono: '+esc(s.ssid)+' &middot; '+s.rssi+' dBm':(s.apActive?'Tryb konfiguracji ('+esc(s.apSsid)+')':'Brak połączenia');
  case'stations':return stations.length+' stacji &middot; edycja, logo, import';
  case'search':return'Nowe stacje z internetu';
  case'weather':{const w=s.weather||{};return!w.query?'Wyłączona':(w.valid?esc(w.place)+' &middot; '+Math.round(w.temp)+'&deg;C':esc(w.query));}
  case'password':return s.panelPassword?'Ustawione':'Nie ustawiono';case'update':return'Wersja '+esc(s.version)+' &middot; plik .bin';case'system':return'Pamięć, restart';case'look':return'Kolory napisów na ekranie radia';}}
function renderTiles(){$('tiles').innerHTML=TILES.map(t=>'<button class="tile'+(t[0]===pane?' on':'')+'" data-p="'+t[0]+'"><div class="ic">'+I[t[1]]+'</div><div><b>'+t[2]+'</b><span>'+tileInfo(t[0])+'</span></div></button>').join('');
 document.querySelectorAll('.tile').forEach(b=>b.onclick=()=>showPane(b.dataset.p));}
async function loadSettings(){try{settings=await api('/api/settings');}catch(e){toast(e.message,1);return;}
 if(!stations.length){try{stations=await api('/api/stations');}catch(e){}}renderTiles();refreshPane();}
function bars(rssi){const n=rssi>=-55?4:rssi>=-65?3:rssi>=-75?2:rssi>=-85?1:0;return'<div class="sg">'+[6,9,12,15].map((h,k)=>'<i class="'+(k<n?'on':'')+'" style="height:'+h+'px"></i>').join('')+'</div>';}
function pct(rssi){return Math.max(5,Math.min(100,(rssi+95)*100/55));}
function showPane(p){pane=p;renderTiles();const el=$('pane');const s=settings||{};
 if(p==='wifi'){el.innerHTML='<div class="lh"><h3>WI-FI</h3></div><div class="two"><div><label>Nazwa sieci (SSID)</label><input class="in" id="ssid" maxlength="32" autocomplete="off">'+
  '<label>Hasło</label><input class="in" type="password" id="wpass" maxlength="63"><div class="btns"><button class="btn" onclick="saveWifi()">Zapisz i połącz</button>'+
  '<button class="btn g" onclick="scan(true)">'+I.search+'Szukaj sieci</button></div><div class="gauges" id="wG"></div></div>'+
  '<div><label>Dostępne sieci</label><div id="nets"><p class="hint">Kliknij &bdquo;Szukaj sieci&rdquo;.</p></div></div></div>';
  $('ssid').value=s.ssid||'';refreshPane();}
 else if(p==='stations'){el.innerHTML='<div class="lh"><h3>ZARZĄDZANIE STACJAMI</h3><span class="badge" id="mCount"></span></div><div id="mlist"></div>'+
  '<div class="btns"><button class="btn" onclick="editStation(-1)">'+I.plus+'Dodaj stację</button><button class="btn g" onclick="exportList()">'+I.dl+'Eksport do pliku</button>'+
  '<button class="btn g" onclick="$(\'imp\').click()">'+I.up+'Import z pliku</button><input type="file" id="imp" accept=".json,application/json" hidden onchange="importList(this)"></div>'+
  '<div class="drop" id="bulkDrop" style="margin-top:16px">'+I.up+'<b>Upuść tutaj logo kilku stacji naraz</b> albo kliknij, aby wybrać pliki<br>'+
  '<span class="hint">Pliki zostaną przypisane do stacji po nazwie (np. rmf_fm.png &rarr; RMF FM). Przed zapisem możesz to poprawić.</span></div>'+
  '<input type="file" id="bulkFiles" accept="image/*" multiple hidden>'+
  '<p class="hint">Eksport zawiera nazwy, adresy i opisy (bez logo). Import zastępuje całą listę.</p>';
  const bd=$('bulkDrop');bd.onclick=()=>$('bulkFiles').click();$('bulkFiles').onchange=e=>{bulkOpen([...e.target.files]);e.target.value='';};
  bd.ondragover=e=>{e.preventDefault();bd.classList.add('over');};bd.ondragleave=()=>bd.classList.remove('over');
  bd.ondrop=e=>{e.preventDefault();bd.classList.remove('over');bulkOpen([...e.dataTransfer.files].filter(f=>f.type.startsWith('image/')));};
  loadStations();}
 else if(p==='search'){el.innerHTML='<div class="lh"><h3>SZUKAJ STACJI</h3></div>'+
  '<div class="sbar"><input class="in" id="sq" maxlength="60" placeholder="Nazwa stacji, np. jazz, BBC, Trójka" enterkeyhint="search"><button class="btn" onclick="doSearch()">'+I.search+'Szukaj</button></div>'+
  '<p class="hint">Baza radio-browser.info (społecznościowa, bezpłatna), do 25 najpopularniejszych wyników. Stacja z wyszukiwania gra bez zmiany listy ulubionych. Nie każdy strumień musi działać.</p><div id="sres"></div>';
  $('sq').onkeydown=e=>{if(e.key==='Enter')doSearch();};pollSearch();}
 else if(p==='weather'){el.innerHTML='<div class="lh"><h3>POGODA</h3></div><div class="two"><div><label>Miejscowość (puste = bez pogody na ekranie)</label>'+
  '<input class="in" id="wxPlace" maxlength="60" placeholder="np. Kraków"><div class="btns"><button class="btn" onclick="saveWeather()">Zapisz</button></div>'+
  '<p class="hint">Dane: Open-Meteo.com (bezpłatne do użytku niekomercyjnego), odświeżane co 15 minut.</p></div><div id="wxCard"></div></div>'+
  '<div class="lh" style="margin-top:24px"><h3>IKONY POGODY</h3></div><p class="hint" style="margin-top:0">Wybierz obraz (najlepiej PNG z przezroczystym tłem) &ndash; zostanie dopasowany do pola 78&times;60 na czarnym tle i od razu pojawi się na ekranie radia.</p><div class="wxg" id="wxIcons"></div>';
  $('wxPlace').value=(s.weather||{}).query||'';refreshPane();loadWxIcons();}
 else if(p==='password'){el.innerHTML='<div class="lh"><h3>HASŁO PANELU</h3></div><div class="two"><div><label>Nowe hasło (użytkownik: admin)</label>'+
  '<input class="in" type="password" id="ppass" maxlength="32"><div class="btns"><button class="btn" onclick="savePanelPass()">Zapisz hasło</button>'+
  '<button class="btn g" onclick="$(\'ppass\').value=\'\';savePanelPass()">Usuń hasło</button></div></div><div class="gauges"><div class="g1"><span>Stan</span><b id="pwState"></b>'+
  '<span>Bez hasła każdy w Twojej sieci może sterować radiem.</span></div></div></div>';refreshPane();}
 else if(p==='update'){el.innerHTML='<div class="lh"><h3>AKTUALIZACJA OPROGRAMOWANIA</h3></div><div class="drop" id="fwDrop">'+I.up+
  '<b>Upuść plik .bin</b> albo kliknij, aby wybrać<br><span class="hint">Arduino IDE: Szkic &rarr; Eksportuj skompilowany plik binarny</span></div>'+
  '<input type="file" id="fw" accept=".bin" hidden><progress id="fwProg" max="100" value="0" hidden></progress><p class="hint" id="fwState">Obecna wersja: '+esc(s.version||'-')+'</p>';
  const d=$('fwDrop');d.onclick=()=>$('fw').click();$('fw').onchange=()=>uploadFw($('fw').files[0]);
  d.ondragover=e=>{e.preventDefault();d.classList.add('over');};d.ondragleave=()=>d.classList.remove('over');
  d.ondrop=e=>{e.preventDefault();d.classList.remove('over');uploadFw(e.dataTransfer.files[0]);};}
 else if(p==='look'){el.innerHTML='<div class="lh"><h3>WYGLĄD EKRANU RADIA</h3></div>'+
  '<div class="k" style="font-size:11px;color:var(--dim);letter-spacing:1px;margin-bottom:8px">GOTOWE MOTYWY</div><div class="btns" style="margin-top:0" id="themes"></div>'+
  '<div class="cgrid" id="cgrid"></div><div class="btns"><button class="btn g" onclick="resetColors()">'+I.restart+'Przywróć domyślne</button>'+
  '<a class="btn g" href="/ekran" target="_blank" style="text-decoration:none">Podgląd ekranu</a></div>'+
  '<p class="hint">Zmiany od razu widać na ekranie radia i są zapamiętywane. Logo stacji i ikony pogody to obrazy &ndash; ich kolory się nie zmieniają.</p>';loadColors();}
 else if(p==='system'){el.innerHTML='<div class="lh"><h3>SYSTEM</h3></div><div class="gauges" id="sysG"></div><div class="btns">'+
  '<button class="btn d" onclick="if(confirm(\'Uruchomić radio ponownie?\'))post(\'/api/restart\')">'+I.restart+'Restart radia</button></div>';refreshPane();}}
function refreshPane(){const s=settings;if(!s)return;
 const fill=(id,v)=>{const e=$(id);if(e&&!e.value&&document.activeElement!==e)e.value=v||'';};
 fill('ssid',s.ssid);fill('wxPlace',(s.weather||{}).query);
 if(pane==='wifi'&&$('wG'))$('wG').innerHTML=(s.connected?'<div class="g1"><span>Sygnał</span><b>'+s.rssi+' dBm</b><div class="pb"><i style="width:'+pct(s.rssi)+'%"></i></div></div>'+
  '<div class="g1"><span>Adres IP</span><b>'+esc(s.ip)+'</b><span>'+esc(s.hostname)+'.local</span></div>':'<div class="g1"><span>Stan</span><b>Brak połączenia</b><span>'+esc(s.ssid||'')+'</span></div>')+
  (s.apActive?'<div class="g1"><span>Punkt konfiguracji</span><b>'+esc(s.apSsid)+'</b><span>'+esc(s.apIp)+'</span></div>':'');
 if(pane==='weather'&&$('wxCard')){const w=s.weather||{};$('wxCard').innerHTML=!w.query?'<div class="g1"><span>Stan</span><b>Wyłączona</b></div>':
  (w.valid?'<div class="wxbig">'+I.sun+'<div><b>'+w.temp.toFixed(1)+'&deg;C</b><div>'+esc(w.place)+'</div><div class="hint" style="margin:0">aktualizacja '+(w.ageMin<1?'przed chwilą':w.ageMin+' min temu')+'</div></div></div>'
  :'<div class="g1"><span>'+esc(w.query)+'</span><b style="color:'+(w.error?'var(--red)':'var(--amb)')+';white-space:normal;font-size:15px">'+esc(w.error||'Pobieranie danych...')+'</b></div>');}
 if(pane==='password'&&$('pwState'))$('pwState').textContent=s.panelPassword?'Hasło ustawione':'Bez hasła';
 if(pane==='system'&&$('sysG')){const fsP=s.fsTotal?s.fsUsed*100/s.fsTotal:0;
  $('sysG').innerHTML='<div class="g1"><span>Adres panelu</span><b>'+esc(s.hostname)+'.local</b><span>'+esc(s.ip||s.apIp)+'</span></div>'+
  '<div class="g1"><span>Podgląd ekranu (np. do OBS)</span><b><a href="/ekran" target="_blank" style="color:var(--amb)">/ekran</a></b><span>parametry: ?clean ukrywa licznik, ?ms=200 odstęp klatek</span></div>'+
  '<div class="g1"><span>Wersja</span><b>'+esc(s.version)+'</b><span>ESP32-S3 &middot; ST7796</span></div>'+
  '<div class="g1"><span>Wolna pamięć RAM</span><b>'+Math.round(s.heap/1024)+' KB</b><span>PSRAM: '+Math.round(s.psram/1024)+' KB</span></div>'+
  '<div class="g1"><span>Pamięć plików</span><b>'+(s.fsTotal?Math.round(s.fsUsed/1024)+' / '+Math.round(s.fsTotal/1024)+' KB':'niedostępna')+'</b>'+
  (s.fsTotal?'<div class="pb"><i style="width:'+Math.max(2,fsP)+'%"></i></div>':'')+'<span>'+esc(s.fsInfo)+'</span></div>';}}

async function scan(start){try{const r=await api('/api/scan'+(start?'?start=1':''));
 if(r.running){$('nets').innerHTML='<p class="hint">Szukam sieci...</p>';setTimeout(()=>scan(false),1500);return;}
 const seen={},nets=r.networks.sort((a,b)=>b.rssi-a.rssi).filter(n=>!seen[n.ssid]&&(seen[n.ssid]=1)),cur=settings&&settings.connected?settings.ssid:'';
 $('nets').innerHTML=nets.map(n=>'<div class="net'+(n.ssid===cur?' cur':'')+'" data-s="'+esc(n.ssid)+'">'+I.wifi+'<div>'+esc(n.ssid)+(n.ssid===cur?' <small>&middot; połączono</small>':'')+(n.open?' <small>&middot; otwarta</small>':'')+'</div>'+bars(n.rssi)+'</div>').join('')||'<p class="hint">Nie znaleziono sieci.</p>';
 document.querySelectorAll('.net').forEach(e=>e.onclick=()=>{$('ssid').value=e.dataset.s;$('wpass').focus();});}catch(e){toast(e.message,1);}}
async function saveWifi(){const b=new URLSearchParams({ssid:$('ssid').value.trim(),pass:$('wpass').value});
 try{await api('/api/wifi',{method:'POST',body:b});toast('Zapisano. Radio łączy się z nową siecią - adres IP pokaże się na ekranie.');}catch(e){toast(e.message,1);}}
// ---------- internet station search ----------
let searchTimer=null;
async function doSearch(){const q=$('sq').value.trim();if(!q){toast('Wpisz nazwę stacji',1);return;}
 try{await api('/api/search',{method:'POST',body:new URLSearchParams({q})});$('sres').innerHTML='<p class="hint">Szukam &bdquo;'+esc(q)+'&rdquo;...</p>';searchTimer=setTimeout(pollSearch,600);}catch(e){toast(e.message,1);}}
async function pollSearch(){clearTimeout(searchTimer);if(pane!=='search'||!$('sres'))return;
 try{const r=await api('/api/search');if(r.state==='busy'){if(!$('sres').innerHTML)$('sres').innerHTML='<p class="hint">Szukam...</p>';searchTimer=setTimeout(pollSearch,700);return;}renderSearch(r);}
 catch(e){$('sres').innerHTML='<p class="hint" style="color:var(--red)">'+esc(e.message)+'</p>';}}
function renderSearch(r){const el=$('sres');if(!el)return;if(r.state==='idle'){el.innerHTML='';return;}
 if(!$('sq').value)$('sq').value=r.query||'';
 if(r.state==='error'||!r.results.length){el.innerHTML='<div class="g1" style="margin-top:10px"><span>'+esc(r.query)+'</span><b style="color:var(--red);white-space:normal;font-size:15px">'+esc(r.error||'Brak wyników')+'</b></div>';return;}
 el.innerHTML='<p class="hint">Wyniki dla &bdquo;'+esc(r.query)+'&rdquo;: '+r.results.length+'</p><div class="srl">'+r.results.map(x=>{
  const meta=[x.country,x.language,x.tags].filter(Boolean).map(esc).join(' &middot; '),fmt=esc(x.codec||'?')+(x.bitrate?' &middot; '+x.bitrate+' kbps':'');
  return'<div class="sr'+(x.playable?'':' off')+'"><div class="sri"><b>'+esc(x.name)+'</b><span>'+(meta||'brak informacji o kraju i gatunku')+'</span><small>'+fmt+(x.playable?'':' &middot; '+esc(x.reason))+'</small></div>'+
  (x.playable?'<div class="srb"><button class="btn" onclick="searchPlay('+x.i+')">'+I.play+'Graj</button><button class="btn g" onclick="searchAdd('+x.i+')">'+I.plus+'Do ulubionych</button></div>':'')+'</div>';}).join('')+'</div>';}
async function searchPlay(i){try{await api('/api/search/play?i='+i,{method:'POST'});toast('Łączenie ze stacją...');setTimeout(poll,1500);}catch(e){toast(e.message,1);}}
async function searchAdd(i){try{await api('/api/search/add?i='+i,{method:'POST'});toast('Dodano do listy ulubionych');loadStations();}catch(e){toast(e.message,1);}}
async function saveWeather(){const b=new URLSearchParams({place:$('wxPlace').value.trim()});
 try{await api('/api/weather',{method:'POST',body:b});toast('Zapisano. Pogoda pojawi się za kilka sekund.');setTimeout(loadSettings,6000);}catch(e){toast(e.message,1);}}
async function savePanelPass(){const b=new URLSearchParams({pass:$('ppass').value});
 try{await api('/api/password',{method:'POST',body:b});$('ppass').value='';toast('Zapisano hasło panelu');loadSettings();}catch(e){toast(e.message,1);}}
function uploadFw(f){if(!f)return;if(!/\.bin$/i.test(f.name)){toast('Wybierz plik .bin',1);return;}
 if(!confirm('Wgrać '+f.name+' ('+Math.round(f.size/1024)+' KB)? Odtwarzanie zostanie przerwane.'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();x.open('POST','/api/update');
 $('fwProg').hidden=false;$('fwState').textContent='Wysyłanie...';
 x.upload.onprogress=e=>{if(e.lengthComputable){$('fwProg').value=e.loaded*100/e.total;$('fwState').textContent='Wysyłanie: '+Math.round(e.loaded*100/e.total)+'%';}};
 x.onload=()=>{let j={};try{j=JSON.parse(x.responseText);}catch(e){}
  if(x.status===200&&j.ok){$('fwState').textContent='Gotowe. Radio uruchamia się ponownie...';setTimeout(()=>location.reload(),15000);}else $('fwState').textContent='Błąd: '+(j.error||x.status);};
 x.onerror=()=>{$('fwState').textContent='Błąd połączenia';};x.send(fd);}

// ---------- station management ----------
function renderManage(){if(!$('mlist'))return;$('mCount').textContent=stations.length;
 $('mlist').innerHTML=stations.length?stations.map((s,i)=>'<div class="mrow"><canvas width="88" height="66" id="mc'+i+'"></canvas><div class="tx"><div class="nm">'+(i+1)+'. '+esc(s.name)+'</div><div class="ds">'+esc(s.url)+'</div></div>'+
  '<button class="ib" title="W górę" onclick="move('+i+',-1)">'+I.upA+'</button><button class="ib" title="W dół" onclick="move('+i+',1)">'+I.downA+'</button>'+
  '<button class="ib" title="Edytuj" onclick="editStation('+i+')">'+I.edit+'</button><button class="ib d" title="Usuń" onclick="removeStation('+i+')">'+I.trash+'</button></div>').join(''):'<p class="hint">Lista jest pusta.</p>';
 stations.forEach((s,i)=>showLogo($('mc'+i),s.id,s.logo));}
async function saveList(list){const body=JSON.stringify(list.map(s=>({id:s.id||0,name:s.name,url:s.url,desc:s.desc||''})));
 stations=await api('/api/stations',{method:'POST',headers:{'Content-Type':'application/json'},body});renderManage();renderList();lastListKey='';return stations;}
async function move(i,d){const j=i+d;if(j<0||j>=stations.length)return;const l=stations.slice();[l[i],l[j]]=[l[j],l[i]];try{await saveList(l);}catch(e){toast(e.message,1);}}
async function removeStation(i){if(!confirm('Usunąć stację "'+stations[i].name+'"?'))return;const l=stations.slice();l.splice(i,1);try{await saveList(l);toast('Usunięto');}catch(e){toast(e.message,1);}}
let srcImg=null;
function syncSeg(){document.querySelectorAll('#segMode button').forEach(b=>b.classList.toggle('on',b.dataset.v===logoOpts.mode));
 document.querySelectorAll('#segBg button').forEach(b=>b.classList.toggle('on',b.dataset.v===logoOpts.bg));}
function rerenderLogo(){if(!srcImg)return;logoBytes=imageToLogo(srcImg);logoDrop=false;paint($('fLogo'),rawToCanvas(logoBytes.buffer));}
document.querySelectorAll('#segMode button').forEach(b=>b.onclick=()=>{logoOpts.mode=b.dataset.v;saveLogoOpts();syncSeg();rerenderLogo();});
document.querySelectorAll('#segBg button').forEach(b=>b.onclick=()=>{logoOpts.bg=b.dataset.v;saveLogoOpts();syncSeg();rerenderLogo();});
function editStation(i){editIdx=i;logoBytes=null;logoDrop=false;srcImg=null;$('fFile').value='';syncSeg();
 const s=i>=0?stations[i]:{name:'',url:'',desc:'',logo:false,id:0};$('dTitle').textContent=i>=0?'Edycja stacji':'Nowa stacja';
 $('fName').value=s.name;$('fUrl').value=s.url;$('fDesc').value=s.desc||'';showLogo($('fLogo'),s.id,s.logo);$('dlg').showModal();}
async function pickLogo(f){if(!f)return;try{srcImg=await loadImage(f);rerenderLogo();}catch(e){toast(e.message,1);}}
function dropLogo(){logoBytes=null;logoDrop=true;srcImg=null;paint($('fLogo'),null);$('fFile').value='';}
async function saveStation(){const st={name:$('fName').value.trim(),url:$('fUrl').value.trim(),desc:$('fDesc').value.trim()};
 if(!st.name){toast('Podaj nazwę',1);return;}if(!/^https?:\/\//i.test(st.url)){toast('Adres musi zaczynać się od http:// lub https://',1);return;}
 const l=stations.slice();let pos;if(editIdx>=0){st.id=stations[editIdx].id;l[editIdx]=st;pos=editIdx;}else{st.id=0;l.push(st);pos=l.length-1;}
 try{const saved=await saveList(l);const id=saved[pos].id;
  if(logoBytes)await uploadLogo(id,logoBytes);else if(logoDrop&&saved[pos].logo){await api('/api/logo/delete?id='+id,{method:'POST'});logoCache.delete(id);}
  $('dlg').close();await loadStations();lastLogoKey='';toast('Zapisano');poll();}catch(e){toast(e.message,1);}}
function exportList(){const data=JSON.stringify(stations.map(s=>({id:s.id,name:s.name,url:s.url,desc:s.desc||''})),null,1);
 const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([data],{type:'application/json'}));a.download='stacje.json';a.click();}
function importList(input){const f=input.files[0];if(!f)return;const r=new FileReader();
 r.onload=async()=>{input.value='';let l;try{l=JSON.parse(r.result);if(!Array.isArray(l))throw new Error('plik musi zawierać tablicę stacji');}catch(e){toast('Błędny plik: '+e.message,1);return;}
  if(!confirm('Zastąpić listę '+l.length+' stacjami z pliku?'))return;try{await saveList(l);toast('Zaimportowano '+l.length+' stacji');}catch(e){toast(e.message,1);}};r.readAsText(f);}

// ---------- screen colours ----------
const CGROUPS=[['Zegar',[['clock','Cyfry zegara'],['colon','Dwukropek']]],
 ['Górny pasek',[['date','Data (dzień tygodnia, rok)'],['dateAccent','Data (dzień i miesiąc)'],['accent','Wi-Fi, sygnał, PAUZA'],['volume','Głośność']]],
 ['Stacja',[['station','Nazwa stacji'],['desc','Opis stacji'],['label','Etykiety (STACJA RADIOWA...)']]],
 ['Utwór',[['artist','Wykonawca'],['title','Tytuł']]],
 ['Pogoda',[['temp','Temperatura'],['city','Miasto']]],
 ['Dolny pasek i linie',[['bottom','Tekst i ikony dolnego paska'],['divider','Linie podziału'],['frame','Ramki pól (zegar, stacja, utwór)']]],
 ['Equalizer',[['eqLow','Dół (zielony)'],['eqMid','Środek (żółty)'],['eqHigh','Góra (czerwony)']]]];
const THEMES={
 'Niebieski':{},
 'Bursztynowy':{frame:'#8a6420',colon:'#ffb300',dateAccent:'#ffb300',accent:'#ffb300',station:'#ffb300',title:'#ffc94d',desc:'#a89a84',label:'#a89a84',divider:'#4a3d24',eqLow:'#ffb300',eqMid:'#ff8f00',eqHigh:'#ff5a1f'},
 'Zielony retro':{frame:'#2f7a3a',clock:'#7dff6b',colon:'#3ddc84',date:'#b8ffb0',dateAccent:'#3ddc84',accent:'#3ddc84',volume:'#b8ffb0',station:'#3ddc84',desc:'#6f9a6a',label:'#6f9a6a',artist:'#d8ffd2',title:'#7dff6b',temp:'#b8ffb0',city:'#8fbf8a',bottom:'#8fbf8a',divider:'#244a2a',eqLow:'#3ddc84',eqMid:'#b6f542',eqHigh:'#f5e642'},
 'Biało-szary':{frame:'#6a6e75',dateAccent:'#ffffff',accent:'#cfd3d9',station:'#ffffff',title:'#cfd3d9',desc:'#8a8f98',label:'#8a8f98',divider:'#3a3d42',eqLow:'#e6e8eb',eqMid:'#b9bdc3',eqHigh:'#ffffff'},
 'Czerwony':{frame:'#8a2f2f',colon:'#ff4d4d',dateAccent:'#ff4d4d',accent:'#ff4d4d',station:'#ff4d4d',title:'#ff8080',desc:'#a08a8a',label:'#a08a8a',divider:'#4a2626',eqLow:'#ff8080',eqMid:'#ff4d4d',eqHigh:'#ffffff'}};
let colors={},colorDefaults={},colorTimer=null,colorPending={};
async function loadColors(){try{const r=await api('/api/colors');colors=r.colors;colorDefaults=r.defaults;}catch(e){toast(e.message,1);return;}renderColors();}
function renderColors(){if(!$('cgrid'))return;
 $('themes').innerHTML=Object.keys(THEMES).map(n=>{const t=Object.assign({},colorDefaults,THEMES[n]);
  return '<button class="btn g th" data-t="'+n+'"><i style="background:'+t.station+'"></i><i style="background:'+t.clock+'"></i>'+n+'</button>';}).join('');
 document.querySelectorAll('#themes button').forEach(b=>b.onclick=()=>applyTheme(b.dataset.t));
 $('cgrid').innerHTML=CGROUPS.map(g=>'<div class="cgrp">'+g[0]+'</div>'+g[1].map(c=>'<label class="crow" style="margin:0;text-transform:none;letter-spacing:0"><input type="color" data-k="'+c[0]+'" value="'+(colors[c[0]]||'#ffffff')+'"><span>'+c[1]+'</span><code>'+(colors[c[0]]||'')+'</code></label>').join('')).join('');
 document.querySelectorAll('#cgrid input[type=color]').forEach(inp=>inp.oninput=()=>{colors[inp.dataset.k]=inp.value;inp.parentNode.querySelector('code').textContent=inp.value;queueColors({[inp.dataset.k]:inp.value});});}
function queueColors(ch){Object.assign(colorPending,ch);clearTimeout(colorTimer);colorTimer=setTimeout(sendColors,250);}
async function sendColors(){const body=new URLSearchParams(colorPending);colorPending={};
 try{const r=await api('/api/colors',{method:'POST',body});colors=r.colors;}catch(e){toast(e.message,1);}}
function applyTheme(n){const t=Object.assign({},colorDefaults,THEMES[n]);colors=Object.assign({},t);renderColors();queueColors(t);toast('Motyw: '+n);}
async function resetColors(){try{const r=await api('/api/colors',{method:'POST',body:new URLSearchParams({reset:1})});colors=r.colors;renderColors();toast('Przywrócono domyślne kolory');}catch(e){toast(e.message,1);}}

// ---------- many logos at once ----------
function norm(t){return String(t).toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/ł/g,'l').replace(/\.[a-z0-9]+$/,'').replace(/[^a-z0-9]+/g,' ').trim();}
function bigrams(t){t=t.replace(/ /g,'');const r=[];for(let i=0;i<t.length-1;i++)r.push(t.slice(i,i+2));return r;}
function similarity(a,b){a=norm(a);b=norm(b);if(!a||!b)return 0;const A=a.replace(/ /g,''),B=b.replace(/ /g,'');
 if(A===B)return 1;if(A.includes(B)||B.includes(A))return .9*Math.min(A.length,B.length)/Math.max(A.length,B.length)+.1;
 const x=bigrams(a),y=bigrams(b);if(!x.length||!y.length)return 0;let hit=0;const pool=y.slice();
 for(const g of x){const k=pool.indexOf(g);if(k>=0){hit++;pool.splice(k,1);}}return 2*hit/(x.length+y.length);}
let bulk=[];
async function bulkOpen(files){if(!files.length)return;if(!stations.length){toast('Najpierw dodaj stacje',1);return;}
 bulk=[];const taken=new Set();
 for(const f of files){let img;try{img=await loadImage(f);}catch(e){continue;}
  let best=-1,score=0;stations.forEach((s,i)=>{const v=similarity(f.name,s.name);if(v>score){score=v;best=i;}});
  if(score<.35||taken.has(best))best=-1;if(best>=0)taken.add(best);
  bulk.push({name:f.name,bytes:imageToLogo(img,{mode:'fit',bg:'auto'}),idx:best});}
 if(!bulk.length){toast('Nie udało się odczytać obrazów',1);return;}
 const opts=i=>'<option value="-1">&mdash; pomiń &mdash;</option>'+stations.map((s,k)=>'<option value="'+k+'"'+(k===i?' selected':'')+'>'+(k+1)+'. '+esc(s.name)+'</option>').join('');
 $('bulkRows').innerHTML=bulk.map((b,n)=>'<div class="brow'+(b.idx<0?' skip':'')+'" id="br'+n+'"><canvas width="88" height="66" id="bc'+n+'"></canvas><div class="fn">'+esc(b.name)+'</div><select data-n="'+n+'">'+opts(b.idx)+'</select></div>').join('');
 bulk.forEach((b,n)=>paint($('bc'+n),rawToCanvas(b.bytes.buffer)));
 document.querySelectorAll('#bulkRows select').forEach(sel=>sel.onchange=()=>{const b=bulk[+sel.dataset.n];b.idx=+sel.value;$('br'+sel.dataset.n).classList.toggle('skip',b.idx<0);});
 $('bulkGo').disabled=false;$('bdlg').showModal();}
async function bulkSave(){const todo=bulk.filter(b=>b.idx>=0);if(!todo.length){$('bdlg').close();return;}
 $('bulkGo').disabled=true;let done=0;
 try{for(const b of todo){const s=stations[b.idx];$('bulkGo').textContent='Zapisuję '+(++done)+'/'+todo.length;await uploadLogo(s.id,b.bytes);s.logo=true;}
  toast('Zapisano logo: '+todo.length);}catch(e){toast(e.message,1);}
 $('bulkGo').textContent='Zapisz logo';$('bdlg').close();lastLogoKey='';renderManage();renderList();poll();}

loadStations();poll();setInterval(()=>{if(!document.hidden)poll();},2000);
setInterval(()=>{if(!document.hidden&&view==='set')loadSettings();},10000);
</script>
</body>
</html>
)rawliteral";
