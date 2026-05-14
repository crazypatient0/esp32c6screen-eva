#include "http_channel.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "messages.h"
#include "channel.h"
#include <string.h>

static const char *TAG = "http_channel";
static httpd_handle_t server = NULL;
static QueueHandle_t s_input_queue = NULL;
static char last_response[1024] = "";

// Response tracking
struct chat_req {
    char msg[256];
    char resp[512];
    bool done;
};

// Chat page HTML
static const char *CHAT_HTML = 
"<!DOCTYPE html><html><head>"
"<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>*{margin:0;padding:0;box-sizing:border-box}body{background:#000;color:#87CEEB;font-family:'Segoe UI',sans-serif;display:flex;flex-direction:column;height:100vh}"
"#chat{flex:1;overflow-y:auto;padding:16px}.msg{margin:8px 0;padding:12px;border-radius:10px;max-width:85%;word-wrap:break-word;font-size:16px;line-height:1.4}"
".user{background:#1a3a4a;margin-left:auto;color:#fff}.bot{background:#0a1a20;margin-right:auto;color:#87CEEB;border:1px solid #1a5a6a}"
"#input{display:flex;padding:12px;gap:8px;border-top:1px solid #1a5a6a;background:#050d10}"
"#input input{flex:1;padding:14px;border:1px solid #1a5a6a;border-radius:8px;background:#0a1a20;color:#87CEEB;font-size:16px;outline:none}"
"#input button{padding:14px 20px;border:none;border-radius:8px;background:#1a5a6a;color:#fff;font-size:16px;font-weight:bold;cursor:pointer}"
".status{text-align:center;color:#4a8a9a;font-size:13px;padding:4px;font-style:italic}"
"</style></head><body>"
"<div id='chat'><div class='msg bot'>Hi! I'm your desktop pet. How are you feeling?</div></div>"
"<div class='status' id='status'></div>"
"<div id='input'><input id='txt' placeholder='Type a message...'/><button onclick='send()'>Send</button></div>"
"<script>"
"function addMsg(text,cls){var d=document.getElementById('chat');var m=document.createElement('div');m.className='msg '+cls;m.textContent=text;d.appendChild(m);d.scrollTop=d.scrollHeight}"
"document.getElementById('txt').addEventListener('keydown',function(e){if(e.key==='Enter')send()})"
"var pollTimer;function poll(){var x=new XMLHttpRequest();x.open('GET','/poll',true);"
"x.onload=function(){if(x.responseText&&x.responseText!=' '){addMsg(x.responseText,'bot');document.getElementById('status').textContent='';clearInterval(pollTimer)}};x.send()}"
"function send(){"
"var t=document.getElementById('txt');var msg=t.value.trim();if(!msg)return;"
"t.value='';addMsg(msg,'user');document.getElementById('status').textContent='Thinking...';"
"var x=new XMLHttpRequest();x.open('POST','/chat',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
"x.onload=function(){document.getElementById('status').textContent='Thinking...';pollTimer=setInterval(poll,1000)};"
"x.send('msg='+encodeURIComponent(msg))"
"}"
"</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, CHAT_HTML, strlen(CHAT_HTML));
    return ESP_OK;
}

static esp_err_t chat_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    // Parse msg= parameter
    char *msg = strstr(buf, "msg=");
    if (!msg) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing msg");
        return ESP_FAIL;
    }
    msg += 4;

    // URL decode (simple: just convert + to space)
    char decoded[256];
    int di = 0;
    for (int i = 0; msg[i] && di < 255; i++) {
        if (msg[i] == '+') decoded[di++] = ' ';
        else if (msg[i] == '%' && msg[i+1] && msg[i+2]) {
            // Basic hex decode
            char hex[3] = {msg[i+1], msg[i+2], 0};
            decoded[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else decoded[di++] = msg[i];
    }
    decoded[di] = '\0';

    ESP_LOGI(TAG, "Chat msg: %s", decoded);

    // Send to agent as proper channel_msg_t
    if (s_input_queue) {
        channel_msg_t msg;
        strncpy(msg.text, decoded, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.source = MSG_SOURCE_CHANNEL;
        msg.chat_id = 0;
        xQueueSend(s_input_queue, &msg, pdMS_TO_TICKS(100));
    }

    // Return confirmation (response will be polled via /poll)
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "ok", 2);
    return ESP_OK;
}

// GET /poll - return the latest agent response
static esp_err_t poll_handler(httpd_req_t *req)
{
    const char *resp = channel_get_last_response();
    if (!resp || !*resp) {
        resp = " ";
    }
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

esp_err_t http_channel_start(QueueHandle_t input_queue)
{
    s_input_queue = input_queue;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 6;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/", .method = HTTP_GET, .handler = index_handler
    });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/chat", .method = HTTP_POST, .handler = chat_handler
    });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/poll", .method = HTTP_GET, .handler = poll_handler
    });

    ESP_LOGI(TAG, "HTTP chat server started on port 80");
    return ESP_OK;
}
