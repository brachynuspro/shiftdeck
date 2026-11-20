# ShiftDeck - Ferramentas e Workflow

## 1. PlatformIO (Build & Flash)

### Instalacao

```bash
pip3 install platformio
```

**Versao usada**: PlatformIO Core 6.1.19

### Comandos Essenciais

| Comando | Descricao |
|---|---|
| `pio run` | Compilar o firmware |
| `pio run --target upload` | Compilar + flashear (porta auto-detect) |
| `pio run --target uploadfs` | Upload da imagem LittleFS (data/) |
| `pio run --target upload --upload-port /dev/cu.usbserial-1440` | Flash em porta especifica |
| `pio run --target uploadfs --upload-port /dev/cu.usbserial-1440` | Flash filesystem em porta especifica |
| `pio device monitor` | Monitor serial (requer terminal interativo) |
| `pio lib list` | Listar bibliotecas instaladas |

### Workflow Completo de Flash

```bash
cd ShiftDeck-CYD

# 1. Compilar e verificar erros
python3 -m platformio run

# 2. Upload da imagem LittleFS (data/default_cover.jpg, etc)
python3 -m platformio run --target uploadfs --upload-port /dev/cu.usbserial-1440

# 3. Upload do firmware
python3 -m platformio run --target upload --upload-port /dev/cu.usbserial-1440

# 4. Verificar logs
python3 -c "
import serial
ser = serial.Serial('/dev/cu.usbserial-1440', 115200, timeout=2)
while True:
    if ser.in_waiting > 0:
        print(ser.readline().decode('utf-8', errors='replace').strip())
"
```

### platformio.ini - Configuracoes Chave

```ini
[env:cyd]
platform = espressif32@~6.10.0    # Framework ESP32 Arduino 3.x
board = esp32dev                   # Board generica ESP32
framework = arduino

# Pinos do display ILI9341 (definidos via build_flags)
build_flags =
    -DILI9341_2_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_BL=21

# Bibliotecas
lib_deps =
    bodmer/TFT_eSPI@^2.5.43         # Driver display
    bodmer/TJpg_Decoder@^1.1.0      # Decodificador JPEG
    bblanchon/ArduinoJson@^7.2.1    # Parse JSON
    ricmoo/QRCode@^0.0.1            # QR code WiFi setup
    witnessmenow/spotify-api-arduino # API Spotify
    arkhipenko/TaskScheduler@^3.8.5  # Agendador
    madhephaestus/ESP32Encoder@^0.11.0

# Particionamento
board_build.partitions = partitions/cyd_4mb.csv
# 3MB app (0x10000-0x310000) + 1MB spiffs/LittleFS (0x310000-0x400000)
```

### Esptool (interno do PlatformIO)

O PlatformIO gerencia o esptool automaticamente. Configuracoes:

| Parametro | Valor |
|---|---|
| Chip | ESP32-D0WD-V3 (revision v3.1) |
| Crystal | 40 MHz |
| Flash | 4 MB |
| Velocidade upload | 460800 baud |
| Porta | /dev/cu.usbserial-1440 (CH340) |
| MAC | d4:8a:fc:a7:bc:98 |

### Particionamento (cyd_4mb.csv)

```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
spiffs,   data, spiffs,  0x310000,0xF0000,
```

---

## 2. Clasp (Google Apps Script CLI)

### Instalacao

```bash
npm install @google/clasp --prefix ~/.local
```

**Binario**: `~/.local/node_modules/.bin/clasp`

### Autenticacao

```bash
~/.local/node_modules/.bin/clasp login --no-localhost
```

1. Abrir a URL exibida no terminal no navegador
2. Autorizar o clasp a acessar o Google Apps Script
3. Copiar o codigo de autorizacao do navegador
4. Colar no terminal

Credenciais salvas em `~/.clasprc.json`.

### Comandos Essenciais

| Comando | Descricao |
|---|---|
| `clasp login --no-localhost` | Autenticar no Google |
| `clasp clone <SCRIPT_ID>` | Baixar projeto existente |
| `clasp pull` | Baixar ultima versao do servidor |
| `clasp push` | Subir alteracoes locais |
| `clasp deploy -d "descricao"` | Criar novo deploy |
| `clasp deployments` | Listar todos os deploys |
| `clasp deploy --deploymentId <ID>` | Atualizar deploy existente |
| `clasp status` | Verificar status do projeto |
| `clasp open` | Abrir no editor web |

### Workflow Completo de Update

```bash
cd appscript

# 1. Puxar versao mais recente (opcional)
~/.local/node_modules/.bin/clasp pull

# 2. Editar o codigo
vim Codigo.js

# 3. Subir alteracoes
~/.local/node_modules/.bin/clasp push

# 4. Criar novo deploy
~/.local/node_modules/.bin/clasp deploy -d "descricao da mudanca"

# Output:
# Deployed AKfycbxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx @N

# 5. Verificar deploys ativos
~/.local/node_modules/.bin/clasp deployments
```

### Script ID do Projeto

```
<SEU_SCRIPT_ID>   # fica em appscript/.clasp.json, que nao vai para o git
```

### Estrutura do Projeto Apps Script

```
appscript/
├── .clasp.json          # Configuracao do clasp (script ID, root)
├── appsscript.json      # Configuracao do projeto (runtime, webapp)
└── Codigo.js            # Codigo fonte (doGet, handlers)
```

### appsscript.json

