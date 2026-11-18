# AcessaRio | Ponto de Rampa Monitorado (ESP32)

**FIAP | Global Solution 2026.2 | CitizenTech | Engenharia de Software, 1º ano | Rio de Janeiro**
Entrega de **Edge Computing & Computer Systems**

> O **AcessaRio** é a solução da equipe para o desafio CitizenTech: um mapeamento colaborativo da acessibilidade urbana no Rio de Janeiro. Este repositório contém a entrega de **Edge Computing**: um protótipo IoT com ESP32, simulado no Wokwi, que envia os dados ao ThingSpeak.

## Integrantes

| Nome completo | RM |
|---|---|
| Arthur Bergami Lucas | 570679 |
| Davi Martins Herculano | 572699 |
| Felipe Braga Terzella | 573529 |

## Links da entrega

| Item | Link |
|---|---|
| Projeto no Wokwi | https://wokwi.com/projects/480781968724411393 |
| Canal público do ThingSpeak | https://thingspeak.com/channels/3578996 |

## Ficha do projeto

1. **Nome da CitizenTech:** AcessaRio
2. **Cidade e bairro:** Município do Rio de Janeiro, todos os bairros.
3. **Cidadão beneficiado:** Pessoas com mobilidade reduzida, usuários de cadeira de rodas, pessoas com deficiência visual e idosos.
4. **Problema, em uma frase:** A falta de informações acessíveis e atualizadas sobre as condições de mobilidade urbana dificulta a circulação independente e segura de pessoas com deficiência e mobilidade reduzida.
5. **Como funciona:** Aplicativo colaborativo que mapeia as condições de acessibilidade urbana do Rio de Janeiro, permitindo que usuários compartilhem informações sobre obstáculos, rampas, calçadas e outros pontos de circulação, auxiliando na escolha de trajetos mais acessíveis.
6. **O que cada parte faz:** Este repositório é a parte de **Edge Computing**: um protótipo com ESP32, DHT22 e MPU6050 que mede a umidade e a inclinação de uma rampa (longitudinal X e transversal Y), avalia pela ABNT NBR 9050, sinaliza o resultado com LEDs verde, amarelo e vermelho e envia cada avaliação ao ThingSpeak.
7. **Link do repositório:** https://github.com/felipeterzella/AcessaRio-Edge
8. **Indicador de impacto:** Número de pontos de acessibilidade mapeados e validados, quantidade de usuários beneficiados e locais com informações atualizadas sobre acessibilidade urbana.
9. **ODS conectados:**
   - ODS 10: Redução das Desigualdades.
   - ODS 11: Cidades e Comunidades Sustentáveis.
   - ODS 3: Saúde e Bem-Estar.

## 1. Problema

A falta de informação confiável e atualizada sobre as condições reais de mobilidade urbana dificulta o planejamento dos deslocamentos de pessoas com deficiência ou mobilidade reduzida. Uma pessoa pode sair de casa esperando encontrar uma rampa adequada e, no caminho, encontrar uma inclinação excessiva, um piso molhado ou escorregadio, ou uma rampa fora da norma. Sem dados objetivos, o usuário perde autonomia e previsibilidade, e pode ficar exposto a situações inseguras.

## 2. Objetivo da solução

O AcessaRio reúne informações georreferenciadas de acessibilidade urbana para apoiar deslocamentos mais seguros, informados e autônomos. Este protótipo representa um **ponto de rampa monitorado**: um dispositivo com ESP32 que mede a inclinação e a umidade de uma rampa, avalia os resultados pelos critérios da **ABNT NBR 9050** e sinaliza o estado com um semáforo de LEDs. Cada avaliação é enviada ao **ThingSpeak**, formando um histórico de monitoramento que pode alimentar a plataforma.

O protótipo:

- lê dois sensores (umidade e inclinação nos eixos X e Y);
- processa os dados localmente, no próprio ESP32;
- sinaliza a condição da rampa com LEDs verde, amarelo e vermelho e com mensagens no Serial Monitor;
- transmite os dados por Wi-Fi ao ThingSpeak, que guarda o histórico e mostra os gráficos.

## 3. Cidadão beneficiado

- Pessoas usuárias de cadeira de rodas, que precisam de rampas dentro da norma e de pisos seguros.
- Pessoas com deficiência física ou mobilidade reduzida.
- Pessoas idosas ou em recuperação temporária.
- Acompanhantes e cuidadores, que planejam deslocamentos com mais previsibilidade.
- Gestores públicos e organizações, que podem identificar rampas fora da norma ou em condição de risco.

## 4. ODS relacionados

