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
.amp{display:inline-block;width:12px;height:12px;border-radius:50%;background:var(--mut);vertical-align:middle;margin-right:6px}
.amp.g{background:var(--ok)}.amp.y{background:var(--warn)}.amp.r{background:var(--bad)}
table{border-collapse:collapse;width:100%;font-size:13px}td,th{text-align:left;padding:4px 6px;border-bottom:1px solid var(--line)}th{color:var(--mut);font-weight:500}
.tw{overflow-x:auto}
</style></head><body>
<header><b>CRSF-Router</b><span id="hdr" class="mono"></span></header>
<nav id="nav"><button data-t="over" class="on">Übersicht</button><button data-t="ports">Ports</button><button data-t="tele">Telemetrie</button><button data-t="funk">Funk</button><button data-t="sys">WLAN &amp; System</button></nav>
<main>
<section id="t-over">
 <div class="card"><h2>Status</h2><div class="kv" id="st"></div></div>
 <div class="card"><h2>Weiterleitung</h2><div class="kv" id="fw"></div>
  <p class="mut">Runter = vom Empfänger zu Port B/C, hoch = von Port B/C zum Empfänger. Verworfen = Warteschlange voll.</p></div>
</section>
<section id="t-ports" hidden>
 <div class="card"><h2>Ports</h2>
  <label>Port B (RX 5, TX 6) <span><select id="pbr"></select> <select id="pbb"></select></span></label>
  <label>Port C (RX 7, TX 8) <span><select id="pcr"></select> <select id="pcb"></select></span></label>
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
<section id="t-funk" hidden>
 <div class="card"><h2>Funk (ESP-NOW)</h2>
  <label>Eingang <select id="fin"><option value="0">Kabel (Port A)</option><option value="1">Kabel + Funk senden (Zentrale)</option><option value="2">Funk (Teilnehmer)</option></select></label>
  <div id="fset">
  <label>Fahrzeug-Kennung <input id="fid" type="number" min="1" max="255" style="width:80px"></label>
  <label>Funkkanal <select id="fch"></select></label>
  <label>Sendeleistung <select id="fpwr"><option value="2">2 dBm</option><option value="5">5 dBm</option><option value="8">8 dBm</option><option value="11">11 dBm</option><option value="15">15 dBm</option><option value="20">20 dBm</option></select></label>
  <label id="frow-rate">Kanäle alle <select id="frate"><option value="0">jedes Frame</option><option value="5">5 ms</option><option value="10">10 ms</option><option value="20">20 ms</option><option value="50">50 ms</option></select></label>
  <label id="frow-back">Rückkanal <select id="fback"><option value="1">An (Lua, Telemetrie)</option><option value="0">Aus (nur empfangen)</option></select></label>
  </div>
  <p class="mut" id="fhelp"></p>
  <div class="row"><button class="b" id="saveFunk">Speichern</button><button class="s" id="rst2">Neustart</button><span id="fmsg" class="mut"></span></div>
 </div>
 <div class="card" id="fstat-card"><h2><span class="amp" id="famp"></span><span id="fhead">Verbindung</span></h2><div class="kv" id="fown"></div><p id="fwarn" class="warn"></p></div>
 <div class="card" id="fpeer-card"><h2 id="fpt">Gegenstellen</h2><div class="tw" id="fpeers"></div>
  <div class="row"><button class="s" id="freset">Zähler zurücksetzen</button></div>
  <p class="mut">Ampel: grün = Verlust unter 1 % und RSSI über −75 dBm, gelb = bis 5 % oder bis −85 dBm, rot = schlechter.</p></div>
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
  ['over','ports','tele','funk','sys'].forEach(function(t){$('t-'+t).hidden=t!==b.dataset.t;});
  if(b.dataset.t==='funk')funkPoll();};});
function kv(el,rows){$(el).innerHTML=rows.map(function(r){return '<span>'+r[0]+'</span><span>'+r[1]+'</span>';}).join('');}
function hex(a){return '0x'+(0xC0+(+a)).toString(16).toUpperCase();}
(function(){var o='';for(var c=1;c<=13;c++)o+='<option value="'+c+'">Kanal '+c+([1,6,11].indexOf(c)>=0?' (empfohlen)':'')+'</option>';$('fch').innerHTML=o;})();
function funkRows(){var i=+$('fin').value;$('fset').hidden=i===0;$('frow-rate').hidden=i!==1;$('frow-back').hidden=i!==2;
  $('fhelp').textContent=i===1?'Zentrale: CRSF vom Empfänger an Port A geht zusätzlich in den Funk. Pro Fahrzeug genau eine Zentrale.'
   :i===2?'Teilnehmer: CRSF kommt nur aus dem Funk, Port A bleibt frei. Kennung und Kanal wie bei der Zentrale.':'Kein Funk. Gilt nach einem Neustart.';}
$('fin').onchange=funkRows;
$('saveFunk').onclick=function(){var id=Math.max(1,Math.min(255,parseInt($('fid').value)||1));$('fid').value=id;
  post('/api/config',{input:+$('fin').value,funk_id:id,funk_ch:+$('fch').value,funk_pwr:+$('fpwr').value,funk_rate:+$('frate').value,funk_back:$('fback').value==='1'})
  .then(function(r){msg('fmsg',r,'Gespeichert. Gilt nach dem Neustart.');});};
