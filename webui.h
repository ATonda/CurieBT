#pragma once

const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>CurieBT</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#0f0f13;color:#dde;padding:0.8rem;max-width:480px;margin:auto;min-height:100vh}
h1{font-size:1.05rem;color:#4fc3f7;margin-bottom:1rem;text-align:center;letter-spacing:1px}
h2{font-size:0.82rem;color:#7eb8d4;margin:1.1rem 0 0.45rem;border-bottom:1px solid #2a2a3a;padding-bottom:4px;text-transform:uppercase;letter-spacing:0.5px}
.row{display:flex;align-items:center;gap:8px;margin-bottom:0.55rem}
.row label{flex:1;font-size:0.8rem;color:#aab;min-width:110px}
input[type=text],input[type=password],select{flex:2;background:#1a1a28;border:1px solid #333;color:#dde;padding:6px 9px;border-radius:6px;font-size:0.82rem;width:100%}
input:focus,select:focus{outline:none;border-color:#4fc3f7}
input[type=checkbox]{width:18px;height:18px;accent-color:#4fc3f7;flex:none;cursor:pointer}
.hint{font-size:0.7rem;color:#556;margin:-3px 0 6px 2px;line-height:1.4}
.card{background:#14141f;border:1px solid #282838;border-radius:8px;padding:0.75rem;margin-bottom:0.6rem}
.infobar{display:flex;gap:10px;align-items:center;font-size:0.78rem;color:#7eb8d4;background:#0d1422;border:1px solid #1565c0;border-radius:6px;padding:0.5rem 0.75rem;margin-bottom:0.7rem;flex-wrap:wrap}
.infobar b{color:#90caf9}
.preview{background:#080810;border:1px solid #1e2030;border-radius:8px;padding:0.75rem;margin-top:0.9rem}
.preview h3{font-size:0.75rem;color:#556;margin-bottom:0.4rem;text-transform:uppercase}
#prev{font-family:'Courier New',monospace;font-size:0.8rem;color:#57d982;word-break:break-all;line-height:1.6}
.live{display:flex;gap:14px;margin-bottom:0.6rem;flex-wrap:wrap}
.lval{background:#0d1422;border:1px solid #1a2a3a;border-radius:6px;padding:5px 10px;font-size:0.82rem}
.lval span{color:#4fc3f7;font-weight:700}
.save-btn{background:linear-gradient(135deg,#0d47a1,#1565c0);color:#90caf9;width:100%;margin-top:1rem;padding:12px;font-size:0.95rem;border:none;border-radius:8px;cursor:pointer;letter-spacing:0.5px;transition:opacity 0.2s}
.save-btn:active{opacity:0.8}
.save-btn:disabled{opacity:0.4;cursor:not-allowed}
.toast{display:none;position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#1b5e20;color:#a5d6a7;padding:10px 22px;border-radius:20px;font-size:0.85rem;box-shadow:0 4px 15px rgba(0,0,0,0.5);z-index:999}
.toast.err{background:#7f0000;color:#ffcdd2}
.sel-wrap{flex:2;display:flex;flex-direction:column;gap:3px}
.sel-factor{font-size:0.7rem;color:#4a6a7a;margin-top:1px}
</style>
</head>
<body>

<h1>⚛ CurieBT Nastavení</h1>

<div class="live" id="live" style="display:none">
  <div class="lval">CPS: <span id="l_cps">--</span></div>
  <div class="lval">VBAT: <span id="l_vbat">--</span> V</div>
  <div class="lval">Uptime: <span id="l_up">--</span> s</div>
</div>

<div class="infobar">
  📡 IP: <b>192.168.4.1</b>
  &nbsp;|&nbsp; Název: <b id="info_name">CurieBT</b>
</div>

<h2>🔵 BT &amp; WiFi</h2>
<div class="card">
  <div class="row">
    <label>Název zařízení</label>
    <input type="text" id="ble_name" maxlength="20" oninput="nameChg()" placeholder="CurieBT">
  </div>
  <div class="hint">Platí pro BT i WiFi AP. Změna se projeví po uložení.</div>
  <div class="row">
    <label>WiFi heslo</label>
    <input type="password" id="wifi_pass" maxlength="32" placeholder="prázdné = bez hesla">
  </div>
  <div class="hint">Min. 8 znaků pro zabezpečenou síť.</div>
</div>

<h2>☢ GM trubice</h2>
<div class="card">
  <div class="row">
    <label>Typ trubice</label>
    <div class="sel-wrap">
      <select id="gm_type" onchange="gmChg()">
        <option value="0.5556">SBM-20 (108 CPM/µSv/h)</option>
        <option value="0.4000">SBM-19 (150 CPM/µSv/h)</option>
        <option value="0.5000">SI-3BG (120 CPM/µSv/h)</option>
        <option value="0.1277">SI-22G (470 CPM/µSv/h)</option>
        <option value="0.5000">LND-712 (120 CPM/µSv/h)</option>
        <option value="0.5556">J305 (108 CPM/µSv/h)</option>
        <option value="custom">Vlastní faktor</option>
      </select>
      <div class="sel-factor" id="gm_fact_lbl"></div>
    </div>
  </div>

  <div class="row" id="gm_cust_row" style="display:none">
    <label>Faktor (µSv/CPS)</label>
    <input type="text" id="gm_factor" placeholder="0.5556" oninput="upd()">
  </div>

  <div class="row" style="margin-top:0.4rem">
    <input type="checkbox" id="en_rate" onchange="upd()">
    <label style="flex:unset">Odesílat µSv/h (RATE=)</label>
  </div>
</div>

<h2>🔋 Baterie (VBAT)</h2>
<div class="card">
  <div class="row">
    <input type="checkbox" id="en_vbat" onchange="upd()">
    <label style="flex:unset">Odesílat napětí baterie</label>
  </div>
  <div id="vbat_cfg">
    <div class="row" style="margin-top:0.5rem">
      <label>R1 (kΩ)</label>
      <input type="text" id="vbat_r1" value="100" oninput="upd()" placeholder="100">
    </div>
    <div class="row">
      <label>R2 (kΩ)</label>
      <input type="text" id="vbat_r2" value="100" oninput="upd()" placeholder="100">
    </div>
    <div class="hint" id="vbat_hint">Dělič: max vstup na GPIO34 = R2/(R1+R2) × Vbat</div>
  </div>
</div>

<div class="preview">
  <h3>📡 BT náhled výstupu</h3>
  <div id="prev">CPS=5.00 RATE=2.78 VBAT=3.7</div>
  <div style="color:#556;font-size:0.7rem;margin-top:4px">← každých 250ms (VBAT jen každých 5s)</div>
</div>

<button class="save-btn" id="save_btn" onclick="save()">💾 Uložit konfiguraci</button>
<div class="toast" id="toast"></div>

<script>
function n(id){ return document.getElementById(id); }
function pf(s){ return parseFloat(String(s||'0').replace(',','.'))||0; }

function showToast(msg, err){
  const t = n('toast');
  t.textContent = msg;
  t.className = 'toast' + (err ? ' err' : '');
  t.style.display = 'block';
  setTimeout(()=>{ t.style.display='none'; }, 3000);
}

// Faktory jsou µSv/CPS = 60 / CPM_per_uSvh
const GM_LABELS = {
  '0.5556': 'SBM-20 — 108 CPM při 1 µSv/h',
  '0.4000': 'SBM-19 — 150 CPM při 1 µSv/h',
  '0.5000': 'SI-3BG / LND-712 — 120 CPM při 1 µSv/h',
  '0.1277': 'SI-22G — 470 CPM při 1 µSv/h',
  'custom': 'Zadej vlastní konverzní faktor'
};

function nameChg(){
  n('info_name').textContent = n('ble_name').value || 'CurieBT';
  upd();
}

function gmChg(){
  const sel = n('gm_type').value;
  const isCustom = sel === 'custom';
  n('gm_cust_row').style.display = isCustom ? 'flex' : 'none';
  n('gm_fact_lbl').textContent = GM_LABELS[sel] || GM_LABELS['0.5556'];
  if (!isCustom) n('gm_factor').value = sel;
  upd();
}

function upd(){
  const sel    = n('gm_type').value;
  const factor = sel === 'custom' ? pf(n('gm_factor').value) : pf(sel);
  const enRate = n('en_rate').checked;
  const enVbat = n('en_vbat').checked;
  const r1     = pf(n('vbat_r1').value) || 100;
  const r2     = pf(n('vbat_r2').value) || 100;
  const maxAdcV = 3.3 * (r2 / (r1 + r2));
  n('vbat_hint').textContent = 'Dělič: max na GPIO34 = ' + maxAdcV.toFixed(2) + ' V (bezpečné < 3.3V ✓)';
  let p = 'CPS=5.00';
  if (enRate) p += ' RATE=' + (5 * factor).toFixed(2);
  if (enVbat) p += ' VBAT=3.8';
  p += '\\n';
  n('prev').textContent = p;
}

async function loadCfg(){
  try {
    const r = await fetch('/config');
    if (!r.ok) return;
    const c = await r.json();
    if (c.ble_name)  { n('ble_name').value = c.ble_name; n('info_name').textContent = c.ble_name; }
    if (c.wifi_pass) n('wifi_pass').value = c.wifi_pass;
    const factor = String(c.gm_factor || '0.5556');
    let matched = false;
    const opts = n('gm_type').options;
    for (let i = 0; i < opts.length; i++){
      if (Math.abs(parseFloat(opts[i].value) - parseFloat(factor)) < 0.0001){
        n('gm_type').selectedIndex = i; matched = true; break;
      }
    }
    if (!matched){ n('gm_type').value = 'custom'; n('gm_factor').value = factor; }
    n('en_rate').checked = !!c.en_rate;
    n('en_vbat').checked = !!c.en_vbat;
    if (c.vbat_r1) n('vbat_r1').value = c.vbat_r1;
    if (c.vbat_r2) n('vbat_r2').value = c.vbat_r2;
    gmChg(); upd();
  } catch(e){ console.warn('loadCfg:', e); }
}

async function pollStatus(){
  try {
    const r = await fetch('/status');
    if (!r.ok) return;
    const s = await r.json();
    n('l_cps').textContent  = (s.cps  || 0).toFixed(2);
    n('l_vbat').textContent = (s.vbat || 0).toFixed(1);
    n('l_up').textContent   = s.uptime || 0;
    n('live').style.display = 'flex';
  } catch(e){}
}

async function save(){
  const btn = n('save_btn');
  btn.disabled = true;
  btn.textContent = '⏳ Ukládám...';
  const sel    = n('gm_type').value;
  const factor = sel === 'custom' ? pf(n('gm_factor').value) : pf(sel);
  const data = {
    ble_name:  n('ble_name').value.trim(),
    wifi_pass: n('wifi_pass').value,
    gm_factor: factor,
    en_rate:   n('en_rate').checked ? 1 : 0,
    vbat_r1:   pf(n('vbat_r1').value),
    vbat_r2:   pf(n('vbat_r2').value),
    en_vbat:   n('en_vbat').checked ? 1 : 0
  };
  if (!data.ble_name){
    showToast('⚠ Název nesmí být prázdný', true);
    btn.disabled = false; btn.textContent = '💾 Uložit konfiguraci'; return;
  }
  try {
    const r = await fetch('/save', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify(data)
    });
    if (r.ok){ showToast('✅ Uloženo! AP se vypne.'); btn.textContent = '✅ Uloženo'; }
    else { showToast('❌ Chyba uložení', true); btn.disabled = false; btn.textContent = '💾 Uložit konfiguraci'; }
  } catch(e){
    showToast('❌ Chyba: ' + e.message, true);
    btn.disabled = false; btn.textContent = '💾 Uložit konfiguraci';
  }
}

loadCfg();
setInterval(pollStatus, 2000);
</script>
</body>
</html>
)rawhtml";