```json
{
  "timeZone": "America/Sao_Paulo",
  "dependencies": {},
  "exceptionLogging": "STACKDRIVER",
  "runtimeVersion": "V8",
  "webapp": {
    "executeAs": "USER_DEPLOYING",
    "access": "ANYONE_ANONYMOUS"
  }
}
```

### Historico de Deploys

Cada `clasp deploy` gera um Deployment ID (`AKfycb...`). Liste com `clasp deployments`.

### URL do Web App

```
https://script.google.com/macros/s/<DEPLOYMENT_ID>/exec
```

E essa URL que vai no campo "Apps Script URL" da pagina de configuracao da ShiftDeck e nas Redirect URIs do app no Spotify Dashboard.

Esta URL deve estar:
1. No campo `apps_script_url` do dispositivo (config.json)
2. Nas **Redirect URIs** do Spotify Dashboard

---

## 3. Monitor Serial

### Leitura rapida via Python

```bash
python3 -c "
import serial
ser = serial.Serial('/dev/cu.usbserial-1440', 115200, timeout=2)
while True:
    if ser.in_waiting > 0:
        print(ser.readline().decode('utf-8', errors='replace').strip())
"
```

### Leitura com reset (DTR toggle)

```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-1440', 115200, timeout=2)
ser.dtr = False; ser.rts = True; time.sleep(0.1)
ser.dtr = True; time.sleep(0.1); ser.rts = False
time.sleep(3)
while True:
    if ser.in_waiting > 0:
        print(ser.readline().decode('utf-8', errors='replace').strip())
"
```

### Leitura por tempo limitado

```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-1440', 115200, timeout=2)
start = time.time()
while time.time() - start < 30:
    if ser.in_waiting > 0:
        print(ser.readline().decode('utf-8', errors='replace').strip())
ser.close()
"
```

---

## 4. Spotify Dashboard

### URL
```
https://developer.spotify.com/dashboard
```

### Configuracao do App Spotify

1. Acessar o dashboard
2. Criar um App (ou usar existente)
3. Copiar **Client ID** e **Client Secret**
4. Em **Settings** → **Redirect URIs** → adicionar a URL do Apps Script (@8):
   ```
   https://script.google.com/macros/s/<DEPLOYMENT_ID>/exec
   ```
5. Clicar **Save**

### Configurar no Dispositivo

1. Conectar ao WiFi do dispositivo
2. Acessar `http://<IP_DO_ESP>/` (ex: `http://192.168.0.19`)
3. Preencher:
   - **Client ID**
   - **Client Secret**
   - **Apps Script URL** (URL @8 acima)
4. Salvar → Dispositivo reinicia

---

## 5. Fluxo Completo de Desenvolvimento

```
┌─────────────────────────────────────────────────────────────┐
│               CICLO DE DESENVOLVIMENTO                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  MUDANCA NO FIRMWARE (ESP32)                                 │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ 1. Editar src/*.cpp ou src/Views/*.cpp                 │ │
│  │ 2. pio run (verificar compilacao)                      │ │
│  │ 3. pio run --target upload --upload-port PORT          │ │
│  │ 4. Verificar logs seriais                              │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  MUDANCA NO APPS SCRIPT                                     │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ 1. cd appscript/                                        │ │
│  │ 2. Editar Codigo.js                                     │ │
│  │ 3. clasp push                                           │ │
│  │ 4. clasp deploy -d "descricao"                          │ │
│  │ 5. Atualizar URL no Spotify Dashboard (se mudou)        │ │
│  │ 6. Reconfigurar dispositivo com nova URL (se mudou)     │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  MUDANCA NOS DADOS (LittleFS)                                │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ 1. Adicionar/editar arquivos em data/                   │ │
│  │ 2. pio run --target uploadfs --upload-port PORT        │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Resumo de Portas e Dispositivos

| Dispositivo | Porta | Descricao |
|---|---|---|
| ESP32 CYD | `/dev/cu.usbserial-1440` | CH340 USB-UART |
| ESP32 CYD | `192.168.0.19` | IP na rede local (WiFi STA) |
| ESP32 CYD | `192.168.4.1` | IP no modo AP (ShiftDeck-Setup) |
| Captive Portal | Porta 80 | Servidor web config WiFi |
| Spotify Config | Porta 80 | Servidor web credenciais Spotify |
| OAuth Server | Porta 80 | Servidor web autorizacao Spotify |

---

## 7. Dicas e Troubleshooting

### Erro: porta serial ocupada
```bash
# Verificar processos usando a porta
lsof /dev/cu.usbserial-1440
# Matar o processo
kill -9 <PID>
```

### Erro: PlatformIO nao encontra a porta
```bash
ls /dev/cu.*
# Deve listar /dev/cu.usbserial-1440
```

### Erro: clasp nao autenticado
```bash
# Verificar credenciais
cat ~/.clasprc.json
# Reautenticar
~/.local/node_modules/.bin/clasp login --no-localhost
```

### Erro: deploy read-only
```bash
# Listar deployments existentes
~/.local/node_modules/.bin/clasp deployments
# Criar NOVO deploy em vez de atualizar
~/.local/node_modules/.bin/clasp deploy -d "descricao"
```

### Erro: redirect_uri mismatch no Spotify
- Verificar se a URL do Apps Script no Spotify Dashboard **bate exatamente** com a configurada no dispositivo
- Atualizar ambos se necessario
