#include "http_channel.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif_types.h"
#include "cJSON.h"
#include "messages.h"
#include "channel.h"
#include "agent.h"
#include "tools.h"
#include "memory.h"
#include "llm.h"
#include "display_task.h"
#include "LVGL_Driver.h"
#include "lvgl.h"
#include "nvs_keys.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "http_channel";
static httpd_handle_t server = NULL;
static QueueHandle_t s_input_queue = NULL;

#define EXPR_COUNT 10

static const char *EXPR_NAMES[EXPR_COUNT] = {
    "neutral", "happy", "wink", "surprised", "sleepy",
    "thinking", "suspicious", "cry", "oops", "sad"
};

#if 0
// Expression HTML page
static const char *CONTROL_HTML =
"<!DOCTYPE html><html><head>"
"<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"* {margin:0;padding:0;box-sizing:border-box}"
"body {background:#0d1117;color:#c9d1d9;font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh}"
".header {background:#161b22;border-bottom:1px solid #30363d;padding:12px 20px;display:flex;align-items:center;gap:16px}"
".header h1 {color:#58a6ff;font-size:18px;font-weight:600}"
".header .version {color:#484f58;font-size:12px}"
".tabs {display:flex;background:#161b22;border-bottom:1px solid #30363d;padding:0 20px}"
".tab {padding:12px 20px;cursor:pointer;color:#8b949e;border-bottom:2px solid transparent;font-size:14px}"
".tab:hover {color:#c9d1d9}"
".tab.active {color:#58a6ff;border-color:#58a6ff}"
".content {padding:20px;max-width:900px}"
".card {background:#161b22;border:1px solid #30363d;border-radius:8px;padding:16px;margin-bottom:16px}"
".card h3 {color:#f0f6fc;font-size:14px;margin-bottom:12px;font-weight:600}"
".row {display:flex;gap:8px;margin-bottom:8px;flex-wrap:wrap}"
".btn {padding:8px 16px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:13px}"
".btn:hover {background:#30363d;border-color:#8b949e}"
".btn.primary {background:#238636;border-color:#238636;color:#fff}"
".btn.primary:hover {background:#2ea043}"
".btn.danger {background:#da3633;border-color:#da3633;color:#fff}"
".expr-grid {display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:8px}"
".expr-btn {padding:12px 8px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:12px;text-align:center;transition:all .15s}"
".expr-btn:hover {background:#30363d;border-color:#58a6ff;color:#58a6ff}"
".expr-btn.active {background:#1f3a1f;border-color:#238636;color:#3fb950}"
".input {width:100%;padding:10px 12px;border:1px solid #30363d;border-radius:6px;background:#0d1117;color:#c9d1d9;font-size:14px;margin-bottom:8px}"
".input:focus {outline:none;border-color:#58a6ff}"
"select.input {width:100%}"
"label {display:block;color:#8b949e;font-size:12px;margin-bottom:4px}"
".info-grid {display:grid;grid-template-columns:1fr 1fr;gap:12px}"
".info-item {background:#0d1117;border:1px solid #21262d;border-radius:4px;padding:10px}"
".info-item .label {color:#484f58;font-size:11px;text-transform:uppercase;margin-bottom:4px}"
".info-item .value {color:#c9d1d9;font-size:14px}"
".chat-messages {height:300px;overflow-y:auto;border:1px solid #30363d;border-radius:6px;padding:12px;margin-bottom:12px;background:#0d1117}"
".msg {margin:8px 0;padding:8px 12px;border-radius:6px;font-size:14px;max-width:85%;word-wrap:break-word}"
".msg.user {background:#1f3a1f;border:1px solid #238636;margin-left:auto;color:#3fb950}"
".msg.bot {background:#21262d;border:1px solid #30363d;color:#c9d1d9}"
".status {color:#484f58;font-size:12px;text-align:center;padding:4px}"
".send-row {display:flex;gap:8px}"
".send-row input {flex:1}"
"#poll-msg {display:none}"
".log-box {background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:12px;font-family:monospace;font-size:12px;max-height:200px;overflow-y:auto;white-space:pre-wrap;color:#8b949e}"
".hidden {display:none}"
"</style></head><body>"
"<div class='header'><h1>ZClaw Control</h1><span class='version' id='ver'></span></div>"
"<div class='tabs'>"
"<div class='tab active' data-tab='chat'>Chat</div>"
"<div class='tab' data-tab='expr'>Expressions</div>"
"<div class='tab' data-tab='status'>Status</div>"
"<div class='tab' data-tab='settings'>Settings</div>"
"</div>"
"<div class='content'>"

"<div id='tab-chat' class='tab-content'>"
"<div class='card'>"
"<h3>Chat</h3>"
"<div class='chat-messages' id='msgs'></div>"
"<div class='status' id='chat-status'></div>"
"<div class='send-row'>"
"<input class='input' id='chat-input' placeholder='Type a message...' onkeydown='if(event.key===\"Enter\")sendChat()'/>"
"<button class='btn primary' onclick='sendChat()'>Send</button>"
"</div>"
"</div>"
"</div>"