- **ODS 11: Cidades e Comunidades Sustentáveis.** Cidades inclusivas, seguras e com acesso a espaços públicos para todos.
- **ODS 10: Redução das Desigualdades.** Inclusão de pessoas com deficiência e mobilidade reduzida.
- **ODS 3: Saúde e Bem-Estar.** Alertas sobre rampas fora da norma ou com piso possivelmente escorregadio ajudam a reduzir o risco de quedas e acidentes, e contribuem para a segurança e o bem-estar de quem circula.

## 5. Como o sistema funciona

O sistema trabalha **sob demanda**: nada roda sozinho. A leitura, a avaliação, o LED e o envio ao ThingSpeak só acontecem quando o usuário pede pelo Serial Monitor.

```
Serial Monitor  ->  ESP32 (lê DHT22 + MPU6050)  ->  avalia NBR 9050 + umidade
                                                     |-> LEDs (semáforo)
                                                     |-> relatório no Serial Monitor
                                                     '-> Wi-Fi -> ThingSpeak (gráficos e histórico)
```

### Comandos do Serial Monitor

| Comando | O que faz |
|---|---|
| `s` | Nova avaliação. Pede a altura vencida (desnível) da rampa em metros. |
| `ok` | Usa a leitura atual dos sensores (X, Y e umidade) para avaliar. |
| `m` | Mede de novo usando a altura já salva. |
| `r` | Repete o relatório da última avaliação. |
| `c` | Cancela a avaliação em andamento. |

### Critérios da NBR 9050 (rampa de 1 segmento)

O limite da inclinação longitudinal (**X**) depende do desnível vencido:

| Desnível (altura vencida) | Inclinação X máxima |
|---|---|
| até 0,80 m | 8,33% |
| acima de 0,80 até 1,00 m | 6,25% |
| acima de 1,00 até 1,50 m | 5,00% |
| acima de 1,50 m | reprovada |

A inclinação transversal (**Y**) tem limite fixo de **3%**, porque a rampa é considerada sempre em ambiente externo. A rampa só é **aprovada** se X e Y estiverem dentro dos limites.

### Semáforo (inclinação + umidade)

A cor do LED combina a situação da inclinação com a umidade do ar. A umidade fica em cinco níveis: baixa (< 50%), média (50 a 60%), média-alta (60 a 70%), alta (70 a 99,5%) e máxima (99,5% ou mais).

> **A umidade é só um indicador simulado.** O sensor mede a umidade do **ar**, e não a do piso. Umidade alta indica que pode chover e que o piso pode estar molhado ou escorregadio, mas **não confirma** isso. Por isso a umidade entra como **agravante** no semáforo, e as mensagens falam em piso "provavelmente molhado" ou "possivelmente escorregadio".

| Situação da inclinação | Baixa | Média | Média-alta | Alta | Máxima |
|---|---|---|---|---|---|
| Dentro dos critérios | Verde | Verde | Amarelo | Amarelo | Vermelho |
| No limite ou próxima do limite | Amarelo | Amarelo | Vermelho | Vermelho | Vermelho |
| Reprovada (X, Y ou desnível fora do limite) | Vermelho | Vermelho | Vermelho | Vermelho | Vermelho |

"Próxima do limite" significa X a partir de 6% (ou do limite do caso, se for menor) ou Y a partir de 2,5%.

- **Verde:** rampa em condições normais.
- **Amarelo:** atenção, usar com cuidado.
- **Vermelho:** perigo, evitar o trecho.

## 6. Componentes utilizados

| Componente | Função | Pino |
|---|---|---|
| ESP32 | Microcontrolador: processa os dados e envia por Wi-Fi | n/a |
| DHT22 | Sensor de umidade do ar | GPIO 4 |
| MPU6050 | Acelerômetro: inclinação nos eixos X e Y (I2C) | SDA 32, SCL 33 |
| LED verde | Rampa em condições normais | GPIO 25 |
| LED amarelo | Atenção | GPIO 26 |
| LED vermelho | Perigo | GPIO 27 |

> **Protótipo simulado.** Usamos o simulador **Wokwi** para **simular** o funcionamento: os valores do DHT22 e do MPU6050 são ajustados manualmente na tela para representar diferentes condições de rampa e de umidade. Em uma instalação real, o mesmo código seria usado com sensores físicos na rampa.

No Wokwi, os controles do MPU6050 variam de 0,1 em 0,1 g. Com `MODO_WOKWI = true` e `ESCALA_WOKWI = 10`, cada passo vale 1% de inclinação. Em um sensor físico, usa-se `MODO_WOKWI = false`, que aplica a fórmula real (tangente do ângulo).

## 7. ThingSpeak

Cada avaliação envia uma atualização ao canal. Campos usados:

