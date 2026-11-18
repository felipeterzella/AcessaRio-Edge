#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <math.h>

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* WRITE_KEY = "NM3X04969IRHR0NF";

// Pinos (inalterados)
const int DHT_PIN = 4;
const int SDA_PIN = 32, SCL_PIN = 33;           // I2C do MPU6050
const int LED_VERDE = 25, LED_AMARELO = 26, LED_VERMELHO = 27;
DHT dht(DHT_PIN, DHT22);

const int MPU_ADDR = 0x68;                      // endereco I2C do MPU6050

// ---------------------------------------------------------------------------
// Eixos e conversao para porcentagem
// ---------------------------------------------------------------------------
// X = inclinacao LONGITUDINAL (sentido da subida); Y = inclinacao TRANSVERSAL (lado).
// O acelerometro mede a gravidade projetada em cada eixo: a/g = sen(angulo).
// Inclinacao (%) = tan(angulo) * 100. Para um sensor deitado na rampa, com
// angulo = atan2(a_eixo, az), isso vira: inclinacao % = 100 * a_eixo / |az|.
// (5% = ~2,86 graus; nao confundir graus com porcentagem.)
//
// No Wokwi os controles andam de 0,1 em 0,1 g; com ESCALA_WOKWI = 10, cada
// passo vale 1%. Em um sensor fisico, coloque MODO_WOKWI = false para usar a
// formula real (tan do angulo).
const bool MODO_WOKWI = true;
const float ESCALA_WOKWI = 10.0;

// ORIENTACAO FISICA: por padrao ax = X (subida) e ay = Y (lado). Se o MPU6050
// for montado girado 90 graus na rampa (seta X da placa apontando para o lado),
// coloque TROCAR_EIXOS = true. Se a seta apontar no sentido contrario ao da
// subida, so o sinal muda (o codigo usa o valor absoluto na avaliacao).
const bool TROCAR_EIXOS = false;

// O eixo Z so confere se o sensor esta deitado na rampa (Z perto de 1 g).
const float Z_MINIMO = 0.5;

// ---------------------------------------------------------------------------
// Criterios ABNT NBR 9050
// ---------------------------------------------------------------------------
// Tabela (rampa de 1 segmento): o desnivel (altura vencida) define ate quanto
// a inclinacao X pode ir.
//   desnivel ate 0,80 m          -> X ate 8,33%
//   desnivel >0,80 ate 1,00 m    -> X ate 6,25%
//   desnivel >1,00 ate 1,50 m    -> X ate 5,00%
//   desnivel acima de 1,50 m     -> reprovada (acima do maximo de 1,50 m)
// Equivalente a olhar a inclinacao medida: ate 5% -> max 1,50 m | >5 a 6,25% -> max 1,00 m
// | >6,25 a 8,33% -> max 0,80 m.
const float TOLERANCIA = 0.0001;               // tolerancia para comparar numeros decimais
const float LIMITE_NBR = 8.33;          // acima disso: fora do limite (sempre vermelho)
const float INCL_PROXIMA = 6.0;         // "proxima do limite" (amarelo) a partir de 6% quando o limite do caso e maior
const float FAIXA1_MAX = 5.0;           // ate 5%      -> desnivel max 1,50 m
const float FAIXA2_MAX = 6.25;          // >5 a 6,25%  -> desnivel max 1,00 m
const float DESNIVEL_FAIXA1 = 1.50;
const float DESNIVEL_FAIXA2 = 1.00;
const float DESNIVEL_FAIXA3 = 0.80;     // >6,25 a 8,33% -> 0,80 m
const float LIMITE_TRANSVERSAL = 3.0;   // rampa SEMPRE em ambiente externo: max 3%
const float TRANSV_PROXIMA = 2.5;       // a partir daqui, Y e considerado "proximo do limite"