"<div id='tab-expr' class='tab-content hidden'>"
"<div class='card'>"
"<h3>Set Expression</h3>"
"<div class='expr-grid' id='expr-grid'></div>"
"</div>"
"<div class='card'>"
"<h3>Play Expression Animation</h3>"
"<div class='row'>"
"<select class='input' id='play-expr' style='width:auto;flex:1'>"
"</select>"
"<input class='input' id='play-count' type='number' value='3' min='1' max='20' style='width:80px'/>"
"<span style='color:#8b949e;font-size:13px;align-self:center'>times</span>"
"<input class='input' id='play-delay' type='number' value='500' min='100' max='3000' style='width:80px'/>"
"<span style='color:#8b949e;font-size:13px;align-self:center'>ms delay</span>"
"<button class='btn primary' onclick='playExpr()'>Play</button>"
"</div>"
"</div>"
"</div>"

"<div id='tab-status' class='tab-content hidden'>"
"<div class='card'>"
"<h3>Device Info</h3>"
"<div class='info-grid' id='info-grid'></div>"
"</div>"
"<div class='card'>"
"<h3>Quick Actions</h3>"
"<div class='row'>"
"<button class='btn' onclick='getHealth()'>Health Check</button>"
"<button class='btn' onclick='getDiag()'>Diagnostics</button>"
"<button class='btn' onclick='getVer()'>Version</button>"
"</div>"
"<div class='log-box' id='log-box' style='margin-top:12px'></div>"
"</div>"
"</div>"

"<div id='tab-settings' class='tab-content hidden'>"
"<div class='card'>"
"<h3>LLM Configuration</h3>"
"<label>Backend</label>"
"<select class='input' id='llm-backend'>"
"<option value='openai'>OpenAI</option>"
"<option value='anthropic'>Anthropic</option>"
"<option value='openrouter'>OpenRouter</option>"
"<option value='ollama'>Ollama</option>"
"</select>"
"<label>API URL (optional)</label>"
"<input class='input' id='llm-url' placeholder='Leave empty for default'/>"
"<label>Model</label>"
"<input class='input' id='llm-model' placeholder='e.g. gpt-5.4'/>"
"<label>API Key</label>"
"<input class='input' id='llm-key' type='password' placeholder='Enter API key'/>"
"<div class='row' style='margin-top:12px'>"
"<button class='btn primary' onclick='saveLLM()'>Save LLM Config</button>"
"</div>"
"</div>"
"<div class='card'>"
"<h3>Current Config</h3>"
"<div class='info-grid' id='cfg-grid'></div>"
"</div>"
"</div>"

"</div>"
"<div id='poll-msg'></div>"
"<script>"
"var curExpr=0;"
"var pollInt=null;"
"var chatLog=[];"
"const EXPRS=";

static const char *CONTROL_HTML2 =
";"
"function $(id){return document.getElementById(id)}"
"function init(){"
"  $('ver').textContent='v'+window.zclaw_ver;"
"  var eg=$('expr-grid');"
"  EXPRS.forEach(function(e,i){"
"    eg.innerHTML+=\"<div class='expr-btn' id='eb\"+i+\"' onclick='setExpr(\"+i+\")'>\"+e+\"</div>\";"
"  });"
"  var ps=$('play-expr');"
"  EXPRS.forEach(function(e,i){ps.innerHTML+=\"<option value='\"+i+\"'>\"+e+\"</option>\"});"
"  loadStatus();"
"  loadLLMcfg();"
"}"
"function tab(n){"
"  document.querySelectorAll('.tab').forEach(function(t){t.classList.remove('active')});"
"  document.querySelectorAll('.tab-content').forEach(function(c){c.classList.add('hidden')});"
"  document.querySelector(\"[data-tab='\"+n+\"']\").classList.add('active');"
"  $('tab-'+n).classList.remove('hidden');"
"}"
"document.querySelectorAll('.tab').forEach(function(t){t.onclick=function(){tab(t.dataset.tab)}});"
"function setExpr(i){"
"  curExpr=i;"
"  document.querySelectorAll('.expr-btn').forEach(function(b){b.classList.remove('active')});"
"  $('eb'+i).classList.add('active');"
"  fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},"
"    body:JSON.stringify({name:'set_expression',input:{id:i}})});"
"}"
"function playExpr(){"
"  var id=parseInt($('play-expr').value);"
"  var count=parseInt($('play-count').value)||3;"
"  var delay=parseInt($('play-delay').value)||500;"
"  fetch('/expr/play',{method:'POST',headers:{'Content-Type':'application/json'},"
"    body:JSON.stringify({id:id,count:count,delay_ms:delay})}).then(function(r){return r.json()}).then(function(d){"
"    log(d.result||d.error);"
"  });"
"}"
"function loadStatus(){"
"  fetch('/status').then(function(r){return r.json()}).then(function(d){"
"    var g=$('info-grid');"
"    g.innerHTML='';"
"    Object.keys(d).forEach(function(k){"
"      g.innerHTML+=\"<div class='info-item'><div class='label'>\"+k+\"</div><div class='value'>\"+d[k]+\"</div></div>\";"
"    });"
"  });"
"  fetch('/config/llm').then(function(r){return r.json()}).then(function(d){"
"    var g=$('cfg-grid');"
"    g.innerHTML='';"
"    Object.keys(d).forEach(function(k){"
"      var v=d[k]||'';if(k==='api_key'&&v)v='***';"
"      g.innerHTML+=\"<div class='info-item'><div class='label'>\"+k+\"</div><div class='value'>\"+v+\"</div></div>\";"
"    });"
"  });"
"}"
"function loadLLMcfg(){"
"  fetch('/config/llm').then(function(r){return r.json()}).then(function(d){"
"    if(d.backend)$('llm-backend').value=d.backend;"
"    if(d.api_url)$('llm-url').value=d.api_url||'';"
"    if(d.model)$('llm-model').value=d.model||'';"
"  });"
"}"
"function saveLLM(){"
"  var body={"
"    backend:$('llm-backend').value,"
"    api_url:$('llm-url').value,"
"    model:$('llm-model').value,"
"    api_key:$('llm-key').value"
"  };"
"  fetch('/config/llm',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})"
"    .then(function(r){return r.json()}).then(function(d){"
"      log(d.result||JSON.stringify(d));"
"      if(d.result==='ok'){"
"        $('llm-key').value='';"
"        loadLLMcfg();"
"      }"
"    });"
"}"
"function log(t){$('log-box').textContent=(t||'')+(t?'\\n':'')+$('log-box').textContent;}"
"function getHealth(){log('...');fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:'get_health',input:{}})}).then(function(r){return r.json()}).then(function(d){log(d.result||d.error)});}"
"function getDiag(){log('...');fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:'get_diagnostics',input:{scope:'all'}})}).then(function(r){return r.json()}).then(function(d){log(d.result||d.error)});}"
"function getVer(){log('...');fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:'get_version',input:{}})}).then(function(r){return r.json()}).then(function(d){log(d.result||d.error)});}"
"function addMsg(t,cls){var m=document.createElement('div');m.className='msg '+cls;m.textContent=t;$('msgs').appendChild(m);$('msgs').scrollTop=$('msgs').scrollHeight}"
"function sendChat(){"
"  var t=$('chat-input');var msg=t.value.trim();if(!msg)return;"
"  t.value='';addMsg(msg,'user');$('chat-status').textContent='Thinking...';"
"  fetch('/chat',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'msg='+encodeURIComponent(msg)})"
"    .then(function(){pollInt=setInterval(function(){"
"      fetch('/poll').then(function(r){return r.text()}).then(function(d){"
"        if(d&&d!==' '){clearInterval(pollInt);$('chat-status').textContent='';addMsg(d,'bot')}"
"      });"
"    },500);"
"    });"
"}"
"window.zclaw_ver='2.13.0';init();"
"</script></body></html>";
#endif

