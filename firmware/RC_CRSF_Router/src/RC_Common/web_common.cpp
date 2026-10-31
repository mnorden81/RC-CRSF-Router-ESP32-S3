// web_common.cpp - RC_Common, siehe web_common.h
#ifdef ARDUINO
#include "web_common.h"
#include "uart_roles.h"
#include "rc_common.h"
#include <Update.h>

static WebServer* otaServer = nullptr;
static void (*otaBeforeRestart)() = nullptr;

// ── OTA ──────────────────────────────────────────────────────────────────
// Die Datei wird ueber die Arduino-"Update"-Bibliothek in das gerade
// INAKTIVE OTA-Segment geschrieben (ota_0/ota_1). Die laufende Firmware
// bleibt unangetastet: schlaegt der Upload fehl oder bricht ab, startet das
// Modul einfach mit der bisherigen Firmware. Zwei-Handler-Muster des ESP32
// WebServers: otaData() laeuft mehrfach waehrend des Uploads, otaDone()
// einmal danach fuer die Antwort.
static bool     otaFailed  = false;
static String   otaErrCode;
static uint32_t otaBytes   = 0;
static bool     otaStarted = false;

static void otaData() {
    HTTPUpload& upload = otaServer->upload();
    if (upload.status == UPLOAD_FILE_START) {
        otaFailed = false; otaErrCode = ""; otaBytes = 0; otaStarted = false;
        Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
        // Groesse vorab unbekannt (multipart): bis zum Segmentende schreiben,
        // Vollstaendigkeit prueft Update.end().
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { otaFailed = true; otaErrCode = "update_begin"; return; }
        otaStarted = true;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (otaFailed || !otaStarted) return;
        // Jede ESP32-Firmware beginnt mit dem Magic-Byte 0xE9: faengt eine
        // falsche Datei (WAV, JSON, Komplett-Image ab 0x0) frueh ab.
        if (otaBytes == 0 && upload.currentSize > 0 && upload.buf[0] != 0xE9) {
            otaFailed = true; otaErrCode = "bad_file"; return;
        }
        size_t written = Update.write(upload.buf, upload.currentSize);
        otaBytes += written;
        if (written != upload.currentSize) { otaFailed = true; otaErrCode = "flash_write"; }
    } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
        if (upload.status == UPLOAD_FILE_ABORTED) { otaFailed = true; otaErrCode = "aborted"; }
        if (otaStarted) {
            if (otaFailed) Update.abort();
            else if (!Update.end(true)) { otaFailed = true; otaErrCode = "update_end"; }
            else Serial.printf("[OTA] Fertig: %u Bytes\n", (unsigned)otaBytes);
        }
    }
}

// Antwort per flush() hinausschicken und kurz warten, damit sie den Browser
// erreicht, bevor der Neustart die WLAN-Verbindung abbricht.
static void otaDone() {
    otaServer->sendHeader("Cache-Control", "no-cache");
    if (otaFailed) {
        otaServer->send(200, "application/json", "{\"ok\":false,\"error\":\"" + otaErrCode + "\"}");
        return;
    }
    otaServer->send(200, "application/json", "{\"ok\":true,\"size\":" + String(otaBytes) + "}");
    otaServer->client().flush();
    if (otaBeforeRestart) otaBeforeRestart();
    delay(500);
    ESP.restart();
}