// ---- Regras do semaforo com umidade ----
// Umidade: <50 baixa | 50-60 media | 60-70 media-alta | 70-99,5 alta | >=99,5 maxima
const float LIMITES_UMID[4] = {50.0, 60.0, 70.0, 99.5};

// Linhas: inclinacao OK / no limite ou proxima do limite / reprovada.
// Colunas: umidade baixa / media / media-alta / alta / maxima.
// 0 = verde, 1 = amarelo, 2 = vermelho
const int TABELA[3][5] = {
  {0, 0, 1, 1, 2},   // dentro dos criterios (longe dos limites)
  {1, 1, 2, 2, 2},   // no limite / proxima do limite (amarelo mesmo com umidade baixa)
  {2, 2, 2, 2, 2}    // reprovada (X, Y ou desnivel fora do limite)
};
const char* NOME_STATUS[3] = {"VERDE", "AMARELO", "VERMELHO"};

// ---------------------------------------------------------------------------
// Modo sob demanda: NADA roda sozinho. Leitura, avaliacao, LED e envio ao
// ThingSpeak so acontecem quando voce pede pelo Serial Monitor.
// Rampa de UM segmento: so a altura vencida (desnivel) e digitada.
// ---------------------------------------------------------------------------
enum Estado { OCIOSO, PEDE_ALTURA, PEDE_MEDIDA };
Estado estado = OCIOSO;

float alturaSalva = 0;      // desnivel (altura vencida) da rampa, em metros
bool temAltura = false;

// Ultima avaliacao (o LED e o relatorio 'r' mostram esta)
bool avaliada = false;
bool rampaAprovada = false, xOk = false, yOk = false;
float ultimaAltura = 0, ultimoX = 0, ultimoY = 0, ultimaUmid = 0;
int ultimoStatus = 0;

String linhaSerial = "";

// Acorda o MPU6050 (ele comeca em modo de repouso)
bool iniciarMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);                             // registrador de energia
  Wire.write(0);                                // 0 = acordar
  return Wire.endTransmission(true) == 0;
}

// Le um valor de 16 bits (byte alto e depois byte baixo)
int16_t lerInt16() {
  int16_t alto = Wire.read();
  int16_t baixo = Wire.read();
  return (int16_t)((alto << 8) | baixo);
}

// Le a aceleracao nos eixos do chip, em g. Devolve false se o sensor nao responder.
bool lerAceleracao(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);                             // primeiro registrador do acelerometro
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 6, true) != 6) return false;
  ax = lerInt16() / 16384.0;                    // 16384 = 1 g na escala padrao
  ay = lerInt16() / 16384.0;
  az = lerInt16() / 16384.0;
  return true;
}

// Converte a aceleracao de um eixo (g) em inclinacao (%), arredondada em 2 casas
float inclinacaoPct(float aEixo, float az) {
  float pct = MODO_WOKWI ? aEixo * ESCALA_WOKWI
                         : 100.0 * aEixo / fabs(az);   // tan(angulo) * 100
  return roundf(pct * 100.0) / 100.0;
}

// Le umidade e inclinacoes AGORA. Se manual = true, o X vem de xManual (em %)
// e o Y continua vindo do sensor. Devolve false (com mensagem) se algo falhar.
bool lerMedidas(bool manual, float xManual, float &x, float &y, float &umid) {
  umid = dht.readHumidity();
  if (isnan(umid)) {
    Serial.println("Erro de leitura do DHT22. Tente de novo em 2 s.");
    return false;
  }
  float ax, ay, az;
  if (!lerAceleracao(ax, ay, az)) {
    Serial.println("Erro de leitura do MPU6050. Confira SDA no 32 e SCL no 33.");
    return false;
  }
  if (fabs(az) < Z_MINIMO) {
    Serial.printf("MPU6050 fora da posicao (Z=%.2f g): mantenha o Z acima de %.1f g e tente de novo.\n", az, Z_MINIMO);
    return false;
  }
  // Eixo X = subida (longitudinal) | Eixo Y = lado (transversal)
  float aSubida = TROCAR_EIXOS ? ay : ax;
  float aLado   = TROCAR_EIXOS ? ax : ay;
  x = manual ? xManual : inclinacaoPct(aSubida, az);
  y = inclinacaoPct(aLado, az);
  return true;
}