// === Handlers ===

static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[HTTP] GET / -> index handler");

    const char *html =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>zclaw</title>"
"<style>"
"*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}"
":root{"
"--bg:#0a0e14;--panel:#111620;--border:#1e2a38;--accent:#00d4aa;"
"--accent2:#0088ff;--text:#c5cdd8;--muted:#5a6a7a;--danger:#ff4757;"
"--input-bg:#0d1420;--btn:#1a2332;--btn-hover:#243044"
"}"
"html,body{height:100%;background:var(--bg);color:var(--text);"
"font-family:'SF Mono','Fira Code','Cascadia Code',monospace;"
"font-size:13px;overflow:hidden}"
".wrap{display:grid;grid-template-columns:320px 1fr;height:100vh}"
".panel-left{padding:20px;border-right:1px solid var(--border);overflow-y:auto}"
".panel-right{display:flex;flex-direction:column;padding:20px}"
".hdr{display:flex;align-items:center;gap:12px;margin-bottom:24px}"
".hdr h1{font-size:16px;font-weight:600;color:var(--accent);letter-spacing:2px}"
".hdr .ver{color:var(--muted);font-size:11px}"
".status-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:20px}"
".stat{background:var(--panel);border:1px solid var(--border);border-radius:8px;"
"padding:10px 12px}"
".stat .l{color:var(--muted);font-size:10px;text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px}"
".stat .v{font-size:15px;font-weight:600;color:var(--text)}"
".stat .v.good{color:#00e676}"
".stat .v.warn{color:#ffab00}"
".stat .v.bad{color:var(--danger)}"
".section{margin-bottom:20px}"
".section h2{font-size:11px;color:var(--muted);text-transform:uppercase;"
"letter-spacing:1px;margin-bottom:10px;padding-bottom:6px;border-bottom:1px solid var(--border)}"
"input,select{width:100%;padding:9px 12px;background:var(--input-bg);"
"border:1px solid var(--border);border-radius:6px;color:var(--text);"
"font-family:inherit;font-size:12px;outline:none;transition:border-color .2s}"
"input:focus,select:focus{border-color:var(--accent)}"
"input::placeholder{color:var(--muted)}"
".row{display:flex;gap:8px}"
".row > *{flex:1}"
"button{width:100%;padding:9px 12px;background:var(--btn);border:1px solid var(--border);"
"border-radius:6px;color:var(--text);font-family:inherit;font-size:12px;"
"cursor:pointer;transition:all .15s}"
"button:hover{background:var(--btn-hover);border-color:var(--accent)}"
"button.primary{background:var(--accent);border-color:var(--accent);color:#000;font-weight:600}"
"button.primary:hover{background:#00eec4}"
".msg-area{flex:1;overflow-y:auto;background:var(--panel);border:1px solid var(--border);"
"border-radius:8px;padding:12px;margin-bottom:12px;min-height:0}"
".msg{margin-bottom:10px;padding:8px 12px;border-radius:8px;max-width:85%;"
"font-size:12px;line-height:1.5;word-break:break-word}"
".msg.user{background:linear-gradient(135deg,#004d3d,#006655);border:1px solid var(--accent);"
"margin-left:auto;color:#00ffcc}"
".msg.bot{background:var(--btn);border:1px solid var(--border);color:var(--text)}"
".msg.error{background:#2a0a0a;border:1px solid var(--danger);color:#ff8080}"
".msg .ts{font-size:10px;opacity:.6;margin-top:4px}"
".chat-input-wrap{display:flex;gap:8px}"
".chat-input-wrap input{flex:1}"
".banner{background:linear-gradient(90deg,var(--panel),transparent);"
"border-left:3px solid var(--accent);padding:8px 12px;margin-bottom:16px;"
"border-radius:0 6px 6px 0;font-size:11px;color:var(--muted)}"
".conn-dot{display:inline-block;width:7px;height:7px;border-radius:50%;"
"background:var(--danger);margin-right:6px;animation:pulse 2s infinite}"
".conn-dot.connected{background:#00e676}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}"
".config-show{font-size:11px;color:var(--muted);margin-top:6px;word-break:break-all}"
".msg.typing{opacity:.6}"
"</style>"
"</head>"
"<body>"
"<div class=\"wrap\">"
"<div class=\"panel-left\">"
"<div class=\"hdr\">"
"<svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2\">"
"<path d=\"M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5\"/>"
"</svg>"
"<h1>ZCLAW</h1>"
"<span class=\"ver\" id=\"ver\">v2.13.0</span>"
"</div>"