$('rst2').onclick=function(){post('/api/restart').then(function(){$('fmsg').textContent='Router startet neu …';});};
$('freset').onclick=function(){post('/api/funk/reset').then(funkPoll);};
function amp(loss,rssi,ok){return !ok?'r':(loss<1&&rssi>-75)?'g':(loss<=5&&rssi>=-85)?'y':'r';}
function pct(v){return v.toFixed(1).replace('.',',')+' %';}
function funkPoll(){if($('t-funk').hidden)return;fetch('/api/funk').then(function(r){return r.json();}).then(function(f){
  var on=!!f.role;$('fstat-card').hidden=!on;$('fpeer-card').hidden=!on;if(!on)return;
  $('famp').className='amp '+amp(f.loss,f.rssi,f.connected);
  $('fhead').textContent=f.role+': '+(f.connected?'verbunden':'keine Funk-Verbindung');
  var o=f.own,Z=f.role==='Zentrale';
  kv('fown',[['Kennung / Kanal / Leistung',f.id+' / '+f.ch+' / '+f.pwr+' dBm'],['MAC',f.mac],
   [Z?'Kanal-Frames in den Funk /s':'Kanal-Frames aus Funk /s',Z?o.ch_cable:o.ch_funk],['Frames gesendet /s',o.frames_out],['Frames empfangen /s',o.frames_in],
   ['Nicht zugestellt (gesamt)',o.noack],['Warteschlange voll (gesamt)',o.qfull]]);
  var w=[];if(f.conflict)w.push('Zweite Zentrale mit derselben Kennung im Funk. Pro Fahrzeug nur eine Zentrale.');
  if(f.dup_addr)w.push('Eine Gegenstelle hat dieselbe CRSF-Adresse ('+f.addr+'). Moduladresse ändern.');
  if(!f.connected&&!Z)w.push('Failsafe: seit '+(f.lost_ms/1000).toFixed(1).replace('.',',')+' s nichts von der Zentrale.');
  $('fwarn').innerHTML=w.join('<br>');
  $('fpt').textContent='Gegenstellen ('+f.peers.length+')';
  $('fpeers').innerHTML=f.peers.length?'<table><tr><th></th><th>Name</th><th>Rolle</th><th>Adresse</th><th>Pakete/s</th><th>Verlust</th><th>RSSI</th><th>Aussetzer</th><th>längste Lücke</th><th>verbunden</th></tr>'+
   f.peers.map(function(p){return '<tr><td><span class="amp '+amp(p.loss,p.rssi,p.alive)+'"></span></td><td>'+p.name+'</td><td>'+p.role+'</td><td class="mono">'+p.addr+'</td><td>'+p.pps+'</td><td>'+pct(p.loss)+'</td><td>'+p.rssi+' dBm</td><td>'+p.drops+'</td><td>'+p.gap+' ms</td><td>'+p.up+'</td></tr>';}).join('')+'</table>'
   :'<p class="mut">Noch keine Gegenstelle. Kennung und Funkkanal müssen überall gleich sein.</p>';
}).catch(function(){});}
setInterval(funkPoll,1000);
(function(){var o='';for(var i=0;i<16;i++)o+='<option value="'+i+'">'+i+' ('+hex(i)+')</option>';$('madr').innerHTML=o;
 o='';for(var v=-5;v<=5;v++)o+='<option value="'+v+'">'+(v>0?'+':'')+(v/10).toFixed(1)+' V</option>';$('vcorr').innerHTML=o;})();
function loadCfg(){fetch('/api/config').then(function(r){return r.json();}).then(function(c){C=c;
  rcFillSelect($('pbr'),RC_ROLES,c.portb_role);rcFillBaud($('pbb'),c.portb_baud);
  rcFillSelect($('pcr'),RC_ROLES,c.portc_role);rcFillBaud($('pcb'),c.portc_baud);
  $('madr').value=c.moduleAddr;$('vmode').value=c.voltMode;$('vcorr').value=c.voltCorr;$('tsend').value=c.tempSend?1:0;
  $('ssid').value=c.ssid;$('ip').value=c.ip;$('dnam').value=c.devName;$('wauto').value=c.wifiAuto?1:0;$('wtmo').value=c.wifiTimeout;$('walw').value=c.wifiAlways?1:0;
  $('fin').value=c.input||0;$('fid').value=c.funk_id;$('fch').value=c.funk_ch;$('fpwr').value=c.funk_pwr;$('frate').value=c.funk_rate;$('fback').value=c.funk_back?1:0;funkRows();});}
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
  kv('st',[['RC-Signal',s.rc?'<b class="ok">OK</b>':'<b class="bad">kein Signal</b>'],['Link-Qualität',s.lq+' %'],['Eingang (Port A)',s.rxBytes+' Bytes · '+s.rxFrames+' Frames · '+s.rxCrc+' CRC-Fehler'],
    ['Status-LED',s.ledText],['Moduladresse','WM '+s.moduleAddr+' · '+hex(s.moduleAddr)],
    ['Adresskonflikt',s.dupAddr?'<b class="bad">ja, ein Gerät '+(s.dupPorts&4?'im Funk':'an Port '+(s.dupPorts==3?'B und C':s.dupPorts==2?'C':'B'))+' hat dieselbe Adresse</b>':'nein'],
    ['Eingang',['Kabel (Port A)','Kabel + Funk (Zentrale)','Funk (Teilnehmer)'][s.input||0]+(s.funk?' · '+(s.funk.connected?'<span class="ok">verbunden, '+s.funk.peers+' Gegenstelle(n)</span>':'<span class="bad">keine Funk-Verbindung</span>'):'')],
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