// ---------------------------------------------------------------------------
// Avaliacao NBR 9050
// ---------------------------------------------------------------------------
// Faixa do eixo X: 1 = ate 5% | 2 = >5 a 6,25% | 3 = >6,25 a 8,33% | 4 = acima de 8,33%
int faixaX(float x) {
  float a = fabs(x);
  if (a > LIMITE_NBR) return 4;
  if (a > FAIXA2_MAX) return 3;
  if (a > FAIXA1_MAX) return 2;
  return 1;
}

float desnivelMaximo(int faixa) {
  if (faixa == 1) return DESNIVEL_FAIXA1;
  if (faixa == 2) return DESNIVEL_FAIXA2;
  if (faixa == 3) return DESNIVEL_FAIXA3;
  return 0;
}

// Limite do eixo X (%) para o desnivel informado: quanto maior a rampa, menor o limite.
// Devolve 0 se o desnivel passa de 1,50 m (nao existe limite de X que aprove).
float limiteXPorAltura(float altura) {
  if (altura <= DESNIVEL_FAIXA3 + TOLERANCIA) return LIMITE_NBR;   // ate 0,80 m -> 8,33%
  if (altura <= DESNIVEL_FAIXA2 + TOLERANCIA) return FAIXA2_MAX;   // ate 1,00 m -> 6,25%
  if (altura <= DESNIVEL_FAIXA1 + TOLERANCIA) return FAIXA1_MAX;   // ate 1,50 m -> 5%
  return 0;
}

String textoLimiteX(int faixa) {
  if (faixa == 1) return "ate 5%: desnivel max 1,50 m";
  if (faixa == 2) return ">5% a 6,25%: desnivel max 1,00 m";
  if (faixa == 3) return ">6,25% a 8,33%: desnivel max 0,80 m";
  return ">8,33%: fora do limite";
}

// Numero com virgula, como nas mensagens (ex.: 6,25)
String formatarPct(float v) {
  String s = String(v, 2);
  s.replace('.', ',');
  return s;
}

// ---------------------------------------------------------------------------
// Semaforo (X, Y, desnivel e umidade)
// ---------------------------------------------------------------------------
// Linha da tabela: 0 = dentro dos criterios, 1 = no limite ou proxima do limite, 2 = reprovada.
// O limite de X depende do desnivel (caso a caso): com 1,50 m o limite e 5%,
// com 1,00 m e 6,25%, com 0,80 m e 8,33%. Chegou no limite do caso -> amarelo,
// mesmo com Y e umidade em zero. Y tem limite fixo de 3%.
int linhaInclinacao(bool aprovada, float x, float y, float altura) {
  if (!aprovada) return 2;                                    // X, Y ou desnivel reprovado
  float limX = limiteXPorAltura(altura);
  float proxX = (limX < INCL_PROXIMA) ? limX : INCL_PROXIMA;  // no limite do caso, ou a partir de 6%
  if (fabs(x) >= proxX - TOLERANCIA || fabs(y) >= TRANSV_PROXIMA - TOLERANCIA) return 1;
  return 0;
}

// Coluna da tabela: conta quantos limites de umidade foram atingidos (0 a 4)
int colunaUmidade(float umid) {
  int coluna = 0;
  for (int i = 0; i < 4; i++) {
    if (umid >= LIMITES_UMID[i]) coluna++;
  }
  return coluna;
}

// Consulta a tabela: 0 = verde, 1 = amarelo, 2 = vermelho
int classificar(bool aprovada, float x, float y, float umid, float altura) {
  return TABELA[linhaInclinacao(aprovada, x, y, altura)][colunaUmidade(umid)];
}

