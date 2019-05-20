#include <WiFi.h>
#include <HTTPClient.h>
extern HardwareSerial GPRS(2);

const byte pinoRX = 5;
const byte pinoTX = 18;
const byte pinoTR = 23;
const byte pinoDHT = 22;
const byte pinoUS1 = 13;
const byte pinoUS2 = 12;
const byte pinoVZ1 = 25;
const byte pinoVZ2 = 26;
const byte pinoSL1 = 21;
const byte pinoSL2 = 19;

const char* ssid = "UFPI"; // Define o nome da rede WiFi.
const char* senha = NULL; // Define a senha da rede WiFi.

const String codigoDispositivo = "789C376B43";

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

float dadosTemperatura = 0;
float dadosUmidadeAr = 0;

float dadosUmidadeSolo1 = 0;
float dadosUmidadeSolo2 = 0;

volatile double litrosTotal1 = 0;
volatile double litrosTotal2 = 0;

#include <DHTesp.h>
#include "Ticker.h"
DHTesp dht;

void tarefaAquisicao(void *pvParameters);
void triggerAquisicao();

TaskHandle_t tarefaAquisicaoHandle = NULL;

Ticker tickerAquisicao;
bool ativaTarefas = false;
void Aquisicoes();

bool iniciarAquisicao() {
  dht.setup(pinoDHT, DHTesp::DHT11);

  xTaskCreatePinnedToCore(
    tarefaAquisicao, // função da tarefa
    "tarefaAquisicao", // nome da tarefa
    4000, // tamanho da pilha (palavras)
    NULL, // entrada de parâmetros
    5, // prioridade
    &tarefaAquisicaoHandle, // handle
    1); // processador

  if (tarefaAquisicaoHandle == NULL) {
    Serial.println("[Tarefa de Aquisição] Falha ao iniciar a tarefa!");
    return false;
  } else tickerAquisicao.attach(5, triggerAquisicao);

  return true;
}

void triggerAquisicao() {
  if (tarefaAquisicaoHandle != NULL) {
    xTaskResumeFromISR(tarefaAquisicaoHandle);
  }
}

void tarefaAquisicao(void *pvParameters) {
  while (1) {
    if (ativaTarefas) Aquisicoes();
    vTaskSuspend(NULL);
  }
}

