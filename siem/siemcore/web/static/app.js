
function getStoredAuth() {
  return sessionStorage.getItem("siem_auth");
}

async function api(path){
  const auth = getStoredAuth();
  const headers = {};
  
  if (auth) {
    headers["Authorization"] = `Basic ${auth}`;
  }
  
  const r = await fetch(path, {
    credentials: "same-origin",
    headers: headers,
    cache: "no-store"
  });
  
  if(!r.ok){
    if (r.status === 401) {
      sessionStorage.removeItem("siem_auth");
      location.href = "/login";
      return;
    }
    const t = await r.text();
    throw new Error(t || ("HTTP " + r.status));
  }
  return await r.json();
}

async function checkAuth() {
  const auth = getStoredAuth();
  if (!auth) {
    location.href = "/login";
    return false;
  }
  return true;
}

function esc(s){
  return String(s ?? "").replace(/[&<>"']/g, c => ({
    '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":"&#39;"
  }[c]));
}

function sevRu(s){
  const m = { low: "низкая", medium: "средняя", high: "высокая" };
  const k = String(s ?? "").toLowerCase();
  return m[k] || (s ?? "");
}

function typeRu(t){
  const m = {
    raw: "Сырой лог",
    process_start: "Запуск процесса",
    userlogin: "Вход в систему",
    userlogin_failed: "Неудачный вход"
  };
  return m[String(t ?? "")] || (t ?? "");
}

function translateListKeyByElementId(el, key){
  if(!el) return key;
  if(el.id === "severity") return sevRu(key);
  if(el.id === "topEventTypes") return typeRu(key);
  return key;
}

function renderList(el, items){
  if(!el) return;
  el.innerHTML = "";
  for(const it of items){
    const row = document.createElement("div");
    row.className = "item";
    row.innerHTML = `<div>${esc(translateListKeyByElementId(el, it.key))}</div><div class="badge">${esc(it.value)}</div>`;
    el.appendChild(row);
  }
}

