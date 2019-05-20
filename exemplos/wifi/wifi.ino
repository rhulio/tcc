#include <WiFi.h>
#include <HTTPClient.h>

#include <BluetoothSerial.h>

BluetoothSerial OBD;

#include <MQTT.h>

WiFiClient net;
MQTTClient mqtt;

const char* ssid = "# Cipriano";
const char* senha = "globo321";

const String token = "438C1C";

String float2str(float x, byte precision = 2) {
  char tmp[50];
  dtostrf(x, 0, precision, tmp);
  return String(tmp);
}

bool conectaWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Conectando...");
    delay(250);
    return 0;
  } else
    return 1;
}

String WiFiGET(String link) {
  if (!conectaWiFi()) return "";

  HTTPClient http; // Inicia biblioteca HTTPClient.
  http.begin(link);
  http.setTimeout(2000);

  int httpCode = http.GET(); // Recebe o código que o servidor retornou.

  if (httpCode == HTTP_CODE_OK) { // Se a conexão obtiver sucesso, executa o código abaixo.
    String resposta = http.getString(); // Recebe o conteúdo da página.
    http.end();
    //Serial.println(resposta);
    return resposta;
  }

  http.end(); // Encerra conexão HTTP.
  return "";
}

String WiFiPOST(String link, String postdata) {
  if (!conectaWiFi()) return "";

  HTTPClient http; // Inicia biblioteca HTTPClient.
  http.begin(link);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(postdata); // Recebe o código que o servidor retornou.

  if (httpCode == HTTP_CODE_OK) { // Se a conexão obtiver sucesso, executa o código abaixo.
    String resposta = http.getString(); // Recebe o conteúdo da página.
    http.end();
    return resposta;
  }

  http.end(); // Encerra conexão HTTP.
  return "";
}

bool autenticaUFPI() {
  if (!conectaWiFi()) return 0;

  if (String(ssid) == "UFPI"){
    String temIP = WiFiGET("http://sistema.rscada.ga/api/123");
    if (temIP != "") return 1;

    String resposta = WiFiPOST("https://login.ufpi.br:6082/php/uid.php?vsys=1&rule=0&url=", "inputStr=&escapeUser=&preauthid=&user=rhulio&passwd=otek24&ok=Login");

    if (resposta.indexOf("User Authenticated") >= 0) {
      Serial.println("[UFPI] Usuário autenticado!");
      return 1;
    } else {
      Serial.println("[UFPI] Falha na autenticação!");
      return 0;
    }
  } else return 1;
}

void wdt() {
  yield();
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    Serial.println("Client Connected");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("[ESP] Iniciando dispositivo...");

  OBD.register_callback(callback);
  OBD.begin("OBD");
  Serial.println("[Bluetooth] Iniciando conexão...");

  //WiFi.persistent(false);
  //WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, senha);
  WiFi.setSleep(false);

  autenticaUFPI();
  mqtt.begin("mqtt.rscada.ga", net);
}

unsigned long tempoTotal = 0;

void loop() {
  if(autenticaUFPI()){
    unsigned long tempoInicial = millis();

    //String dadosGet;
    String sinal = String(WiFi.RSSI());
    /*
    if(tempoTotal > 0)
      dadosGet = "sinal=" + sinal + "&latencia=" + String(tempoTotal);
    else
      dadosGet = "sinal=" + sinal;


    String webservice = WiFiGET("http://sistema.rscada.ga/api/"+token+"/envio?"+dadosGet);
    */
    if (!mqtt.connected())
      mqtt.connect("438C1C", "438C1C", "438C1C");
    else {
      mqtt.publish("438C1C/sinal", String(sinal), false, 1);
      if(tempoTotal > 0)
        mqtt.publish("438C1C/latencia", String(tempoTotal), false, 1);
    }

    tempoTotal = millis() - tempoInicial;
    //while((millis() - tempoInicial) < 200) wdt();
    wdt();
  }
  wdt();
}