void IRAM_ATTR vazao1() {
  portENTER_CRITICAL_ISR(&mux);
  litrosTotal1++;
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR vazao2() {
  portENTER_CRITICAL_ISR(&mux);
  litrosTotal2++;
  portEXIT_CRITICAL_ISR(&mux);
}

void Aquisicoes() {
  TempAndHumidity DHT11 = dht.getTempAndHumidity();
  if (dht.getStatus())
    Serial.println("[Tarefa de Aquisição] Falha ao ler o DHT11!");
  else {
    dadosTemperatura = DHT11.temperature;
    dadosUmidadeAr = DHT11.humidity;
  }

  int ValorADC = analogRead(pinoUS1);
  if (!ValorADC)
    Serial.println("[Tarefa de Aquisição] Falha ao ler Umidade do Solo 1!");
  else
    dadosUmidadeSolo1 = 100 * ((4095 - (float)ValorADC) / 4095);

  ValorADC = 0;
  ValorADC = analogRead(pinoUS2);
  if (!ValorADC)
    Serial.println("[Tarefa de Aquisição] Falha ao ler Umidade do Solo 2!");
  else
    dadosUmidadeSolo2 = 100 * ((4095 - (float)ValorADC) / 4095);

  litrosTotal1 += 100;
  litrosTotal2 += 100;
}

void iniciaVazao() {
  pinMode(pinoVZ1, INPUT_PULLUP);
  pinMode(pinoVZ2, INPUT_PULLUP);

  pinMode(pinoSL1, OUTPUT);
  pinMode(pinoSL2, OUTPUT);

  digitalWrite(pinoSL1, LOW);
  digitalWrite(pinoSL2, LOW);

  attachInterrupt(digitalPinToInterrupt(pinoVZ1), vazao1, RISING);
  attachInterrupt(digitalPinToInterrupt(pinoVZ2), vazao2, RISING);

  Serial.println("[Sensores de Vazão] Iniciados!");
}

String float2str(float x, byte precision = 2) {
  char tmp[50];
  dtostrf(x, 0, precision, tmp);
  return String(tmp);
}

String webservice = "";
String dadosPost = "";
int errosSeguidos = 0;
int limiteErros = 25;

//#include <NTPClient.h>
//#include <WiFiUdp.h>

//WiFiUDP ntpUDP; // Chama biblioteca UDP para o NTP.
//NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3*3600, 60000); // Define servidor NTP tal como timezone.

void reiniciaGPRS() {
  errosSeguidos = 0;
  GPRS.println("AT+CPOF");
  GPRS.end();
  digitalWrite(pinoTR, LOW);
  delay(500);
  digitalWrite(pinoTR, HIGH);
  iniciaGPRS();
  WiFi.reconnect();
}

int iniciaGPRS() {
  pinMode(pinoTR, OUTPUT);
  digitalWrite(pinoTR, HIGH);

  pinMode(pinoRX, INPUT);
  pinMode(pinoTX, OUTPUT);

  digitalWrite(pinoTX, LOW);
  digitalWrite(pinoRX, LOW);

  GPRS.begin(9600, SERIAL_8N1, pinoRX, pinoTX);

  Serial.flush();
  GPRS.flush();

  Serial.println();
  GPRS.println();

  if (GPRS) {
    return 1;
  } else
    return 0;
}

String enviaGPRS(String comando, int delayMs) {
  String conteudo = ""; // Declara variável para receber as respostas do serial.

  GPRS.flush();

  GPRS.setTimeout(delayMs);
  GPRS.println(comando);

  unsigned long tempo = millis();

  while (millis() - tempo < delayMs) {
    while (GPRS.available()) {
      conteudo = GPRS.readString();
    }

    if (conteudo != "") break;
  }

  Serial.println(conteudo);
  return conteudo;
}

int conectaGPRS() {
  String conteudo = ""; // Declara variável para receber as respostas do serial.
  String comando = "";
  String apn = "";
  int ole = 0;

  goto CGACT;

CREG:
  if (errosSeguidos > limiteErros) reiniciaGPRS();
  comando = "AT+CREG?";
  conteudo = enviaGPRS(comando, 250);
  if (conteudo.indexOf(comando) + conteudo.indexOf("1,1\r\n") < 0) {
    Serial.println("[GPRS] Operadora desconectada...\n");
    wdt();
    errosSeguidos++;

    return 0;
  }

  Serial.println("[GPRS] Operadora conectada...\n");

  comando = "AT+COPS=3,0";
  conteudo = enviaGPRS(comando, 250);
  if (conteudo.indexOf(comando) + conteudo.indexOf("OK") < 0) {
    Serial.println("[GPRS] Falha ao selecionar o COPS...\n");
    wdt();

    return 0;
  }

  comando = "AT+COPS?";
  conteudo = enviaGPRS(comando, 500);
  if (conteudo.indexOf(comando) + conteudo.indexOf("OK") < 0) {
    Serial.println("[GPRS] Falha ao identificar operadora...\n");
    wdt();

    return 0;
  }

  if (conteudo.indexOf("Oi") >= 0) {
    apn = "gprs.oi.com.br";
  } else if (conteudo.indexOf("Claro BR") >= 0) {
    apn = "claro.com.br";
  } else if (conteudo.indexOf("TIM") >= 0) {
    apn = "tim.br";
  }

  if (apn == "") goto CREG;

  Serial.println("[GPRS] Operadora identificada...\n");

  comando = "AT+CGATT=1";
  conteudo = enviaGPRS(comando, 1000); // Envia comando para Adquirir conexão à internet.
  if (conteudo.indexOf(comando) + conteudo.indexOf("OK") < 0) {
    Serial.println("[GPRS] Falha ao ativar GPRS...\n");
    wdt();

    return 0;
  }

  Serial.println("[GPRS] GPRS ativado...\n");

  comando = "AT+CGDCONT=1,IP," + apn;
  conteudo = enviaGPRS(comando, 500); // Envia comando para Adquirir conexão à internet.
  if (conteudo.indexOf(comando) + conteudo.indexOf("OK") < 0) {
    Serial.println("[GPRS] Falha ao configurar APN...\n");
    wdt();

    return 0;
  }

  Serial.println("[GPRS] APN configurado...\n");

CGACT:
  comando = "AT+CGACT=1,1";
  conteudo = enviaGPRS(comando, 2000); // Envia comando para Adquirir conexão à internet.
  if (conteudo.indexOf(comando) + conteudo.indexOf("OK") < 0) {
    Serial.println("[GPRS] Falha ao selecionar APN...\n");
    errosSeguidos++;
    wdt();
    if (ole)
      return 0;
    else {
      ole = 1;
      goto CREG;
    }
  }

  Serial.println("[GPRS] APN selecionado...\n");

FINAL:
  Serial.println("[GPRS] Pronto para conectar...\n");

  return 1;
}

String gprsGET(String url, String uri) {
  String conteudo = ""; // Declara variável para receber as respostas do serial.
  String comando = "";

  goto CIPSTART;

CIPCLOSE:
  enviaGPRS("AT+CIPCLOSE", 500);
  Serial.println("[GPRS] Perdida a conexão com o módulo...");
  wdt();
  errosSeguidos++;
  if (errosSeguidos > limiteErros) reiniciaGPRS();

  return "";

CIPSTART:
  if (!conectaGPRS()) return "";
  comando = "AT+CIPSTART=TCP," + url + ",80";
  conteudo = enviaGPRS(comando, 2000); // Envia comando para iniciar comunicação TCP.
  if (conteudo.indexOf("ERROR:50") >= 0 or conteudo.indexOf(comando) + conteudo.indexOf("CONNECT OK") < 0)
    goto CIPCLOSE;

  String payload = "GET /" + uri + " HTTP/1.1\r\nHost: " + url + "\r\n\r\n";

  comando = "AT+CIPSEND";
  conteudo = enviaGPRS(comando, 1000);
  if (conteudo.indexOf(comando) + conteudo.indexOf(">") < 0)
    goto CIPCLOSE;

  GPRS.print(payload);
  GPRS.write(0x1A);

  conteudo = enviaGPRS("", 2000);

  return conteudo;
}

String gprsPOST(String url, String uri, String postdata) {
  String conteudo = ""; // Declara variável para receber as respostas do serial.
  String comando = "";

  goto CIPSTART;

CIPCLOSE:
  enviaGPRS("AT+CIPCLOSE", 500);
  Serial.println("[GPRS] Perdida a conexão com o módulo...");
  errosSeguidos++;
  if (errosSeguidos > limiteErros) reiniciaGPRS();
  wdt();
  return "";

CIPSTART:
  if (!conectaGPRS()) return "";
  comando = "AT+CIPSTART=TCP," + url + ",80";
  conteudo = enviaGPRS(comando, 2000); // Envia comando para iniciar comunicação TCP.
  if (conteudo.indexOf("ERROR:50") >= 0 or conteudo.indexOf(comando) + conteudo.indexOf("CONNECT OK") < 0)
    goto CIPCLOSE;

  String payload = "POST /" + uri + " HTTP/1.1\r\nHost: " + url + "\r\nAccept: */*\r\nUser-Agent: Mozilla/5.0 (compatible; Rigor/1.0.0; http://rigor.com)\r\nContent-type: application/x-www-form-urlencoded\r\n";

  comando = "AT+CIPSEND";
  conteudo = enviaGPRS(comando, 1000);
  if (conteudo.indexOf(comando) + conteudo.indexOf(">") < 0)
    goto CIPCLOSE;

  GPRS.print(payload);
  GPRS.print("Content-Length: ");
  GPRS.println(postdata.length());
  GPRS.println();
  GPRS.print(postdata);
  GPRS.println();
  GPRS.write(0x1A);

  conteudo = enviaGPRS("", 2000);

  if (conteudo.indexOf("PRCV") < 0)
    goto CIPCLOSE;

  return conteudo;
}

int conectaWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("[WiFi] Conectando... Status: ");
    Serial.println(WiFi.status());
    delay(250);
    WiFi.reconnect();
    return 0;
  } else {
    //Serial.print("[WiFi] Conectado... IP: ");
    //Serial.println(WiFi.localIP());
    return 1;
  }
}