"<div class=\"banner\">"
"<span class=\"conn-dot\" id=\"conn-dot\"></span>"
"<span id=\"conn-status\">Connecting...</span>"
"</div>"

"<div class=\"status-grid\">"
"<div class=\"stat\"><div class=\"l\">Heap</div><div class=\"v\" id=\"heap\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">Uptime</div><div class=\"v\" id=\"uptime\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">WiFi</div><div class=\"v\" id=\"wifi\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">Signal</div><div class=\"v\" id=\"rssi\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">Face</div><div class=\"v\" id=\"expr\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">FPS</div><div class=\"v\" id=\"fps\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">CPU</div><div class=\"v\" id=\"cpu\">--</div></div>"
"<div class=\"stat\"><div class=\"l\">LLM</div><div class=\"v\" id=\"llm-status\">--</div></div>"
"</div>"

"<div class=\"section\">"
"<h2>LLM Config</h2>"
"<select id=\"backend\">"
"<option value=\"openai\">OpenAI</option>"
"<option value=\"anthropic\">Anthropic</option>"
"<option value=\"openrouter\">OpenRouter</option>"
"<option value=\"ollama\">Ollama</option>"
"</select>"
"<div style=\"height:8px\"></div>"
"<input id=\"api_url\" placeholder=\"API URL (e.g. https://api.openai.com/v1)\">"
"<div style=\"height:8px\"></div>"
"<input id=\"model\" placeholder=\"Model (e.g. gpt-4o)\">"
"<div style=\"height:8px\"></div>"
"<input id=\"api_key\" type=\"password\" placeholder=\"API Key\">"
"<div style=\"height:6px\"></div>"
"<input id=\"search_api_key\" type=\"password\" placeholder=\"Search API Key (Exa.ai, optional)\">"
"<div style=\"height:10px\"></div>"
"<button class=\"primary\" onclick=\"saveLLM()\">Save Config</button>"
"<div id=\"cfg-show\" class=\"config-show\"></div>"
"</div>"

"<div class=\"section\">"
"<h2>Expression</h2>"
"<select id=\"expr-sel\"></select>"
"<div style=\"height:8px\"></div>"
"<div style=\"display:flex;gap:8px\">"
"<button onclick=\"setExpr()\">Set</button>"
"<button onclick=\"playExpr()\">Play</button>"
"</div>"
"<div style=\"height:8px\"></div>"
"<div style=\"display:flex;gap:8px;align-items:center\">"
"<input id=\"play-count\" type=\"number\" value=\"3\" min=\"1\" max=\"20\" "
"style=\"width:70px;flex:none\">"
"<span style=\"color:var(--muted);font-size:11px\">times</span>"
"<input id=\"play-delay\" type=\"number\" value=\"500\" min=\"100\" max=\"3000\" "
"style=\"width:70px;flex:none\">"
"<span style=\"color:var(--muted);font-size:11px\">ms delay</span>"
"</div>"
"</div>"

"<div class=\"section\">"
"<h2>Quick Actions</h2>"
"<div class=\"row\">"
"<button onclick=\"sendTool('get_health',{})()\">Health</button>"
"<button onclick=\"sendTool('get_version',{})()\">Version</button>"
"</div>"
"<div style=\"height:8px\"></div>"
"<div id=\"diag-log\" style=\"background:var(--panel);border:1px solid var(--border);"
"border-radius:6px;padding:8px;font-size:11px;color:var(--muted);"
"max-height:80px;overflow-y:auto;white-space:pre-wrap\"></div>"
"</div>"
"</div>"

