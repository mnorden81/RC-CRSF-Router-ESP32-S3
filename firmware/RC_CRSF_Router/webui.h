#ifndef WEBUI_H
#define WEBUI_H

// Weboberflaeche des CRSF-Routers (eine Seite, Daten ueber /api/status und
// /api/config, Rollen/Baudraten/OTA/Backups aus /rc_common.js).
static const char WEBUI_HTML[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CRSF-Router</title>
<style>
:root{--bg:#f3f5f6;--p:#fff;--ink:#18232b;--mut:#5b6a74;--line:#d5dde2;--acc:#0d6f8c;--bad:#b3261e;--warn:#a86400;--ok:#2e7d32}
@media (prefers-color-scheme:dark){:root{--bg:#11181c;--p:#192227;--ink:#e3eaee;--mut:#93a3ad;--line:#2c3a42;--acc:#4fb3d1;--bad:#f28b82;--warn:#f0b05a;--ok:#81c784}}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.45 system-ui,sans-serif}
header{background:var(--acc);color:#fff;padding:10px 16px;display:flex;justify-content:space-between;flex-wrap:wrap;gap:6px}
header b{font-size:17px}
nav{display:flex;flex-wrap:wrap;gap:4px;padding:8px 16px;border-bottom:1px solid var(--line);background:var(--p)}
nav button{border:0;background:none;padding:6px 10px;color:var(--mut);font:inherit;cursor:pointer;border-bottom:2px solid transparent}
nav button.on{color:var(--ink);border-bottom-color:var(--acc);font-weight:600}
main{max-width:760px;margin:0 auto;padding:16px}
.card{background:var(--p);border:1px solid var(--line);border-radius:8px;padding:12px 14px;margin-bottom:12px}
h2{font-size:15px;margin:0 0 8px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px}.kv span:nth-child(odd){color:var(--mut)}
label{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:5px 0;flex-wrap:wrap}
select,input{font:inherit;color:var(--ink);background:var(--p);border:1px solid var(--line);border-radius:5px;padding:3px 6px}
button.b{font:inherit;border:1px solid var(--acc);background:var(--acc);color:#fff;border-radius:5px;padding:5px 12px;cursor:pointer}
button.s{font:inherit;border:1px solid var(--line);background:var(--p);color:var(--ink);border-radius:5px;padding:5px 12px;cursor:pointer}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-top:8px}
.bad{color:var(--bad)}.warn{color:var(--warn)}.ok{color:var(--ok)}.mut{color:var(--mut);font-size:12px}
.mono{font-family:ui-monospace,monospace}
[hidden]{display:none!important}
</style></head><body>
<header><b>CRSF-Router</b><span id="hdr" class="mono"></span></header>
<nav id="nav"><button data-t="over" class="on">Übersicht</button><button data-t="ports">Ports</button><button data-t="tele">Telemetrie</button><button data-t="sys">WLAN &amp; System</button></nav>
<main>
<section id="t-over">
 <div class="card"><h2>Status</h2><div class="kv" id="st"></div></div>
 <div class="card"><h2>Weiterleitung</h2><div class="kv" id="fw"></div>
  <p class="mut">Runter = vom Empfänger zu Port B/C, hoch = von Port B/C zum Empfänger. Verworfen = Warteschlange voll.</p></div>
</section>
<section id="t-ports" hidden>
 <div class="card"><h2>Ports</h2>
  <label>Port B (RX 7, TX 8) <span><select id="pbr"></select> <select id="pbb"></select></span></label>
  <label>Port C (RX 44, TX 43) <span><select id="pcr"></select> <select id="pcb"></select></span></label>
  <label>Moduladresse (WM) <select id="madr"></select></label>
  <p class="mut">CRSF = weitere Module (Kette), S.Port = FLVSS/MLVSS über Diode, GPS = NMEA-Modul. Rollen und Baudraten gelten nach einem Neustart.</p>
  <div class="row"><button class="b" id="savePorts">Speichern</button><button class="s" id="rst1">Neustart</button><span id="pmsg" class="mut"></span></div>
 </div>
 <div class="card"><h2>Aktiv seit dem Start</h2><div class="kv" id="pact"></div></div>
</section>
<section id="t-tele" hidden>
 <div class="card"><h2>Eigene Telemetrie</h2>
  <label>Empfängerspannung senden <select id="vmode"><option value="0">Aus</option><option value="1">als RxBt</option><option value="2">als Zellen-Wert</option></select></label>
  <label>Korrektur Spannung <select id="vcorr"></select></label>
  <label>Temperatur senden <select id="tsend"><option value="0">Aus</option><option value="1">Ein</option></select></label>
  <p class="mut">Spannung nur mit Spannungsteiler an GPIO1 (33 kΩ / 10 kΩ). „als RxBt“ nur, wenn der Empfänger selbst keine Spannung meldet.</p>
  <div class="row"><button class="b" id="saveTele">Speichern</button><span id="tmsg" class="mut"></span></div>
 </div>
 <div class="card"><h2>Sensoren</h2><div class="kv" id="sens"></div></div>
</section>
<section id="t-sys" hidden>
 <div class="card"><h2>WLAN</h2>
  <label>Name (SSID) <input id="ssid" maxlength="32"></label>
  <label>Passwort (min. 8) <input id="pass" maxlength="64" type="password" placeholder="unverändert"></label>
  <label>IP-Adresse <input id="ip" maxlength="15"></label>
  <label>Gerätename <input id="dnam" maxlength="23"></label>
  <label>Auto-WLAN ohne RC-Signal <select id="wauto"><option value="0">Aus</option><option value="1">Ein</option></select></label>
  <label>Auto-WLAN nach (s) <input id="wtmo" type="number" min="5" max="240"></label>
  <label>WLAN dauerhaft an <select id="walw"><option value="0">Aus</option><option value="1">Ein</option></select></label>
  <div class="row"><button class="b" id="saveSys">Speichern</button><span id="smsg" class="mut"></span></div>
 </div>
 <div class="card"><h2>Einstellungen sichern</h2>
  <div class="row"><button class="s" id="bkSave">Im Modul sichern</button><select id="bkSel"></select><button class="s" id="bkLoad">Aus Modul laden</button></div>
  <div class="row"><button class="s" id="exp">Export als Datei</button><label class="s" style="padding:5px 12px;border:1px solid var(--line);border-radius:5px">Import <input type="file" id="imp" accept=".json" hidden></label></div>
  <p id="bmsg" class="mut"></p>
 </div>
 <div class="card"><h2>Firmware-Update</h2>
  <div class="row"><input type="file" id="ota" accept=".bin"></div><p id="omsg" class="mut"></p></div>
</section>
</main>
<script src="/rc_common.js"></script>
<script>
var $=function(i){return document.getElementById(i);},C={};
function post(u,d){return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d||{})}).then(function(r){return r.json();}).catch(function(){return {ok:false,error:'offline'};});}
function msg(id,r,t){$(id).textContent=r&&r.ok?t:'Fehler: '+rcErrText(r?r.error:'');}
document.querySelectorAll('#nav button').forEach(function(b){b.onclick=function(){
  document.querySelectorAll('#nav button').forEach(function(x){x.classList.toggle('on',x===b);});
  ['over','ports','tele','sys'].forEach(function(t){$('t-'+t).hidden=t!==b.dataset.t;});};});
function kv(el,rows){$(el).innerHTML=rows.map(function(r){return '<span>'+r[0]+'</span><span>'+r[1]+'</span>';}).join('');}
function hex(a){return '0x'+(0xC0+(+a)).toString(16).toUpperCase();}
(function(){var o='';for(var i=0;i<16;i++)o+='<option value="'+i+'">'+i+' ('+hex(i)+')</option>';$('madr').innerHTML=o;
 o='';for(var v=-5;v<=5;v++)o+='<option value="'+v+'">'+(v>0?'+':'')+(v/10).toFixed(1)+' V</option>';$('vcorr').innerHTML=o;})();
function loadCfg(){fetch('/api/config').then(function(r){return r.json();}).then(function(c){C=c;
  rcFillSelect($('pbr'),RC_ROLES,c.portb_role);rcFillBaud($('pbb'),c.portb_baud);
  rcFillSelect($('pcr'),RC_ROLES,c.portc_role);rcFillBaud($('pcb'),c.portc_baud);
  $('madr').value=c.moduleAddr;$('vmode').value=c.voltMode;$('vcorr').value=c.voltCorr;$('tsend').value=c.tempSend?1:0;
  $('ssid').value=c.ssid;$('ip').value=c.ip;$('dnam').value=c.devName;$('wauto').value=c.wifiAuto?1:0;$('wtmo').value=c.wifiTimeout;$('walw').value=c.wifiAlways?1:0;});}
['pbr','pcr'].forEach(function(id){$(id).onchange=function(){var b=rcRoleDefaultBaud(+this.value);if(b)$(id==='pbr'?'pbb':'pcb').value=String(b);};});
$('savePorts').onclick=function(){post('/api/config',{portb_role:+$('pbr').value,portb_baud:+$('pbb').value,portc_role:+$('pcr').value,portc_baud:+$('pcb').value,moduleAddr:+$('madr').value})
  .then(function(r){msg('pmsg',r,'Gespeichert. Ports gelten nach dem Neustart.');});};
$('saveTele').onclick=function(){post('/api/config',{voltMode:+$('vmode').value,voltCorr:+$('vcorr').value,tempSend:$('tsend').value==='1'}).then(function(r){msg('tmsg',r,'Gespeichert.');});};
$('saveSys').onclick=function(){var d={ssid:$('ssid').value,ip:$('ip').value,devName:$('dnam').value,wifiAuto:$('wauto').value==='1',wifiTimeout:+$('wtmo').value,wifiAlways:$('walw').value==='1'};
  if($('pass').value)d.pass=$('pass').value;post('/api/config',d).then(function(r){msg('smsg',r,'Gespeichert. WLAN-Änderungen gelten nach dem Neustart.');});};
$('rst1').onclick=function(){post('/api/restart').then(function(){$('pmsg').textContent='Router startet neu …';});};
function collect(){return fetch('/api/config').then(function(r){return r.json();});}
function importCfg(d){if(!d||d.type!=='crsf-router'){$('bmsg').textContent='Keine Router-Konfiguration.';return Promise.resolve();}
  return rcBackupSnapshot(collect).then(function(){return post('/api/config',d);}).then(function(r){msg('bmsg',r,'Geladen. Neustart, damit alles gilt.');loadCfg();});}
function bkList(){rcBackupList().then(function(r){rcBackupFill($('bkSel'),r.files);});}
$('bkSave').onclick=function(){collect().then(function(d){return rcBackupSave(d,false);}).then(function(r){msg('bmsg',r,'Im Modul gesichert.');bkList();});};
$('bkLoad').onclick=function(){var n=$('bkSel').value;if(!n)return;rcBackupGet(n).then(importCfg);};
$('exp').onclick=function(){collect().then(function(d){var a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(d,null,1)],{type:'application/json'}));a.download='crsf-router-'+rcBackupWhen()+'.json';a.click();});};
$('imp').onchange=function(){var f=this.files[0];if(!f)return;var rd=new FileReader();rd.onload=function(){var d=null;try{d=JSON.parse(rd.result);}catch(e){}importCfg(d);};rd.readAsText(f);this.value='';};
$('ota').onchange=function(){rcOtaUpload(this.files[0],$('omsg'),this,null,function(){return rcBackupSnapshot(collect);});};
function poll(){fetch('/api/status').then(function(r){return r.json();}).then(function(s){
  $('hdr').textContent='v'+s.version+' · '+hex(s.moduleAddr);
  kv('st',[['RC-Signal',s.rc?'<b class="ok">OK</b>':'<b class="bad">kein Signal</b>'],['Link-Qualität',s.lq+' %'],
    ['Status-LED',s.ledText],['Moduladresse','WM '+s.moduleAddr+' · '+hex(s.moduleAddr)],
    ['Adresskonflikt',s.dupAddr?'<b class="bad">ja, ein Gerät weiter hinten hat dieselbe Adresse</b>':'nein'],
    ['Empfängerspannung',s.volt.toFixed(2)+' V'],['Temperatur',s.temp.toFixed(1)+' °C'],['Laufzeit',Math.floor(s.uptime/60)+' min']]);
  kv('fw',[['Frames runter',s.fwdDown],['Frames hoch',s.fwdUp],['Verworfen',s.dropped],['CRC-Fehler von unten',s.crcDown]]);
  kv('pact',s.ports.map(function(p){return [p.name,rcRoleName(p.role)+(p.role?' · '+rcBaudName(p.baud):'')+(p.duplicate?' <span class="warn">(Rolle schon auf Port B)</span>':'')];}));
  var sr=[];s.cells.forEach(function(c,i){sr.push(['LiPo '+(i+1),c.online?c.cells+' Zellen, '+c.total.toFixed(2)+' V':'nicht gefunden']);});
  if(s.gps)sr.push(['GPS',s.gps.sats+' Satelliten, '+s.gps.kmh.toFixed(1)+' km/h']);
  if(!sr.length)sr.push(['–','kein S.Port oder GPS aktiv']);kv('sens',sr);
}).catch(function(){$('hdr').textContent='nicht erreichbar';});}
loadCfg();bkList();poll();setInterval(poll,1000);
</script></body></html>)HTML";

#endif