| Campo | Conteúdo |
|---|---|
| field1 | Umidade (%) |
| field2 | Inclinação X longitudinal (%, em módulo) |
| field3 | Status do semáforo: 0 verde, 1 amarelo, 2 vermelho |
| field4 | Inclinação Y transversal (%, em módulo) |
| field5 | Resultado da rampa: 1 aprovada, 2 reprovada |
| field6 | Desnível da rampa (m) |

O plano gratuito do ThingSpeak aceita uma atualização a cada **15 segundos**. Avaliações em intervalo menor não são salvas.

## 8. Como executar a simulação

### Passo a passo

1. Abra o projeto no Wokwi pelo link acima e clique em **Start Simulation**.
2. No Serial Monitor, digite `s` e informe a altura da rampa em metros (ex.: `0,75`).
3. Ajuste os valores do MPU6050 e do DHT22 no Wokwi (veja a tabela abaixo).
4. No Serial Monitor, digite `ok` para o ESP32 ler os sensores e avaliar.
5. Veja o LED, o relatório no Serial Monitor e o novo ponto nos gráficos do ThingSpeak.
6. Para testar outra condição com a mesma altura, ajuste os sensores e digite `m`, depois `ok`. Para mudar a altura, digite `s`.
7. Varie a altura, as inclinações e a umidade para ver o semáforo mudar entre verde, amarelo e vermelho.

### Como ajustar os sensores no Wokwi

Clique no sensor na tela do Wokwi para abrir os controles dele.

| Sensor | Controle no Wokwi | O que representa | Como converter |
|---|---|---|---|
| MPU6050 | Aceleração **X** | Inclinação **X** (longitudinal, no sentido da subida) | aceleração em g x 10 = inclinação em % |
| MPU6050 | Aceleração **Y** | Inclinação **Y** (transversal, para o lado) | aceleração em g x 10 = inclinação em % |
| MPU6050 | Aceleração **Z** | Só confere se o sensor está deitado na rampa | deixar sempre em **1 g** |
| DHT22 | Umidade | Umidade do ar | valor direto em % |

Exemplos de conversão (X ou Y):

| Aceleração no Wokwi | Inclinação |
|---|---|
| 0,1 g | 1% |
| 0,3 g | 3% |
| 0,4 g | 4% |
| 0,5 g | 5% |
| 0,8 g | 8% |
| 1,0 g | 10% |

Observações:

- O **Z deve ficar em 1 g**. Se o Z for menor que 0,5 g, o ESP32 não avalia e mostra a mensagem "MPU6050 fora da posicao".
- A conversão só vale na simulação (`MODO_WOKWI = true`). Com sensor físico, o código calcula a inclinação real pela tangente do ângulo.
- Valores negativos de X e Y também funcionam: a avaliação usa o valor em módulo.
- O ThingSpeak do plano gratuito aceita uma atualização a cada **15 segundos**. Se aparecer `entry_id: 0` no Serial Monitor, o envio foi recusado por ser rápido demais e a avaliação não foi salva no canal.
- A altura da rampa (desnível) não é lida por sensor: ela é digitada no Serial Monitor, com no máximo **1,50 m**.

## 9. Evidências

| Evidência | Arquivo |
|---|---|
| Circuito no Wokwi | `evidencias/wokwi-circuito.png` |
| Semáforo verde | `evidencias/led-verde.png` |
| Semáforo amarelo | `evidencias/led-amarelo.png` |
| Semáforo vermelho | `evidencias/led-vermelho.png` |
| Gráficos do ThingSpeak | `evidencias/thingspeak-graficos.png` |
| Comando `m` (medir de novo com a altura salva) | `evidencias/serial-comando-m.png` |
| Comando `r` (repetir o relatório) | `evidencias/serial-comando-r.png` |
| Comando `c` (cancelar a avaliação) | `evidencias/serial-comando-c.png` |

## 10. Limitações

- Este protótipo roda em um **simulador (Wokwi)**: os valores dos sensores são ajustados manualmente para **simular** condições de rampa e de umidade, e não vêm de sensores instalados em rampas reais.
- A leitura é **indicativa**: um sensor simulado ou de baixo custo **não certifica** a acessibilidade de uma rampa, e uma avaliação com este protótipo não substitui a vistoria técnica.
- O protótipo avalia rampas de **um segmento**, só com o desnível informado manualmente.
- O resultado depende da qualidade das leituras dos sensores.
- A umidade do ar é uma **estimativa indireta**: não garante chuva nem piso molhado, e um piso úmido com ar seco não seria detectado.

## 11. Estrutura do repositório

```
.
|-- README.md          (este arquivo)
|-- sketch.ino         (código do ESP32)
'-- evidencias/        (prints da simulação e do ThingSpeak)
```