"<div class=\"panel-right\">"
"<div class=\"section\" style=\"margin-bottom:12px\">"
"<h2>Chat</h2>"
"</div>"
"<div class=\"msg-area\" id=\"msgs\"></div>"
"<div class=\"chat-input-wrap\">"
"<input id=\"chat-input\" placeholder=\"Send a message...\" "
"onkeydown=\"if(event.key==='Enter')sendChat()\">"
"<button class=\"primary\" onclick=\"sendChat()\" style=\"width:auto;padding:9px 20px\">"
"<svg width=\"14\" height=\"14\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\">"
"<line x1=\"22\" y1=\"2\" x2=\"11\" y2=\"13\"/><polygon points=\"22 2 15 22 11 13 2 9 22 2\"/>"
"</svg>"
"</button>"
"</div>"
"</div>"
"</div>"

"<script>"
"var pollTimer=null;"
"var lastResp='';"
"var typingId=null;"
"var EXPRS=['neutral','happy','wink','surprised','sleepy','thinking','suspicious','cry','oops','sad'];"

"function $(id){return document.getElementById(id)}"

"function fmt(n){if(n>=1e6)return(n/1e6).toFixed(1)+'M';if(n>=1e3)return(n/1e3).toFixed(1)+'K';return String(n)}"
"function fmtUptime(ms){var s=Math.floor(ms/1000);var m=Math.floor(s/60);var h=Math.floor(m/60);"
"if(h>0)return h+'h '+m%60+'m';if(m>0)return m+'m '+s%60+'s';return s+'s'}"

"function addMsg(text,cls,ts){"
"var d=document.createElement('div');d.className='msg '+cls;d.textContent=text;"
"if(ts){var t=document.createElement('div');t.className='ts';t.textContent=new Date().toLocaleTimeString();d.appendChild(t)}"
"$('msgs').appendChild(d);$('msgs').scrollTop=$('msgs').scrollHeight;return d}"

"function setTyping(show){if(show){if(!typingId){typingId=addMsg('typing...','bot');var dots=0;"
"typingId.timer=setInterval(function(){dots=(dots+1)%4;"
"typingId.childNodes[0].textContent='thinking'+\".\".repeat(dots+1)},400)}}"
"else{clearInterval(typingId.timer);if(typingId&&typingId.parentNode){typingId.parentNode.removeChild(typingId)}typingId=null}}"

"function saveLLM(){var b={backend:$('backend').value,api_url:$('api_url').value,"
"model:$('model').value,api_key:$('api_key').value,search_api_key:$('search_api_key').value};"
"fetch('/config/llm',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(b)}).then(function(r){return r.json()}).then(function(d){"
"if(d.result==='ok'){addMsg('Config saved','bot');$('api_key').value='';$('search_api_key').value='';loadLLMCfg()}"
"else{addMsg('Save failed: '+JSON.stringify(d),'error')}}).catch(function(e){addMsg('Error: '+e,'error')})}"

"function loadLLMCfg(){fetch('/config/llm').then(function(r){return r.json()}).then(function(d){"
"$('backend').value=d.backend||'openai';$('api_url').value=d.api_url||'';"
"$('model').value=d.model||'';if(d.api_key){var s=d.api_key;s.length>8?s='****'+s.slice(-4):s='****';"
"$('cfg-show').textContent='Current: '+d.backend+' | '+d.model+' | '+s}else{$('cfg-show').textContent=''}"
"var llm=d.backend||'?';$('llm-status').textContent=llm}).catch(function(){$('llm-status').textContent='ERR'})}"

"function loadStatus(){fetch('/status').then(function(r){return r.json()}).then(function(d){"
"var h=d.free_heap;var v=$('heap');v.textContent=fmt(h);v.className='v '+(h>80000?'good':h>40000?'warn':'bad');"
"$('uptime').textContent=fmtUptime(d.uptime_ms);$('wifi').textContent=d.wifi_ssid||'--';"
"var rssi=d.rssi;var r=$('rssi');r.textContent=rssi||'--';r.className='v '+(rssi>-50?'good':rssi>-70?'warn':'bad');"
"$('expr').textContent=d.expression||'--';$('fps').textContent=(d.fps!=null?d.fps:'--');$('cpu').textContent=(d.cpu!=null?d.cpu+'%':'--');$('ver').textContent='v'+(d.version||'?');"
"$('conn-dot').className='conn-dot connected';$('conn-status').textContent='Online'}).catch(function(){"
"$('conn-dot').className='conn-dot';$('conn-status').textContent='Offline'})}"

"function sendTool(name,inp){return function(){"
"setTyping(true);fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({name:name,input:inp})}).then(function(r){return r.json()}).then(function(d){"
"setTyping(false);var t=$('diag-log');t.textContent=(d.result||d.error||JSON.stringify(d));"
"t.scrollTop=t.scrollHeight}).catch(function(e){setTyping(false);addMsg('Tool error: '+e,'error')})}}"

"function sendChat(){var inp=$('chat-input');var msg=inp.value.trim();if(!msg)return;"
"inp.value='';addMsg(msg,'user');setTyping(true);"
"fetch('/chat',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
"body:'msg='+encodeURIComponent(msg)}).then(function(){startPoll()}).catch(function(e){"
"setTyping(false);addMsg('Send error: '+e,'error')})}"

"function startPoll(){if(pollTimer)return;pollTimer=setInterval(function(){"
"fetch('/poll').then(function(r){return r.text()}).then(function(d){"
"if(d&&d.trim()&&d.trim()!==lastResp&&d.trim()!==' '){"
"clearInterval(pollTimer);pollTimer=null;setTyping(false);lastResp=d.trim();addMsg(lastResp,'bot')}})"
"},500)}"