// ── Gemeinsames JavaScript ───────────────────────────────────────────────
static const char RC_COMMON_JS[] PROGMEM = R"JS(
function rcErrText(c){
  var m={update_begin:'Update konnte nicht starten (Partition ohne OTA?)',
    bad_file:'Keine Firmware-Datei (.bin fuer OTA, nicht das Komplett-Image)',
    flash_write:'Schreiben in den Flash fehlgeschlagen',aborted:'Upload abgebrochen',
    update_end:'Datei unvollstaendig oder beschaedigt',
    no_fs:'Kein Flash-Speicher (Partitionsschema mit SPIFFS waehlen)',bad_data:'Keine gueltige Konfiguration',
    too_big:'Konfiguration zu gross',write:'Schreiben in den Flash fehlgeschlagen',
    bad_name:'Ungueltiger Dateiname',not_found:'Backup nicht gefunden',offline:'Modul nicht erreichbar'};
  return m[c]||c||'unbekannter Fehler';
}
function rcFillSelect(sel,options,value){
  if(!sel)return;
  sel.innerHTML='';
  options.split(';').forEach(function(t,i){var o=document.createElement('option');o.value=i;o.textContent=t;sel.appendChild(o);});
  if(value!==undefined&&value!==null)sel.value=String(value);
}
function rcRoleName(r){return RC_ROLES.split(';')[r]||RC_ROLES.split(';')[0];}
function rcRoleDefaultBaud(r){return RC_ROLE_BAUD[r]||0;}
// Baudraten-Dropdown: Werte aus RC_BAUD_VALUES (Eintrag 0 = "Auto"), ein
// eigener Wert ausserhalb der Liste bleibt als zusaetzlicher Eintrag erhalten
function rcFillBaud(sel,cur){
  if(!sel)return;
  sel.innerHTML='';
  var names=RC_BAUDS.split(';');
  RC_BAUD_VALUES.forEach(function(v,i){var o=document.createElement('option');o.value=v;o.textContent=names[i];sel.appendChild(o);});
  if(cur!==undefined&&cur!==null&&RC_BAUD_VALUES.indexOf(+cur)<0){var o=document.createElement('option');o.value=cur;o.textContent=cur;sel.appendChild(o);}
  if(cur!==undefined&&cur!==null)sel.value=String(cur);
}
function rcBaudName(v){var i=RC_BAUD_VALUES.indexOf(+v);return i>=0?RC_BAUDS.split(';')[i]:String(v);}
// before (optional): laeuft nach der Bestaetigung vor dem Upload und darf
// ein Promise liefern, z. B. ein automatisches Backup (rcBackupSnapshot)
function rcOtaUpload(file,statusEl,fileInput,onDone,before){
  if(!file)return;
  if(!confirm('Firmware-Update mit "'+file.name+'" starten? Waehrend des '+
      'Uploads die Spannungsversorgung des Moduls NICHT unterbrechen '+
      '- moeglichst ueber ESC/Akku bestromt, nicht nur ueber USB. '+
      'Das Modul startet danach automatisch neu.')){ if(fileInput)fileInput.value=''; return; }
  var st=function(t){ if(statusEl)statusEl.textContent=t; };
  if(fileInput)fileInput.value='';
  var go=function(){ rcOtaSend(file,st,onDone); };
  if(before){ st('Einstellungen sichern …'); Promise.resolve().then(before).then(go,go); }
  else go();
}
function rcOtaSend(file,st,onDone){
  st('Hochladen … (0%)');
  var fd=new FormData(); fd.append('file',file,file.name);
  var x=new XMLHttpRequest();
  x.open('POST','/api/otaupdate',true);
  x.upload.onprogress=function(e){ if(e.lengthComputable) st('Hochladen … ('+Math.round(e.loaded/e.total*100)+'%)'); };
  x.onload=function(){
    var d=null; try{ d=JSON.parse(x.responseText); }catch(e){}
    if(d&&d.ok){ st('Update erfolgreich ('+d.size+' Bytes) – Modul startet neu …'); if(onDone)onDone(true); }
    else { st('Fehler: '+rcErrText(d?d.error:'')); if(onDone)onDone(false,d?d.error:''); }
  };
  // Kann auch heissen: Update erfolgreich, Modul startet schon neu
  x.onerror=function(){ st('Verbindung unterbrochen – Update war evtl. trotzdem erfolgreich (neu verbinden und pruefen).'); };
  x.send(fd);
}
// Einstellungs-Backups im Flash (config_backup.cpp, /api/backup/*). Den
// Inhalt liefert das Projekt (dieselben Daten wie beim Export als Datei).
function rcBackupReq(method,url,body){
  var o={method:method};
  if(body!==undefined){o.body=body;o.headers={'Content-Type':'application/json'};}
  return fetch(url,o).then(function(r){return r.json();}).catch(function(){return {ok:false,error:'offline'};});
}
function rcBackupWhen(){
  var d=new Date(),p=function(n){return (n<10?'0':'')+n;};
  return d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'-'+p(d.getHours())+p(d.getMinutes());
}
function rcBackupList(){return rcBackupReq('GET','/api/backup/list');}
function rcBackupSave(data,auto){
  return rcBackupReq('POST','/api/backup/save?when='+rcBackupWhen()+(auto?'&auto=1':''),JSON.stringify(data));
}
function rcBackupGet(name){
  return fetch('/api/backup/get?file='+encodeURIComponent(name)).then(function(r){return r.json();})
    .then(function(d){return d&&d.ok===false?null:d;}).catch(function(){return null;});
}
function rcBackupDelete(name){return rcBackupReq('POST','/api/backup/delete?file='+encodeURIComponent(name));}
// Automatisches Backup vor Import/Firmware-Update. collect liefert die
// Konfiguration (oder ein Promise darauf); ein Fehler haelt nichts auf.
function rcBackupSnapshot(collect){
  return Promise.resolve().then(collect).then(function(d){return d?rcBackupSave(d,true):null;}).catch(function(){return null;});
}
// Anzeigename: "21.10.2026 21:05 · Nr. 12 (automatisch) · 3.4 kB"
function rcBackupLabel(f){
  var m=/^cfg-(\d{4})-(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})/.exec(f.name)||[];
  var t=m[2]&&m[2]!=='0000'?m[4]+'.'+m[3]+'.'+m[2]+' '+m[5]+':'+m[6]+' · ':'';
  return t+'Nr. '+(+m[1]||'?')+(f.auto?' (automatisch)':'')+' · '+(f.size/1024).toFixed(1)+' kB';
}
// Backups neuestes zuerst in ein <select> fuellen, liefert die Anzahl
function rcBackupFill(sel,files){
  if(!sel)return 0;
  sel.innerHTML='';
  (files||[]).slice().sort(function(a,b){return a.name<b.name?1:-1;}).forEach(function(f){
    var o=document.createElement('option');o.value=f.name;o.textContent=rcBackupLabel(f);sel.appendChild(o);
  });
  return sel.options.length;
}
)JS";

