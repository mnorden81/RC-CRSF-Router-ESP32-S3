// config_backup.cpp - RC_Common, siehe config_backup.h
#include "config_backup.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char CFG_PREFIX[]  = "cfg-";
static const char CFG_AUTO[]    = "-auto";
static const char CFG_SUFFIX[]  = ".json";

static bool digitsAt(const char* s, int n) {
    for (int i = 0; i < n; i++) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

bool cfgBackupWhenValid(const char* when) {
    return when && strlen(when) == 13 && digitsAt(when, 8) && when[8] == '-' && digitsAt(when + 9, 4);
}

// cfg-NNNN-YYYYMMDD-HHMM[-auto].json
int cfgBackupParseName(const char* name, bool* isAuto) {
    if (!name) return -1;
    size_t len = strlen(name);
    const size_t base = 4 + 4 + 1 + 13;                 // "cfg-" NNNN "-" when
    bool a;
    if (len == base + 5)      a = false;                // + ".json"
    else if (len == base + 10) a = true;                // + "-auto.json"
    else return -1;
    if (strncmp(name, CFG_PREFIX, 4) != 0 || !digitsAt(name + 4, 4) || name[8] != '-') return -1;
    char when[14];
    memcpy(when, name + 9, 13); when[13] = 0;
    if (!cfgBackupWhenValid(when)) return -1;
    const char* rest = name + base;
    if (a) { if (strncmp(rest, CFG_AUTO, 5) != 0) return -1; rest += 5; }
    if (strcmp(rest, CFG_SUFFIX) != 0) return -1;
    if (isAuto) *isAuto = a;
    return (name[4] - '0') * 1000 + (name[5] - '0') * 100 + (name[6] - '0') * 10 + (name[7] - '0');
}

void cfgBackupMakeName(char* out, size_t n, int number, const char* when, bool isAuto) {
    if (number < 0) number = 0;
    snprintf(out, n, "%s%04d-%s%s%s", CFG_PREFIX, number % 10000,
             cfgBackupWhenValid(when) ? when : "00000000-0000", isAuto ? CFG_AUTO : "", CFG_SUFFIX);
}

int cfgBackupPruneIndex(const char* const* names, int count, bool isAuto, int keep) {
    int found = 0, oldest = -1, oldestNr = 0;
    for (int i = 0; i < count; i++) {
        bool a;
        int nr = cfgBackupParseName(names[i], &a);
        if (nr < 0 || a != isAuto) continue;
        found++;
        if (oldest < 0 || nr < oldestNr) { oldest = i; oldestNr = nr; }
    }
    return found > keep ? oldest : -1;
}

#ifdef ARDUINO
#include <LittleFS.h>
#include <vector>

static const char BACKUP_DIR[] = "/backup";
static WebServer* bkServer = nullptr;
static bool bkMounted = false;

// Partition "spiffs" mit LittleFS. Ist sie schon eingebunden (Soundmodul
// Mini: Sounds im Flash), liefert begin() sofort true.
bool cfgBackupMount() {
    if (bkMounted) return true;
    bkMounted = LittleFS.begin(true);
    if (!bkMounted) Serial.println("[Backup] Kein Dateisystem (Partitionsschema mit SPIFFS waehlen)");
    else if (!LittleFS.exists(BACKUP_DIR)) LittleFS.mkdir(BACKUP_DIR);
    return bkMounted;
}

static void sendJson(int code, const String& j) {
    bkServer->sendHeader("Cache-Control", "no-cache");
    bkServer->send(code, "application/json", j);
}
static void sendError(const char* code) {
    sendJson(200, String("{\"ok\":false,\"error\":\"") + code + "\"}");
}

static std::vector<String> listNames() {
    std::vector<String> names;
    File dir = LittleFS.open(BACKUP_DIR);
    if (!dir || !dir.isDirectory()) return names;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (!f.isDirectory() && cfgBackupParseName(f.name()) >= 0) names.push_back(f.name());
        f.close();
    }
    return names;
}

static String pathOf(const String& name) { return String(BACKUP_DIR) + "/" + name; }