"function setExpr(){var id=parseInt($('expr-sel').value)||0;"
"fetch('/tool',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({name:'set_expression',input:{id:id}})}).then(function(r){return r.json()}).then(function(d){"
"var t=$('diag-log');t.textContent=d.result||d.error||JSON.stringify(d);t.scrollTop=t.scrollHeight;"
"loadStatus()}).catch(function(e){addMsg('Error: '+e,'error')})}"

"function playExpr(){var id=parseInt($('expr-sel').value)||0;"
"var count=parseInt($('play-count').value)||3;"
"var delay=parseInt($('play-delay').value)||500;"
"fetch('/expr/play',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({id:id,count:count,delay_ms:delay})}).then(function(r){return r.json()}).then(function(d){"
"var t=$('diag-log');t.textContent=d.result||d.error||JSON.stringify(d);t.scrollTop=t.scrollHeight;"
"loadStatus()}).catch(function(e){addMsg('Error: '+e,'error')})}"

"function init(){"
"EXPRS.forEach(function(e,i){var o=document.createElement('option');o.value=i;o.text=e;$('expr-sel').appendChild(o)});"
"loadStatus();loadLLMCfg();setInterval(loadStatus,5000);"
"addMsg('zclaw controller ready','bot');addMsg('Configure LLM above, then type a message','bot')}"

"window.onload=init;"
"</script>"
"</body>"
"</html>";

    httpd_resp_set_type(req, "text/html");
    esp_err_t err = httpd_resp_send(req, html, strlen(html));
    ESP_LOGI(TAG, "[HTTP] GET / -> sent %d bytes", strlen(html));
    return err;
}

static esp_err_t chat_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[HTTP] POST /chat");
    char buf[512] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        ESP_LOGW(TAG, "[HTTP] POST /chat -> recv failed, len=%d", len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    char *msg = strstr(buf, "msg=");
    if (!msg) {
        ESP_LOGW(TAG, "[HTTP] POST /chat -> missing msg param");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing msg");
        return ESP_FAIL;
    }
    msg += 4;

    // URL decode
    char decoded[256];
    int di = 0;
    for (int i = 0; msg[i] && di < 255; i++) {
        if (msg[i] == '+') decoded[di++] = ' ';
        else if (msg[i] == '%' && msg[i+1] && msg[i+2]) {
            char hex[3] = {msg[i+1], msg[i+2], 0};
            decoded[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else decoded[di++] = msg[i];
    }
    decoded[di] = '\0';

    if (s_input_queue) {
        channel_msg_t msg_q;
        strncpy(msg_q.text, decoded, sizeof(msg_q.text) - 1);
        msg_q.text[sizeof(msg_q.text) - 1] = '\0';
        msg_q.source = MSG_SOURCE_CHANNEL;
        msg_q.chat_id = 0;
        if (xQueueSend(s_input_queue, &msg_q, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "[HTTP] POST /chat -> queue send failed");
        }
    } else {
        ESP_LOGW(TAG, "[HTTP] POST /chat -> input_queue is NULL!");
    }

    httpd_resp_set_type(req, "text/plain");
    esp_err_t err = httpd_resp_send(req, "ok", 2);
    ESP_LOGI(TAG, "[HTTP] POST /chat -> sent ok, err=%s", esp_err_to_name(err));
    return err;
}

static esp_err_t poll_handler(httpd_req_t *req)
{
    const char *resp = channel_get_last_response();
    if (!resp || !*resp) resp = " ";
    ESP_LOGI(TAG, "[HTTP] GET /poll -> resp_len=%d", (int)strlen(resp));
    httpd_resp_set_type(req, "text/plain");
    esp_err_t err = httpd_resp_send(req, resp, strlen(resp));
    ESP_LOGI(TAG, "[HTTP] GET /poll -> sent, err=%s", esp_err_to_name(err));
    return err;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[HTTP] GET /status");
    // Get device status as JSON
    cJSON *root = cJSON_CreateObject();

    // Free heap
    uint32_t free_heap = esp_get_free_heap_size();
    cJSON_AddNumberToObject(root, "free_heap", free_heap);

    // WiFi info
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        cJSON_AddStringToObject(root, "wifi_ssid", (const char*)ap.ssid);
        cJSON_AddNumberToObject(root, "rssi", (int)ap.rssi);
    } else {
        ESP_LOGW(TAG, "[HTTP] GET /status -> esp_wifi_sta_get_ap_info failed");
    }

    // Current expression
    int expr = display_get_expr();
    cJSON_AddStringToObject(root, "expression", EXPR_NAMES[expr]);

    // Uptime
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", uptime_ms);

    // LVGL FPS and CPU
    uint32_t fps_avg = lvgl_fps_avg_get();
    uint8_t cpu_usage = display_cpu_usage_get();
    cJSON_AddNumberToObject(root, "fps", fps_avg);
    cJSON_AddNumberToObject(root, "cpu", cpu_usage);

    // Version
    cJSON_AddStringToObject(root, "version", "2.13.0");

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, strlen(json));
    ESP_LOGI(TAG, "[HTTP] GET /status -> sent %d bytes, err=%s", (int)strlen(json), esp_err_to_name(err));
    free(json);
    cJSON_Delete(root);
    return err;
}