static void sendCommonJs() {
    String head = "var RC_COMMON_VERSION='" RC_COMMON_VERSION "';var RC_ROLES='";
    head += uartRoleOptions();
    head += "';var RC_BAUDS='";
    head += uartBaudOptions();
    head += "';var RC_BAUD_AUTO=" + String(UART_BAUD_AUTO) + ";var RC_BAUD_VALUES=[";
    for (uint8_t i = 0; i < UART_BAUD_COUNT; i++) { if (i) head += ','; head += String(UART_BAUD_LIST[i]); }
    head += "];var RC_ROLE_BAUD=[";
    for (uint8_t r = 0; r < UROLE_COUNT; r++) { if (r) head += ','; head += String(uartRoleDefaultBaud(r)); }
    head += "];\n";
    otaServer->sendHeader("Cache-Control", "max-age=300");
    otaServer->setContentLength(head.length() + strlen_P(RC_COMMON_JS));
    otaServer->send(200, "application/javascript", "");
    otaServer->sendContent(head);
    otaServer->sendContent_P(RC_COMMON_JS);
}

static void sendUartRoles() {
    String j = "{\"roles\":[";
    for (uint8_t r = 0; r < UROLE_COUNT; r++) {
        if (r) j += ',';
        j += "{\"id\":" + String(r) + ",\"name\":\"" + uartRoleName(r) + "\",\"baud\":" + String(uartRoleDefaultBaud(r)) + "}";
    }
    j += "],\"bauds\":[";
    for (uint8_t i = 0; i < UART_BAUD_COUNT; i++) { if (i) j += ','; j += String(UART_BAUD_LIST[i]); }
    j += "],\"auto\":" + String(UART_BAUD_AUTO) + "}";
    otaServer->sendHeader("Cache-Control", "no-cache");
    otaServer->send(200, "application/json", j);
}

void webCommonRegister(WebServer& server, void (*beforeRestart)()) {
    otaServer = &server;
    otaBeforeRestart = beforeRestart;
    server.on("/api/otaupdate", HTTP_POST, otaDone, otaData);
    server.on("/rc_common.js", HTTP_GET, sendCommonJs);
    server.on("/api/uartroles", HTTP_GET, sendUartRoles);
}

// ── JSON-Helfer ──────────────────────────────────────────────────────────
String webJsonEscape(const char* s) {
    String out;
    if (!s) return out;
    for (; *s; ++s) {
        switch (*s) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += *s; break;
        }
    }
    return out;
}

static int valueStart(const String& body, const char* key) {
    String token = "\"" + String(key) + "\"";
    int p = body.indexOf(token);
    if (p < 0) return -1;
    p = body.indexOf(':', p + token.length());
    if (p < 0) return -1;
    p++;
    while (p < (int)body.length() && isspace((unsigned char)body[p])) p++;
    return p;
}

bool webJsonInt(const String& body, const char* key, int& out) {
    int p = valueStart(body, key);
    if (p < 0) return false;
    char* end = nullptr;
    long v = strtol(body.c_str() + p, &end, 10);
    if (end == body.c_str() + p) return false;
    out = (int)v;
    return true;
}

bool webJsonBool(const String& body, const char* key, bool& out) {
    int p = valueStart(body, key);
    if (p < 0) return false;
    if (body.startsWith("true", p))  { out = true;  return true; }
    if (body.startsWith("false", p)) { out = false; return true; }
    return false;
}

bool webJsonString(const String& body, const char* key, String& out) {
    int p = valueStart(body, key);
    if (p < 0 || p >= (int)body.length() || body[p] != '"') return false;
    p++;
    out = "";
    bool esc = false;
    while (p < (int)body.length()) {
        char c = body[p++];
        if (esc) {
            switch (c) {
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                default:  out += c;    break;   // " \ /
            }
            esc = false;
            continue;
        }
        if (c == '\\') { esc = true; continue; }
        if (c == '"') return true;
        out += c;
    }
    return false;
}
#endif
