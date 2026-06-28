# `IoT Space Launch Monitor`

> Plataforma de monitoramento em tempo real de uma base de lancamento espacial. ESP32 (Wokwi) detecta gas, temperatura e intrusos, classifica o risco em 4 estados e dispara alertas automaticos via MQTT, n8n, Supabase e Telegram. Controle remoto bidirecional via dashboard HTML. Global Solution 2026.1, FIAP.

---

## `Tecnologias`

![C++](https://img.shields.io/badge/C++-ESP32-blue)
![MQTT](https://img.shields.io/badge/MQTT-HiveMQ%20Cloud-orange)
![n8n](https://img.shields.io/badge/n8n-automacao-red)
![Supabase](https://img.shields.io/badge/Supabase-PostgreSQL-green)
![Telegram](https://img.shields.io/badge/Telegram-alertas-2CA5E0)
![Grafana](https://img.shields.io/badge/Grafana-dashboard-F46800)
![Python](https://img.shields.io/badge/Python-relatorio-blue)
![License](https://img.shields.io/badge/license-MIT-green)

---

## `Arquitetura`

```
ESP32 (Wokwi)
    |
    +-- DHT22 (temperatura + umidade)
    +-- MQ-2 AOUT/DOUT (gas %)
    +-- HC-SR04 (distancia / intrusao)
    +-- LEDs (verde/amarelo/vermelho/azul)
    +-- Buzzer
    |
    MQTT over TLS (HiveMQ Cloud :8883)
        |
        +-- topico telemetria   fiap/iot/lancamento/PLT-01/telemetria
        +-- topico alerta       fiap/iot/lancamento/PLT-01/alerta
        +-- topico comando      fiap/iot/lancamento/PLT-01/comando
        |
        n8n Workflow
            +-- Supabase (PostgreSQL): historico de leituras
            +-- Telegram Bot: alertas criticos em tempo real
            |
            Dashboard HTML     monitoramento tempo real
            Grafana Cloud      historico e analises
```

---

## `Estados do Sistema`

| Estado | Condicao | Indicadores |
|--------|---------|------------|
| **LIVRE** | Tudo normal | LED verde piscando (800ms) |
| **ATENCAO** | Temp > 40 C ou objeto se aproximando (<300cm) | LED amarelo fixo |
| **PERIGO** | Gas detectado OU intruso na zona (<150cm) | LED vermelho + buzzer 900Hz |
| **CRITICO** | Gas + temperatura critica simultaneos | Todos os LEDs + buzzer alternado 600/1400Hz |

---

## `Hardware (ESP32)`

| Componente | Pino | Funcao |
|-----------|------|--------|
| DHT22 | 15 | Temperatura e umidade |
| MQ-2 AOUT | 34 | Concentracao de gas (% via ADC) |
| MQ-2 DOUT | 35 | Alarme digital de gas |
| HC-SR04 TRIG | 5 | Disparo do ultrassonico |
| HC-SR04 ECHO | 19 | Leitura do eco |
| LED Verde | 2 | Estado LIVRE |
| LED Amarelo | 4 | Estado ATENCAO |
| LED Vermelho | 13 | Estado PERIGO / CRITICO |
| LED Azul | 26 | Estado CRITICO / controle remoto |
| Buzzer | 14 | Alarme sonoro |

---

## `Controle Remoto Bidirecional`

O dashboard envia comandos ao ESP32 via topico MQTT `PLT-01/comando`:

| Comando | Acao |
|---------|------|
| `LED_AZUL_ON` | Acende LED azul remotamente |
| `LED_AZUL_OFF` | Apaga LED azul remotamente |
| `BUZZER_ON` | Aciona buzzer por 500ms |
| `RESET` | Reseta todos os estados remotos |

---

## `Payload MQTT`

```json
{
  "tag": "PLT-01",
  "nome": "Plataforma de Lancamento Espacial",
  "local": "Base de Lancamento FIAP - SP",
  "estado": "PERIGO",
  "motivo": "VAZAMENTO DE GAS DETECTADO",
  "medicoes": {
    "temperatura": 38.5,
    "umidade": 45.2,
    "gas_pct": 67,
    "gas_alarme": true,
    "distancia_cm": 230,
    "intruso": false
  }
}
```

---

## `Arquivos`

| Arquivo | Descricao |
|---------|---------|
| `main.cpp` | Firmware ESP32: sensores, estados, MQTT bidirecional |
| `secrets.h.example` | Template de credenciais (copiar para `secrets.h`) |
| `diagram.json` | Circuito Wokwi completo |
| `dashboard_gs.html` | Dashboard HTML tempo real com controle remoto |
| `n8n_workflow_gs.json` | Workflow n8n: MQTT, Supabase, Telegram |
| `nodered_flow_gs.json` | Flow Node-RED alternativo |
| `grafana_dashboard.json` | Dashboard Grafana configurado para Supabase |
| `gerar_relatorio.py` | Gerador de relatorio Python a partir do historico |
| `insert_200.py` | Script de insercao de dados de teste no Supabase |
| `wokwi.toml` | Configuracao do simulador Wokwi |

---

## `Como executar`

### Simulacao (Wokwi)

```bash
# 1. Acesse wokwi.com e crie novo projeto ESP32
# 2. Cole diagram.json no circuito
# 3. Cole main.cpp no editor
# 4. Crie secrets.h com suas credenciais (ver secrets.h.example)
# 5. Execute a simulacao
```

### Credenciais

```bash
cp secrets.h.example secrets.h
# Edite secrets.h com suas credenciais HiveMQ Cloud
```

### Pipeline de dados

```
1. Importe n8n_workflow_gs.json no n8n e configure credenciais MQTT e Supabase
2. Abra dashboard_gs.html no browser (conecta direto ao HiveMQ via WebSocket)
3. Importe grafana_dashboard.json no Grafana Cloud conectado ao Supabase
```

### Relatorio Python

```bash
python -m venv .venv
.venv\Scripts\activate
pip install supabase pandas fpdf2
python gerar_relatorio.py
```

---

## `Conceitos aplicados`

- **`MQTT over TLS`**: comunicacao segura na porta 8883 com `WiFiClientSecure`
- **`Publicacao diferenciada`**: LIVRE/ATENCAO vai para topico de telemetria, PERIGO/CRITICO vai para topico de alerta
- **`Controle bidirecional`**: ESP32 subscreve no topico de comandos e executa acoes remotas
- **`Non-blocking loop`**: todas as operacoes usam `millis()` sem `delay()` no loop principal
- **`n8n como middleware`**: recebe MQTT, persiste no Supabase e dispara Telegram em uma unica automacao
- **`secrets.h no .gitignore`**: credenciais nunca vao para o repositorio

---

## `Licenca`

Distribuido sob a licenca MIT. Veja [LICENSE](LICENSE) para mais informacoes.

---

## `Autor`

**Arthur Baptista dos Santos**
RM 565346 · Inteligencia Artificial · FIAP 2025-2026

[![LinkedIn](https://img.shields.io/badge/LinkedIn-Arthur%20Baptista-0077B5?logo=linkedin)](https://linkedin.com/in/arthur-baptista-dos-santos)
[![GitHub](https://img.shields.io/badge/GitHub-Arthur--Baptista--dos--Santos-181717?logo=github)](https://github.com/Arthur-Baptista-dos-Santos)