static esp_err_t debug_handler(httpd_req_t *req)
{
    cJSON *state = agent_get_debug_state();
    if (!state) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get debug state");
        return ESP_FAIL;
    }
    
    const char *last_resp = channel_get_last_response();
    cJSON_AddStringToObject(state, "last_response", last_resp && last_resp[0] ? last_resp : "(none)");
    
    char *json = cJSON_PrintUnformatted(state);
    cJSON_Delete(state);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to format debug state");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, strlen(json));
    free(json);
    return err;
}

static esp_err_t tool_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[HTTP] POST /tool");
    char buf[1024] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        ESP_LOGW(TAG, "[HTTP] POST /tool -> recv failed, len=%d", len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';
    ESP_LOGI(TAG, "[HTTP] POST /tool -> recv %d bytes: %.80s...", len, buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "[HTTP] POST /tool -> JSON parse failed");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    cJSON *input_item = cJSON_GetObjectItem(root, "input");
    if (!name_item || !cJSON_IsString(name_item)) {
        ESP_LOGW(TAG, "[HTTP] POST /tool -> missing name field");
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[HTTP] POST /tool -> executing tool: %s", name_item->valuestring);

    char result[512];
    bool ok = tools_execute(name_item->valuestring, input_item, result, sizeof(result));

    ESP_LOGI(TAG, "[HTTP] POST /tool -> result: ok=%d, msg=%.100s", ok, result);

    cJSON *resp = cJSON_CreateObject();
    if (ok) {
        cJSON_AddStringToObject(resp, "result", result);
    } else {
        cJSON_AddStringToObject(resp, "error", result);
    }

    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, strlen(json));
    ESP_LOGI(TAG, "[HTTP] POST /tool -> sent resp %d bytes, err=%s", (int)strlen(json), esp_err_to_name(err));
    free(json);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return err;
}

