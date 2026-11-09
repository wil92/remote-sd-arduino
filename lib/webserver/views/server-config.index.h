
#ifndef SERVER_CONFIG_INDEX_HTML_H
#define SERVER_CONFIG_INDEX_HTML_H

const char server_config_index_html[] PROGMEM = R"PAGE(
<!doctype html><title>Device configuration page</title><body><h1>Configuration Page</h1><h2>Network</h2><div><label for=ssid>SSID</label><input id=ssid value=$$SSID$$></div><div><label for=password>Password</label><input id=password value=$$PASSWORD$$></div><button id=summit style=margin-top:20px>Configure device</button></body><script>var but=document.getElementById(`summit`),ssid=document.getElementById(`ssid`),password=document.getElementById(`password`);function toQueryString(obj){return Object.keys(obj).map(key=>`${encodeURIComponent(key)}=${encodeURIComponent(obj[key])}`).join(`&`)}but.onclick=function(){var queries={ssid:ssid.value,password:password.value};fetch(`/?${toQueryString(queries)}`,{method:`post`}).then(()=>console.log(`Device configurated`)).catch(e=>console.error(e))};function removeDefaultValue(ele,isCheckbox=!1){ele.value.startsWith(`$$`)&&ele.value.endsWith(`$$`)?(ele.value=``,isCheckbox&&(ele.checked=!0)):isCheckbox&&(ele.checked=ele.value===`true`,ele.value=``)}(function(){removeDefaultValue(ssid),removeDefaultValue(password)})();</script>
)PAGE";

#endif //SERVER_CONFIG_INDEX_HTML_H
