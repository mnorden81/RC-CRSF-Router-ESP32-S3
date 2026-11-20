// Simulierter CRSF-Router (Firmware RC_CRSF_Router, ESP32-S3 Zero).
// Startwerte aus dem Code: build.py fuehrt configJson() und
// routerStatusJson() trocken aus (nach cfgDefaults()), Pins aus pins.h.
PST.register('mock_router', function (variant, ctx) {
  var K = ctx.variant.consts || {};
  var M = PST.autoMock(ctx, {});
  var C = M.get('/api/config');
  var boot = { input: C.input, b: C.portb_role, c: C.portc_role, addr: C.moduleAddr };
  var apActive = true, ledGrb = 0, restart = 0;
  var IN = ['Kabel', 'Kabel + Funk (Zentrale)', 'Funk (Teilnehmer)'];
  var ROLES = ['Aus', 'CRSF', 'S.Port', 'GPS', 'Raw'];
  var common = PST.common({
    name: function () { return C.devName; }, addr: function () { return C.moduleAddr; },
    funkRole: function () { return boot.input === 1 ? 'Zentrale' : (boot.input === 2 ? 'Teilnehmer' : null); },
    funkCfg: function () { return { id: C.funk_id, ch: C.funk_ch, pwr: C.funk_pwr, rate: C.funk_rate, back: C.funk_back }; },
    backupData: function () { return C; }
  });
  var t0 = Date.now();
  function status() {
    var s = JSON.parse(JSON.stringify(M.get('/api/status'))), t = (Date.now() - t0) / 1000;
    s.rc = true; s.lq = 100;
    s.rxBytes = Math.round(t * 2900); s.rxFrames = Math.round(t * 150); s.rxCrc = 0;
    s.moduleAddr = boot.addr; s.input = boot.input; s.funk = common.funkSummary() || 0;
    s.volt = C.voltMode ? 7.84 + C.voltCorr / 10 : 0; s.temp = 41.5; s.uptime = Math.round(t) + 37;
    s.fwdDown = Math.round(t * 150); s.fwdUp = Math.round(t * 12); s.dropped = 0; s.crcDown = 0;
    s.ledText = boot.input ? 'Funk' : 'CRSF ok';
    s.ports = [{ name: 'B', role: boot.b, baud: C.portb_baud, duplicate: false }, { name: 'C', role: boot.c, baud: C.portc_baud, duplicate: false }];
    s.cells = (boot.b === 2 || boot.c === 2) ? [{ online: true, cells: 3, total: 11.82 }, { online: false, cells: 0, total: 0 }] : [];
    if (boot.b === 3 || boot.c === 3) s.gps = { sats: 9, kmh: 12.4 }; else delete s.gps;
    return s;
  }
  function handle(m, path, q, b) {
    var r = common.handle(m, path, q, b); if (r) return r;
    if (path === '/api/status') return PST.json(status());
    if (path === '/api/restart') { setTimeout(PST.reboot, 300); return PST.json({ ok: true }); }
    if (path === '/api/config' && m === 'POST') {
      var d = PST.parseBody(b);
      if (d.pass != null && String(d.pass).length < 8) delete d.pass;   // wie handleConfigPost()
      M.apply(JSON.stringify(d));
      return PST.json({ ok: true });
    }
    return M.handle(m, path, q, b);
  }
  // ── Lua, Funktionsnamen wie in RC_CRSF_Router.ino ──
  var BAUDS = [1, 9600, 19200, 38400, 57600, 115200, 230400, 420000], ROLE_BAUD = [0, 420000, 57600, 1, 115200];
  function role(a) { return a ? C.portc_role : C.portb_role; }
  var funkCfg = { get id() { return C.funk_id; }, set id(v) { C.funk_id = v; }, get ch() { return C.funk_ch; }, set ch(v) { C.funk_ch = v; },
    get pwr() { return C.funk_pwr; }, set pwr(v) { C.funk_pwr = v; }, get rate() { return C.funk_rate; }, set rate(v) { C.funk_rate = v; },
    get back() { return C.funk_back; }, set back(v) { C.funk_back = !!v; } };
  var lua = PST.mergeLua({
    get: {
      luaGetRole: function (a) { return role(a); }, luaGetBaud: function (a) { return Math.max(0, BAUDS.indexOf(a ? C.portc_baud : C.portb_baud)); },
      luaGetAddr: function () { return C.moduleAddr; }, luaGetVMode: function () { return C.voltMode; },
      luaGetVCorr: function () { return C.voltCorr + 5; }, luaGetTSend: function () { return C.tempSend ? 1 : 0; },
      luaGetWifiNow: function () { return apActive ? 1 : 0; }, luaGetWifiAuto: function () { return C.wifiAuto ? 1 : 0; },
      luaGetWifiTmo: function () { return C.wifiTimeout; }, luaGetWifiAlw: function () { return C.wifiAlways ? 1 : 0; },
      luaGetLedRgb: function () { return ledGrb; }, luaGetRestart: function () { return restart; },
      luaGetInput: function () { return C.input; }
    },
    set: {
      luaSetRole: function (a, v) { var k = a ? 'portc' : 'portb'; if (v !== C[k + '_role'] && ROLE_BAUD[v] && v !== 4) C[k + '_baud'] = ROLE_BAUD[v]; C[k + '_role'] = v; },
      luaSetBaud: function (a, v) { C[(a ? 'portc' : 'portb') + '_baud'] = BAUDS[v]; },
      luaSetAddr: function (a, v) { C.moduleAddr = v; boot.addr = v; }, luaSetVMode: function (a, v) { C.voltMode = v; },
      luaSetVCorr: function (a, v) { C.voltCorr = v - 5; }, luaSetTSend: function (a, v) { C.tempSend = !!v; },
      luaSetWifiNow: function (a, v) { apActive = !!v; }, luaSetWifiAuto: function (a, v) { C.wifiAuto = !!v; },
      luaSetWifiTmo: function (a, v) { C.wifiTimeout = v; }, luaSetWifiAlw: function (a, v) { C.wifiAlways = !!v; },
      luaSetLedRgb: function (a, v) { ledGrb = v ? 1 : 0; },
      luaSetRestart: function (a, v) { if (v) { restart = 1; setTimeout(function () { restart = 0; PST.reboot(); }, 300); } },
      luaSetInput: function (a, v) { C.input = v; }
    },
    views: {
      luaShowVersion: function (it, v) { v.text = ctx.variant.version + ' S3'; },
      luaShowName: function (it, v) { v.text = C.devName; },
      luaShowPortsFolder: function (it, v) { v.name = 'Ports (B ' + ROLES[role(0)] + ', C ' + ROLES[role(1)] + ')'; },
      luaViewBaud: function (it, v) { if (!role(it.arg)) v.hidden = true; },
      luaViewVCorr: function (it, v) { if (!C.voltMode) v.hidden = true; },
      luaShowVolt: function (it, v) { v.text = status().volt.toFixed(2) + ' V'; },
      luaShowTemp: function (it, v) { v.text = '41.5 C'; },
      luaShowCount: function (it, v) { var s = status(); v.text = String([s.fwdDown, s.fwdUp, s.dropped, s.crcDown][it.arg]); },
      luaShowDup: function (it, v) { v.text = 'nein'; },
      luaViewFunkInfo: function (it, v) { v.text = boot.input ? '1 Geg., 0.2% Verl., -48 dBm' : 'Funk aus'; }
    }
  }, PST.funkLua(funkCfg, function () { return C.input; }, function (v) { C.input = v; }));
  delete lua.set.luaSetFunkMode; delete lua.get.luaGetFunkMode;
  lua.views.luaShowFunkFolder = function (it, v) { v.name = boot.input ? 'Funk (' + (boot.input === 1 ? 'Zentrale' : 'Teilnehmer') + ', verbunden)' : 'Funk (aus)'; };
  return {
    handle: handle, get: lua.get, set: lua.set, views: lua.views,
    deviceName: function () { return C.devName; },
    reboot: function () { boot = { input: C.input, b: C.portb_role, c: C.portc_role, addr: C.moduleAddr }; },
    hw: function () {
      return [['Board', 'ESP32-S3 Zero'], ['Port A (Eingang)', 'RX ' + K.PIN_UPLINK_RX + ' / TX ' + K.PIN_UPLINK_TX],
        ['Port B', 'RX ' + K.PIN_PORTB_RX + ' / TX ' + K.PIN_PORTB_TX], ['Port C', 'RX ' + K.PIN_PORTC_RX + ' / TX ' + K.PIN_PORTC_TX],
        ['Status-LED', 'GPIO ' + K.PIN_STATUS_LED], ['Spannungsmessung', 'GPIO ' + K.PIN_VSENSE], ['Eingang (Start)', IN[boot.input]]];
    }
  };
});