static esp_err_t expr_play_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    cJSON *count_item = cJSON_GetObjectItem(root, "count");
    cJSON *delay_item = cJSON_GetObjectItem(root, "delay_ms");

    int id = id_item ? cJSON_GetObjectItem(root, "id")->valueint : 0;
    int count = count_item ? count_item->valueint : 3;
    int delay_ms = delay_item ? delay_item->valueint : 500;

    if (id < 0 || id >= EXPR_COUNT) id = 0;
    if (count < 1) count = 1;
    if (count > 20) count = 20;
    if (delay_ms < 100) delay_ms = 100;
    if (delay_ms > 3000) delay_ms = 3000;

    cJSON_Delete(root);

    char result[128];

    // Save current (set) expression before playing
    int saved_expr = display_get_expr();

    for (int i = 0; i < count; i++) {
        // Trigger expression or animation
        if (id == EXPR_THINKING || id == EXPR_SUSPICIOUS) {
            // thinking/suspicious: use display_set_expr (5-phase animation)
            display_set_expr(id);
            // Thinking/suspicious: display task manages the 5-phase animation loop (3.5s),
            // wait for display_is_thinking() to go false
            while (display_is_thinking()) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            // Non-thinking/non-suspicious: use display_play_expr (3-phase animation)
            display_play_expr(id);
            // Normal expression: display_play_expr manages 3-phase animation (2s total),
            // wait for it to complete before next iteration
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        // delay_ms: hold this expression for the specified interval before next cycle
        if (i + 1 < count && delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    // Restore saved expression after all cycles done
    display_set_expr(saved_expr);
    vTaskDelay(pdMS_TO_TICKS(300));

    snprintf(result, sizeof(result), "Played %s x%d (interval %dms), restored to %s",
             EXPR_NAMES[id], count, delay_ms, EXPR_NAMES[saved_expr]);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "result", result);
    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(resp);
    return ESP_OK;
}

static esp_err_t config_llm_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    char val[256];
    
    // Backend
    if (memory_get("llm_backend", val, sizeof(val))) {
        cJSON_AddStringToObject(root, "backend", val);
    } else {
        cJSON_AddStringToObject(root, "backend", "openai");
    }
    
    // API URL
    if (memory_get("llm_api_url", val, sizeof(val))) {
        cJSON_AddStringToObject(root, "api_url", val);
    }
    
    // Model
    if (memory_get("llm_model", val, sizeof(val))) {
        cJSON_AddStringToObject(root, "model", val);
    } else {
        cJSON_AddStringToObject(root, "model", "gpt-5.4");
    }
    
    // API key (masked)
    if (memory_get("api_key", val, sizeof(val))) {
        if (strlen(val) > 4) {
            char masked[64];
            int ml = strlen(val) - 4;
            memset(masked, '*', ml);
            masked[ml] = '\0';
            strcat(masked, val + ml);
            cJSON_AddStringToObject(root, "api_key", masked);
        } else {
            cJSON_AddStringToObject(root, "api_key", "****");
        }
    }

    // Search API key (Exa) - masked
    if (memory_get("exa_api_key", val, sizeof(val))) {
        if (strlen(val) > 4) {
            char masked[64];
            int ml = strlen(val) - 4;
            memset(masked, '*', ml);
            masked[ml] = '\0';
            strcat(masked, val + ml);
            cJSON_AddStringToObject(root, "search_api_key", masked);
        } else {
            cJSON_AddStringToObject(root, "search_api_key", "****");
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t config_llm_set_handler(httpd_req_t *req)
{
    char buf[1024] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *backend = cJSON_GetObjectItem(root, "backend");
    cJSON *api_url = cJSON_GetObjectItem(root, "api_url");
    cJSON *model = cJSON_GetObjectItem(root, "model");
    cJSON *api_key = cJSON_GetObjectItem(root, "api_key");
    cJSON *search_api_key = cJSON_GetObjectItem(root, "search_api_key");

    if (backend && cJSON_IsString(backend)) {
        memory_set("llm_backend", backend->valuestring);
    }
    if (api_url && cJSON_IsString(api_url) && strlen(api_url->valuestring) > 0) {
        char trimmed[256];
        int in_len = strlen(api_url->valuestring);
        if (in_len >= (int)sizeof(trimmed)) in_len = sizeof(trimmed) - 1;
        int start = 0, end = in_len - 1;
        while (start < in_len && (api_url->valuestring[start] == ' ' || api_url->valuestring[start] == '\t'))
            start++;
        while (end >= start && (api_url->valuestring[end] == ' ' || api_url->valuestring[end] == '\t'))
            end--;
        int out_len = end - start + 1;
        memcpy(trimmed, api_url->valuestring + start, out_len);
        trimmed[out_len] = '\0';
        memory_set("llm_api_url", trimmed);
    }
    if (model && cJSON_IsString(model) && strlen(model->valuestring) > 0) {
        memory_set("llm_model", model->valuestring);
    }
    if (api_key && cJSON_IsString(api_key) && strlen(api_key->valuestring) > 0) {
        char trimmed[256];
        int in_len = strlen(api_key->valuestring);
        if (in_len >= (int)sizeof(trimmed)) in_len = sizeof(trimmed) - 1;
        int start = 0, end = in_len - 1;
        while (start < in_len && (api_key->valuestring[start] == ' ' || api_key->valuestring[start] == '\t'))
            start++;
        while (end >= start && (api_key->valuestring[end] == ' ' || api_key->valuestring[end] == '\t'))
            end--;
        int out_len = end - start + 1;
        memcpy(trimmed, api_key->valuestring + start, out_len);
        trimmed[out_len] = '\0';
        memory_set("api_key", trimmed);
    }
    if (search_api_key && cJSON_IsString(search_api_key) && strlen(search_api_key->valuestring) > 0) {
        char trimmed[256];
        int in_len = strlen(search_api_key->valuestring);
        if (in_len >= (int)sizeof(trimmed)) in_len = sizeof(trimmed) - 1;
        int start = 0, end = in_len - 1;
        while (start < in_len && (search_api_key->valuestring[start] == ' ' || search_api_key->valuestring[start] == '\t'))
            start++;
        while (end >= start && (search_api_key->valuestring[end] == ' ' || search_api_key->valuestring[end] == '\t'))
            end--;
        int out_len = end - start + 1;
        memcpy(trimmed, search_api_key->valuestring + start, out_len);
        trimmed[out_len] = '\0';
        memory_set("exa_api_key", trimmed);
    }

    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "result", "ok");
    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(resp);
    return ESP_OK;
}

// Catch-all 404 handler - logs ALL requests that don't match registered URIs
static esp_err_t not_found_handler(httpd_req_t *req)
{
    int sockfd = httpd_req_to_sockfd(req);
    ESP_LOGW(TAG, "[HTTP] 404 NOT FOUND: sockfd=%d URI=%s", sockfd, req->uri);
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "404 Not Found", 13);
    return ESP_OK;  // Return OK so socket stays open for next request
}

esp_err_t http_channel_start(QueueHandle_t input_queue)
{
    s_input_queue = input_queue;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;
    // Increase recv timeout so slow clients don't get truncated
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    ESP_LOGI(TAG, "[HTTP] Starting server on port 80, recv_timeout=%d, send_timeout=%d, max_handlers=%d",
             config.recv_wait_timeout, config.send_wait_timeout, config.max_uri_handlers);

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[HTTP] Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "[HTTP] Server started, registering URI handlers...");

    // Register each URI and log result
    {
        httpd_uri_t uris[] = {
            {"/", HTTP_GET, index_handler, NULL},
            {"/chat", HTTP_POST, chat_handler, NULL},
            {"/poll", HTTP_GET, poll_handler, NULL},
            {"/status", HTTP_GET, status_handler, NULL},
            {"/tool", HTTP_POST, tool_handler, NULL},
            {"/expr/play", HTTP_POST, expr_play_handler, NULL},
            {"/config/llm", HTTP_GET, config_llm_get_handler, NULL},
            {"/config/llm", HTTP_POST, config_llm_set_handler, NULL},
            {"/debug", HTTP_GET, debug_handler, NULL},
            {"/*", HTTP_GET, not_found_handler, NULL},
            {"/*", HTTP_POST, not_found_handler, NULL},
            {"/*", HTTP_PUT, not_found_handler, NULL},
            {"/*", HTTP_DELETE, not_found_handler, NULL},
        };
        int uri_count = sizeof(uris) / sizeof(uris[0]);
        for (int i = 0; i < uri_count; i++) {
            esp_err_t r = httpd_register_uri_handler(server, &uris[i]);
            if (r != ESP_OK) {
                ESP_LOGW(TAG, "[HTTP] FAILED to register %s: %s", uris[i].uri, esp_err_to_name(r));
            } else {
                ESP_LOGI(TAG, "[HTTP] Registered: %s", uris[i].uri);
            }
        }
    }

    ESP_LOGI(TAG, "[HTTP] Server startup complete");
    return ESP_OK;
}