// status 0/1/2 acende um LED; qualquer outro valor (ex.: -1) apaga todos
void sinalizar(int status) {
  digitalWrite(LED_VERDE,    status == 0);
  digitalWrite(LED_AMARELO,  status == 1);
  digitalWrite(LED_VERMELHO, status == 2);
}

void adicionarCausa(String &causas, const char* texto) {
  if (causas != "") causas += "; ";
  causas += texto;
}

// Texto que explica o estado (muda junto com a cor do LED)
String descrever(int status, float x, float y, float umid, float altura) {
  String causas = "";
  float ax = fabs(x), ay = fabs(y);
  float limX = limiteXPorAltura(altura);

  if (ax > LIMITE_NBR) {
    adicionarCausa(causas, "inclinacao X acima da NBR 9050 (8,33%)");
  } else if (altura > DESNIVEL_FAIXA1 + TOLERANCIA) {
    adicionarCausa(causas, "desnivel (altura vencida) acima do maximo da NBR 9050 (1,50 m)");
  } else if (ax > limX + TOLERANCIA) {
    adicionarCausa(causas, ("inclinacao X acima do limite para esse desnivel (" + formatarPct(limX) + "%)").c_str());
  } else if (ax >= limX - TOLERANCIA) {
    adicionarCausa(causas, ("inclinacao X no limite da NBR 9050 para esse desnivel (" + formatarPct(limX) + "%)").c_str());
  } else if (ax >= INCL_PROXIMA) {
    adicionarCausa(causas, ("inclinacao X proxima do limite da NBR 9050 para esse desnivel (" + formatarPct(limX) + "%)").c_str());
  }

  if (ay > LIMITE_TRANSVERSAL) adicionarCausa(causas, "inclinacao transversal Y acima de 3%");
  else if (ay >= TRANSV_PROXIMA) adicionarCausa(causas, "inclinacao transversal Y proxima do limite de 3%");

  int coluna = colunaUmidade(umid);
  if (coluna == 4) adicionarCausa(causas, "umidade maxima (piso provavelmente molhado)");
  else if (coluna == 3) adicionarCausa(causas, "umidade alta (piso possivelmente escorregadio)");
  else if (coluna >= 1) adicionarCausa(causas, "umidade media");

  if (status == 0) {
    if (causas == "") return "LIVRE: rampa em condicoes normais";
    return "LIVRE: rampa em condicoes normais | observacao: " + causas;
  }
  if (causas == "") causas = "inclinacao e umidade moderadas";
  if (status == 1) return "ATENCAO: " + causas + " | use com cuidado";
  return "PERIGO: " + causas + " | evite o trecho";
}

// ---------------------------------------------------------------------------
// Wi-Fi e ThingSpeak
// ---------------------------------------------------------------------------
void conectarWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  Serial.print("Conectando ao Wi-Fi");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500); Serial.print("."); tentativas++;
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " conectado" : " falhou");
}

// Campos do canal (enviados uma vez a cada avaliacao):
//  field1 = umidade (%)
//  field2 = inclinacao X (%, em modulo)
//  field3 = status do semaforo: 0 verde | 1 amarelo | 2 vermelho
//  field4 = inclinacao Y (%, em modulo)   [habilitar no canal]
//  field5 = resultado da rampa: 1 aprovada | 2 reprovada   [habilitar no canal]
//  field6 = desnivel da rampa (m)   [habilitar no canal; o main.py usa para achar o limite de X do caso]
void enviarThingSpeak(float umid, float x, float y, int status, int resultado, float altura) {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://api.thingspeak.com/update?api_key=" + String(WRITE_KEY) +
               "&field1=" + String(umid, 1) +
               "&field2=" + String(fabs(x), 2) +
               "&field3=" + String(status) +
               "&field4=" + String(fabs(y), 2) +
               "&field5=" + String(resultado) +
               "&field6=" + String(altura, 2);
  HTTPClient http;
  http.begin(url);
  int codigo = http.GET();
  Serial.print("ThingSpeak HTTP "); Serial.print(codigo);
  Serial.print(" | entry_id: "); Serial.println(http.getString());
  http.end();
}