int autenticaUFPI() {
  if (!conectaWiFi) return 0;

  String temIP = WiFiGET("http://tenhafe.ml/webService/123");
  if (temIP != "") return 1;

  if (String(ssid) != "UFPI") return 0;

  String resposta = WiFiPOST("https://login.ufpi.br:6082/php/uid.php?vsys=1&rule=0&url=", "inputStr=&escapeUser=&preauthid=&user=rhulio&passwd=otek24&ok=Login");

  if (resposta.indexOf("User Authenticated") >= 0) {
    Serial.println("[UFPI] Usuário autenticado!");
    return 1;
  } else {
    Serial.println("[UFPI] Falha na autenticação!");
    return 0;
  }
}

String WiFiGET(String link) {
  if (!conectaWiFi) return "";

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
  if (!conectaWiFi) return "";

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

String webService(String dadosPost) {
  if (WiFi.status() == WL_CONNECTED && autenticaUFPI())
    webservice = WiFiPOST("http://tenhafe.ml/webService/" + codigoDispositivo, dadosPost);
  else
    webservice = gprsPOST("tenhafe.ml", "webService/" + codigoDispositivo, dadosPost);

  if (webservice.indexOf("gravada") >= 0){
    errosSeguidos = 0;
    
    if (webservice.indexOf("ltc1") >= 0) litrosTotal1 = 0;
    if (webservice.indexOf("ltc2") >= 0) litrosTotal2 = 0;

    if (webservice.indexOf("canal1:on") >= 0)
      digitalWrite(pinoSL1, HIGH);
    else if (webservice.indexOf("canal1:off") >= 0)
      digitalWrite(pinoSL1, LOW);        

    if (webservice.indexOf("canal2:on") >= 0)
      digitalWrite(pinoSL2, HIGH);
    else if (webservice.indexOf("canal2:off") >= 0)
      digitalWrite(pinoSL2, LOW);

    return webservice;
  } else
    return "";
}

void wdt() {
  yield();
}

/*
  void atualizaNTP(){
  timeClient.update(); // Sincroniza relógio através do WiFi via NTP.
  Serial.println(timeClient.getFormattedTime()); // Mostra o horário do NTP atualizado.
  }
*/

void setup() {
  Serial.begin(115200); // Define baudrate da transmissão com o PC.
  Serial.println();

  iniciaVazao();

  Serial.println("[ESP] Iniciando dispositivo...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, senha);
  conectaWiFi();

  Serial.println("[GPRS] Iniciando...\n");
  if (iniciaGPRS()) conectaGPRS();

  //timeClient.begin(); // Inicia o cliente NTP para atualizar o relógio do ESP.

  iniciarAquisicao();
  ativaTarefas = true;
}

void loop() {
  if (!ativaTarefas) {
    ativaTarefas = true;
    if (tarefaAquisicaoHandle != NULL)
      vTaskResume(tarefaAquisicaoHandle);
  }
  conectaWiFi();

  dadosPost = "temp=" + float2str(dadosTemperatura) + "&umar=" + dadosUmidadeAr + "&ums1=" + dadosUmidadeSolo1 + "&ums2=" + dadosUmidadeSolo2 + "&ltc1=" + (float)(litrosTotal1 / 477) + "&ltc2=" + (float)(litrosTotal2 / 477);
  webservice = webService(dadosPost);

  Serial.println(webservice);

  wdt();
  delay(1000);
}
