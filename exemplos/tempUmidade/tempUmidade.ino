#include <WiFi.h>
#include <HTTPClient.h>
#include <DHTesp.h>

DHTesp dht;
float dadosTemperatura = 0;
float dadosUmidade = 0;

const char* ssid = "# Cipriano";
const char* senha = "globo321";

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

void setup() {
  Serial.begin(9600);
  Serial.println("[ESP] Iniciando dispositivo...");
  
  //WiFi.persistent(false);
  //WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, senha);
  WiFi.setSleep(false);
  
  autenticaUFPI();
  dht.setup(23, DHTesp::DHT11);
}

unsigned long tempoTotal = 0;
  
void loop() {
  if(autenticaUFPI()){
    unsigned long tempoInicial = millis();

    TempAndHumidity DHT11 = dht.getTempAndHumidity();
    if (dht.getStatus())
      Serial.println("[Tarefa de Aquisição] Falha ao ler o DHT11!");
    else {
      dadosTemperatura = DHT11.temperature;
      dadosUmidade = DHT11.humidity;
      String webservice = WiFiGET("http://sistema.rscada.ga/api/53027A/envio?temperatura="+float2str(dadosTemperatura)+"&umidade="+float2str(dadosUmidade));
    }

    while((millis() - tempoInicial) < 10000) wdt();
  }
  wdt();
}