// ---------------------------------------------------------------------------
// Relatorio e avaliacao
// ---------------------------------------------------------------------------
void imprimirRelatorio() {
  int f = faixaX(ultimoX);
  float limX = limiteXPorAltura(ultimaAltura);
  Serial.println("\n===== AVALIACAO DA RAMPA (ABNT NBR 9050) =====");
  Serial.printf("Desnivel (altura vencida): %.2f m\n", ultimaAltura);
  if (limX > 0) Serial.printf("Limite de X para esse desnivel: %.2f%%\n", limX);
  else Serial.println("Desnivel acima de 1,50 m: nenhum limite de X aprova");
  Serial.println("--- Eixo X (inclinacao longitudinal) ---");
  Serial.printf("Inclinacao X medida: %.2f%%\n", ultimoX);
  Serial.printf("Faixa da inclinacao medida: %s\n", textoLimiteX(f).c_str());
  Serial.printf("Resultado eixo X: %s\n", xOk ? "APROVADO" : "REPROVADO");
  Serial.println("--- Eixo Y (inclinacao transversal) ---");
  Serial.printf("Inclinacao Y medida: %.2f%%\n", ultimoY);
  Serial.printf("Resultado eixo Y: %s\n", yOk ? "APROVADO" : "REPROVADO");
  Serial.println("--- Ultimo resultado ---");
  Serial.printf("RAMPA %s (X %s | Y %s)\n", rampaAprovada ? "APROVADA" : "REPROVADA",
                xOk ? "ok" : "reprovado", yOk ? "ok" : "reprovado");
  Serial.printf("Umidade: %.1f%% | semaforo: %s\n", ultimaUmid, NOME_STATUS[ultimoStatus]);
  Serial.println(descrever(ultimoStatus, ultimoX, ultimoY, ultimaUmid, ultimaAltura));
  Serial.println("==============================================");
  Serial.println("Digite 'm' para medir de novo, 's' para informar outra altura ou 'r' para repetir o relatorio.");
}

// Avalia, guarda o resultado, acende o LED, imprime o relatorio e envia ao ThingSpeak
void avaliarRampa(float altura, float x, float y, float umid) {
  int f = faixaX(x);
  xOk = (f <= 3) && (altura <= desnivelMaximo(f) + TOLERANCIA);
  yOk = fabs(y) <= LIMITE_TRANSVERSAL + TOLERANCIA;
  rampaAprovada = xOk && yOk;

  ultimaAltura = altura;
  ultimoX = x;
  ultimoY = y;
  ultimaUmid = umid;
  ultimoStatus = classificar(rampaAprovada, x, y, umid, altura);
  avaliada = true;

  sinalizar(ultimoStatus);
  imprimirRelatorio();
  enviarThingSpeak(umid, x, y, ultimoStatus, rampaAprovada ? 1 : 2, altura);
}

// ---------------------------------------------------------------------------
// Entrada pelo Serial Monitor (maquina de estados: nunca bloqueia)
// ---------------------------------------------------------------------------
// Aceita ponto ou virgula como separador decimal
bool parseNumero(String s, float &valor) {
  s.replace(',', '.');
  const char* inicio = s.c_str();
  char* fim;
  valor = strtof(inicio, &fim);
  return fim != inicio && *fim == '\0';
}

void ajuda() {
  Serial.println("Comandos: 's' = nova avaliacao (pede a altura) | 'm' = medir de novo (usa a altura salva)");
  Serial.println("          'r' = repetir relatorio | 'c' = cancelar");
}

void pedirAltura() {
  Serial.println("Informe a altura vencida (desnivel) da rampa em metros (ex.: 0,75):");
}

