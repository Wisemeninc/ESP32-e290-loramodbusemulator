#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

// Common HTML Header
const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Vision Master E290</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}
.container{max-width:900px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}
h1{color:#2c3e50;margin-top:0;border-bottom:3px solid #3498db;padding-bottom:15px;}
h2{color:#34495e;margin-top:30px;}
.nav{background:#3498db;padding:15px;margin:-30px -30px 30px -30px;border-radius:10px 10px 0 0;display:flex;align-items:center;}
.nav a{color:white;text-decoration:none;padding:10px 20px;margin:0 5px;background:#2980b9;border-radius:5px;display:inline-block;}
.nav a:hover{background:#21618c;}
.nav .reboot{margin-left:auto;background:#e74c3c;}
.nav .reboot:hover{background:#c0392b;}
.card{background:#ecf0f1;padding:20px;margin:15px 0;border-radius:8px;border-left:4px solid #3498db;}
.info{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0;}
.info-item{background:#fff;padding:15px;border-radius:5px;border:1px solid #ddd;}
.info-label{font-size:12px;color:#7f8c8d;text-transform:uppercase;margin-bottom:5px;}
.info-value{font-size:24px;font-weight:bold;color:#2c3e50;}
form{background:#ecf0f1;padding:20px;border-radius:8px;margin:20px 0;}
label{display:block;margin-bottom:8px;color:#2c3e50;font-weight:600;}
input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;border:2px solid #bdc3c7;border-radius:5px;font-size:16px;box-sizing:border-box;margin-bottom:15px;}
input[type=text]:focus,input[type=password]:focus,input[type=number]:focus,select:focus{border-color:#3498db;outline:none;}
input[type=submit],button{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px;}
input[type=submit]:hover,button:hover{background:#229954;}
.footer{text-align:center;margin-top:30px;color:#7f8c8d;font-size:14px;}
table{border-collapse:collapse;width:100%;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
th{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:12px;text-align:left;font-weight:600;}
td{border:1px solid #e0e0e0;padding:10px;background:white;}
tr:nth-child(even) td{background:#f8f9fa;}
tr:hover td{background:#e3f2fd;}
.value{font-weight:bold;color:#2c3e50;}
.warning{background:#fff3cd;border:1px solid #ffc107;padding:15px;border-radius:5px;margin:20px 0;color:#856404;}
.spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;display:inline-block;vertical-align:middle;}
@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
</style>
</head><body>
<div class='container'>
<div class='nav'>
<a href='/'>Home</a>
<a href='/stats'>Statistics</a>
<a href='/registers'>Registers</a>
<a href='/lorawan'>LoRaWAN</a>
<a href='/lorawan/profiles'>Profiles</a>
<a href='/wifi'>WiFi</a>
<a href='/security'>Security</a>
<a href='/reboot' class='reboot' onclick='return confirm("Are you sure you want to reboot the device?");'>Reboot</a>
</div>
)rawliteral";

// Common HTML Footer
const char HTML_FOOTER[] PROGMEM = R"rawliteral(
<div class='footer'>ESP32-S3R8 | Modbus RTU/TCP + E-Ink Display | Arduino Framework</div>
</div></body></html>
)rawliteral";

// Redirect Page Template
const char HTML_REDIRECT[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Redirecting...</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<meta http-equiv='refresh' content='%d;url=%s'>
<style>
body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}
.container{max-width:600px;margin:50px auto;background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);text-align:center;}
h1{color:#27ae60;}
p{color:#7f8c8d;font-size:16px;margin:20px 0;}
.spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:20px auto;}
@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
</style></head><body><div class='container'>
<h1>%s</h1>
<div class='spinner'></div>
<p>%s</p>
<p>You will be redirected in %d seconds.</p>
</div></body></html>
)rawliteral";

// WiFi Page Scripts and Styles
const char WIFI_PAGE_ASSETS[] PROGMEM = R"rawliteral(
<script>
function scanNetworks(){
  document.getElementById('scanBtn').disabled=true;
  document.getElementById('scanBtn').innerHTML='<span class="spinner" style="width:20px;height:20px;border-width:3px;"></span> Scanning...';
  fetch('/wifi/scan').then(r=>r.json()).then(data=>{
    let html='';
    data.networks.forEach(n=>{
      html+='<div class="network" onclick="selectNetwork(\''+n.ssid+'\')"><strong>'+n.ssid+'</strong><span class="rssi">'+n.rssi+' dBm</span></div>';
    });
    document.getElementById('scanResults').innerHTML=html;
    document.getElementById('scanBtn').disabled=false;
    document.getElementById('scanBtn').innerHTML='Scan for Networks';
  }).catch(e=>{
    document.getElementById('scanResults').innerHTML='<p style="color:red;">Scan failed</p>';
    document.getElementById('scanBtn').disabled=false;
    document.getElementById('scanBtn').innerHTML='Scan for Networks';
  });
}
function selectNetwork(ssid){
  document.getElementById('ssid').value=ssid;
}
function refreshStatus(){
  fetch('/wifi/status').then(r=>r.json()).then(data=>{
    let statusHtml='';
    if(data.client_connected){
      statusHtml='<div class="status"><strong>Connected to WiFi</strong><br>SSID: '+data.client_ssid+'<br>IP Address: '+data.client_ip+'<br>Signal: '+data.client_rssi+' dBm</div>';
    }else if(data.saved_ssid){
      statusHtml='<div class="status disconnected"><strong>Not Connected</strong><br>Saved network: '+data.saved_ssid+'<br>Will try to connect on next boot</div>';
    }else{
      statusHtml='<div class="status disconnected"><strong>No WiFi Configured</strong><br>Use the form below to connect to a WiFi network</div>';
    }
    document.getElementById('statusArea').innerHTML=statusHtml;
  });
}
window.onload=function(){refreshStatus();};
</script>
<style>
.status{padding:15px;margin:20px 0;border-radius:8px;background:#d4edda;border:1px solid #c3e6cb;color:#155724;}
.status.disconnected{background:#f8d7da;border-color:#f5c6cb;color:#721c24;}
#scanResults{margin-top:15px;}
.network{background:white;padding:12px;margin:8px 0;border-radius:5px;border:1px solid #ddd;cursor:pointer;}
.network:hover{background:#e3f2fd;border-color:#3498db;}
.network strong{color:#2c3e50;}
.network .rssi{color:#7f8c8d;float:right;}
</style>
)rawliteral";

#endif // WEB_PAGES_H