function renderLastLogins(tbl, rows){
  if(!tbl) return;
  const tb = tbl.querySelector("tbody");
  if(!tb) return;
  tb.innerHTML = "";
  for(const e of rows){
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${esc(e.timestamp)}</td>
      <td>${esc(e.hostname)}</td>
      <td>${esc(e.user)}</td>
      <td>${esc(e.process)}</td>
      <td>${esc(typeRu(e.eventtype))}</td>
      <td>${esc(sevRu(e.severity))}</td>
    `;
    tb.appendChild(tr);
  }
}

function makeEventRow(e){
  const tr = document.createElement("tr");
  tr.dataset.id = e._id || "";
  tr.innerHTML = `
    <td>${esc(e.timestamp)}</td>
    <td>${esc(e.agentid)}</td>
    <td>${esc(e.hostname)}</td>
    <td>${esc(e.user)}</td>
    <td>${esc(e.process)}</td>
    <td>${esc(typeRu(e.eventtype))}</td>
    <td>${esc(sevRu(e.severity))}</td>
    <td>${esc(e.source)}</td>
    <td title="${esc(e.rawlog)}">${esc(String(e.rawlog ?? "").slice(0,120))}</td>
  `;
  return tr;
}

function showModal(title, body){
  let m = document.getElementById("modal");
  if(!m){
    m = document.createElement("div");
    m.id = "modal";
    m.style.position = "fixed";
    m.style.inset = "0";
    m.style.background = "rgba(0,0,0,.55)";
    m.style.display = "flex";
    m.style.alignItems = "center";
    m.style.justifyContent = "center";
    m.style.zIndex = "9999";
    m.innerHTML = `
      <div style="max-width:900px;width:92vw;max-height:86vh;overflow:auto;background:#111a2e;border:1px solid rgba(255,255,255,.08);border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,.35);padding:14px">
        <div style="display:flex;gap:10px;align-items:center;justify-content:space-between">
          <div id="modalTitle" style="font-weight:700"></div>
          <button id="modalClose" class="btn">Закрыть</button>
        </div>
        <pre id="modalBody" style="white-space:pre-wrap;margin:10px 0 0"></pre>
      </div>
    `;
    document.body.appendChild(m);
    m.addEventListener("click", (e) => { if(e.target === m) m.remove(); });
    m.querySelector("#modalClose").addEventListener("click", () => m.remove());
  }
  m.querySelector("#modalTitle").textContent = title;
  m.querySelector("#modalBody").textContent = body;
}

function debounce(fn, ms){
  let t = 0;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}

function kvToLabelsValues(kv, translateFn){
  const labels = [];
  const values = [];
  for(const it of (kv || [])){
    labels.push(translateFn ? translateFn(it.key) : String(it.key ?? ""));
    values.push(Number(it.value ?? 0));
  }
  return {labels, values};
}

function makeDoughnut(ctx, labels, values){
  return new Chart(ctx, {
    type: "doughnut",
    data: {
      labels,
      datasets: [{
        data: values,
        borderWidth: 0
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false }   
      },
      cutout: "65%"                 
    }
  });
}

async function initDashboard(){
  if (!checkAuth()) return;
  const d = await api("/api/dashboard");
  if (!d) return;

  renderList(document.getElementById("activeAgents"), d.active_agents || []);
  renderList(document.getElementById("topEventTypes"), d.top_event_types || []);
  renderList(document.getElementById("severity"), d.severity_24h || []);
  renderList(document.getElementById("topUsers"), d.top_users_24h || []);
  renderList(document.getElementById("hosts24h"), d.hosts_24h || []);
  renderList(document.getElementById("topProcesses24h"), d.top_processes_24h || []);

  renderLastLogins(document.getElementById("lastLogins"), d.last_logins || []);

  if(typeof Chart === "undefined") return;

  const c1 = document.getElementById("eventsPerHour");
  if(c1){
    const labels = Array.from({length:24}, (_,i)=>String(i));
    const data = d.events_per_hour_24 || Array(24).fill(0);

    new Chart(c1, {
      type: "bar",
      data: { labels, datasets: [{ label: "Событий/час", data, borderWidth: 0 }] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { legend: { display: false } },
        scales: {
          x: { grid: { color: "rgba(255,255,255,.06)" }, ticks: { color: "rgba(232,238,252,.75)" } },
          y: { grid: { color: "rgba(255,255,255,.06)" }, ticks: { color: "rgba(232,238,252,.75)" } }
        }
      }
    });
  }

  const c2 = document.getElementById("topEventTypesChart");
  if(c2){
    const {labels, values} = kvToLabelsValues(d.top_event_types || [], typeRu);
    makeDoughnut(c2, labels, values);
  }

  const c3 = document.getElementById("severityChart");
  if(c3){
    const {labels, values} = kvToLabelsValues(d.severity_24h || [], sevRu);
    makeDoughnut(c3, labels, values);
  }
}

async function initEvents(){
  if (!checkAuth()) return;

  const qEl = document.getElementById("q");
  const reEl = document.getElementById("re");
  const reloadEl = document.getElementById("reload");
  const tbl = document.getElementById("eventsTable");
  const status = document.getElementById("statusLine");

  const fUser = document.getElementById("fUser");
  const fHost = document.getElementById("fHost");
  const fType = document.getElementById("fType");
  const fSev  = document.getElementById("fSev");
  const fProc = document.getElementById("fProc");

  const exportCsv = document.getElementById("exportCsv");
  const exportJson = document.getElementById("exportJson");

  if(!qEl || !reloadEl || !tbl) return;

  let cursor = "";
  let hasMore = true;
  let loading = false;

  function buildParams(forExport){
    const p = new URLSearchParams();
    p.set("limit", forExport ? "50000" : "200");

    const q = (qEl.value || "").trim();
    if(q) p.set("q", q);
    if(reEl && reEl.checked) p.set("re", "1");

    if(fUser && fUser.value.trim()) p.set("user", fUser.value.trim());
    if(fHost && fHost.value.trim()) p.set("host", fHost.value.trim());
    if(fType && fType.value.trim()) p.set("type", fType.value.trim());
    if(fSev  && fSev.value.trim())  p.set("severity", fSev.value.trim());
    if(fProc && fProc.value.trim()) p.set("process", fProc.value.trim());

    if(!forExport && cursor) p.set("cursor", cursor);
    return p;
  }

  function updateExportLinks(){
    const auth = getStoredAuth();
    const authParam = auth ? "&auth=" + encodeURIComponent(auth) : "";
    if(exportCsv) exportCsv.href = "/api/events.csv?" + buildParams(true).toString() + authParam;
    if(exportJson) exportJson.href = "/api/events.json?" + buildParams(true).toString() + authParam;
  }

  async function loadNext(){
    if(!hasMore || loading) return;
    loading = true;

    try{
      updateExportLinks();

      const url = "/api/events?" + buildParams(false).toString();
      const r = await api(url);

      const items = r.data || [];
      hasMore = !!r.has_more;
      cursor = r.next_cursor || "";

      const tb = tbl.querySelector("tbody");
      for(const e of items){
        tb.appendChild(makeEventRow(e));
      }

      if(status){
        status.textContent = hasMore ? "Прокрутите вниз, чтобы загрузить ещё..." : "Конец списка.";
      }
    } catch (e) {
      if(status) status.textContent = e.message;
      throw e;
    } finally {
      loading = false;
    }
  }

  async function resetAndLoad(){
    cursor = "";
    hasMore = true;
    tbl.querySelector("tbody").innerHTML = "";
    if(status) status.textContent = "Загрузка...";
    await loadNext();
  }

  tbl.addEventListener("click", async (ev) => {
    const tr = ev.target.closest("tr");
    if(!tr) return;
    const id = tr.dataset.id;
    if(!id) return;
    const r = await api("/api/events/" + encodeURIComponent(id));
    const e = r.data;
    showModal("Событие " + id, JSON.stringify(e, null, 2));
  });

  const onFilter = () => resetAndLoad().catch(e => alert(e.message));
  qEl.addEventListener("input", debounce(onFilter, 300));
  if(reEl) reEl.addEventListener("change", onFilter);
  for(const el of [fUser,fHost,fType,fSev,fProc]){
    if(el) el.addEventListener("input", debounce(onFilter, 300));
  }

  reloadEl.addEventListener("click", () => resetAndLoad().catch(e => alert(e.message)));

  window.addEventListener("scroll", () => {
    const nearBottom = window.innerHeight + window.scrollY >= document.body.offsetHeight - 400;
    if(nearBottom) loadNext().catch(() => {});
  });

  await resetAndLoad();
}



function doBasicLogout(){

  sessionStorage.removeItem("siem_auth");
  
  location.href = "/logout";
}


(function bindLogout(){
  const a = document.getElementById("logoutLink");
  if(!a) return;

  a.addEventListener("click", (e) => {
    e.preventDefault();
    doBasicLogout();
    setTimeout(() => { location.href = "/login"; }, 800);
  });
})();

(function(){
  const p = (location.pathname || "").replace(/\/+$/, "");
  
  if (p !== "" && p !== "/login" && p !== "/logout") {
    if (!getStoredAuth()) {
      location.href = "/login";
      return;
    }
  }
  
  if(p === "" || p === "/dashboard") initDashboard().catch(e => alert(e.message));
  if(p === "/events") initEvents().catch(e => alert(e.message));
})();