static void prune(bool isAuto, int keep) {
    for (;;) {
        std::vector<String> names = listNames();
        std::vector<const char*> p;
        for (auto& n : names) p.push_back(n.c_str());
        int i = cfgBackupPruneIndex(p.data(), (int)p.size(), isAuto, keep);
        if (i < 0) return;
        Serial.printf("[Backup] Entferne %s\n", p[i]);
        if (!LittleFS.remove(pathOf(names[i]))) return;
    }
}

static void handleList() {
    if (!cfgBackupMount()) { sendError("no_fs"); return; }
    String j = "{\"ok\":true,\"files\":[";
    bool first = true;
    for (auto& n : listNames()) {
        bool a = false;
        cfgBackupParseName(n.c_str(), &a);
        File f = LittleFS.open(pathOf(n), "r");
        size_t size = f ? f.size() : 0;
        if (f) f.close();
        if (!first) j += ',';
        first = false;
        j += "{\"name\":\"" + n + "\",\"size\":" + String((unsigned)size) + ",\"auto\":" + (a ? "1" : "0") + "}";
    }
    j += "],\"free\":" + String((unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes())) + "}";
    sendJson(200, j);
}

static void handleSave() {
    if (!cfgBackupMount()) { sendError("no_fs"); return; }
    const String& body = bkServer->arg("plain");
    if (body.length() == 0 || body[0] != '{') { sendError("bad_data"); return; }
    if (body.length() > CFG_BACKUP_MAX_SIZE) { sendError("too_big"); return; }
    bool isAuto = bkServer->arg("auto") == "1";
    int next = 1;
    for (auto& n : listNames()) { int nr = cfgBackupParseName(n.c_str()); if (nr >= next) next = nr + 1; }
    char name[CFG_BACKUP_NAME_LEN];
    cfgBackupMakeName(name, sizeof(name), next, bkServer->arg("when").c_str(), isAuto);
    // Erst in eine Zwischendatei, dann umbenennen: ein Stromausfall beim
    // Schreiben hinterlaesst keine halbe Sicherung unter gueltigem Namen.
    String tmp = String(BACKUP_DIR) + "/tmp";
    File f = LittleFS.open(tmp, "w");
    if (!f) { sendError("write"); return; }
    size_t w = f.print(body);
    f.close();
    if (w != body.length() || !LittleFS.rename(tmp, pathOf(name))) {
        LittleFS.remove(tmp);
        sendError("write");
        return;
    }
    Serial.printf("[Backup] Gesichert: %s (%u Bytes)\n", name, (unsigned)w);
    prune(isAuto, isAuto ? CFG_BACKUP_KEEP_AUTO : CFG_BACKUP_KEEP);
    sendJson(200, String("{\"ok\":true,\"file\":\"") + name + "\"}");
}

static bool fileArg(String& name) {
    name = bkServer->arg("file");
    return cfgBackupParseName(name.c_str()) >= 0;
}

static void handleGet() {
    if (!cfgBackupMount()) { sendError("no_fs"); return; }
    String name;
    if (!fileArg(name)) { sendError("bad_name"); return; }
    File f = LittleFS.open(pathOf(name), "r");
    if (!f) { sendError("not_found"); return; }
    bkServer->sendHeader("Cache-Control", "no-cache");
    bkServer->streamFile(f, "application/json");
    f.close();
}

static void handleDelete() {
    if (!cfgBackupMount()) { sendError("no_fs"); return; }
    String name;
    if (!fileArg(name)) { sendError("bad_name"); return; }
    if (!LittleFS.exists(pathOf(name))) { sendError("not_found"); return; }
    if (!LittleFS.remove(pathOf(name))) { sendError("write"); return; }
    sendJson(200, "{\"ok\":true}");
}

void cfgBackupRegister(WebServer& server) {
    bkServer = &server;
    server.on("/api/backup/list",   HTTP_GET,  handleList);
    server.on("/api/backup/save",   HTTP_POST, handleSave);
    server.on("/api/backup/get",    HTTP_GET,  handleGet);
    server.on("/api/backup/delete", HTTP_POST, handleDelete);
}
#endif