void pedirMedida() {
  Serial.println("Posicione o sensor na rampa e digite 'ok' para usar a leitura atual.");
  Serial.println("As inclinacoes X e Y e a umidade vem do sensor.");
}

void tratarEntrada(String linha) {
  String cmd = linha;
  cmd.toLowerCase();

  if (cmd == "c" || cmd == "cancelar") {
    if (estado != OCIOSO) {
      estado = OCIOSO;
      if (avaliada) sinalizar(ultimoStatus);     // volta o LED da avaliacao anterior
      Serial.println("Cancelado. A avaliacao anterior (se houver) foi mantida.");
    } else {
      Serial.println("Nada para cancelar. Digite 's' para comecar.");
    }
    return;
  }

  // 'r' funciona em qualquer momento (mesmo esperando altura ou medida)
  if (cmd == "r") {
    if (avaliada) imprimirRelatorio();
    else Serial.println("Ainda nao ha avaliacao. Digite 's' para comecar.");
    return;
  }

  float v;
  switch (estado) {
    case OCIOSO:
      if (cmd == "s") {
        estado = PEDE_ALTURA;
        sinalizar(-1);                           // nova avaliacao: apaga o LED da anterior
        Serial.println("\n--- Nova avaliacao ---");
        pedirAltura();
      } else if (cmd == "m") {
        if (!temAltura) {
          Serial.println("Ainda nao ha altura salva. Digite 's' para informar a altura.");
        } else {
          estado = PEDE_MEDIDA;
          sinalizar(-1);                         // nova medida: apaga o LED da anterior
          Serial.printf("\nMedindo de novo com a altura salva: %.2f m\n", alturaSalva);
          pedirMedida();
        }
      } else {
        ajuda();
      }
      break;

    case PEDE_ALTURA:
      if (parseNumero(linha, v) && v > DESNIVEL_FAIXA1 + TOLERANCIA) {
        Serial.println("Valor invalido: a altura nao pode ser maior que 1,50 m.");
      } else if (parseNumero(linha, v) && v > 0) {
        alturaSalva = v;
        temAltura = true;
        estado = PEDE_MEDIDA;
        pedirMedida();
      } else {
        Serial.println("Valor invalido. Digite a altura em metros, maior que 0 (ex.: 0,75).");
      }
      break;

    case PEDE_MEDIDA: {
      float x, y, umid;
      bool leu = false;
      if (cmd == "ok") {
        leu = lerMedidas(false, 0, x, y, umid);
      } else if (parseNumero(linha, v)) {
        leu = lerMedidas(true, v, x, y, umid);
      } else {
        Serial.println("Entrada invalida. Digite 'ok' para usar a leitura do sensor.");
        break;
      }
      if (!leu) break;                           // mensagem ja impressa; continua esperando
      estado = OCIOSO;
      Serial.printf("Leitura registrada: X=%.2f%% | Y=%.2f%% | umid=%.1f%%\n", x, y, umid);
      avaliarRampa(alturaSalva, x, y, umid);
      break;
    }
  }
}

// Le os caracteres disponiveis sem esperar; processa a linha quando chega o Enter
void lerSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String linha = linhaSerial;
      linhaSerial = "";
      linha.trim();
      if (linha.length() > 0) tratarEntrada(linha);
    } else if (linhaSerial.length() < 40) {
      linhaSerial += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  if (iniciarMPU()) {
    Serial.println("MPU6050 encontrado");
  } else {
    Serial.println("MPU6050 NAO encontrado: confira SDA no 32 e SCL no 33");
  }
  pinMode(LED_VERDE, OUTPUT); pinMode(LED_AMARELO, OUTPUT); pinMode(LED_VERMELHO, OUTPUT);
  sinalizar(-1);
  conectarWiFi();
  Serial.println("\nAcessaRio - avaliacao de rampa (NBR 9050) | modo sob demanda");
  ajuda();
}

void loop() {
  lerSerial();                                   // so reage ao que voce digita
  delay(10);
}