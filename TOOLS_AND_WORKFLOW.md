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
# 2 x 1,75MB app (OTA) + 448KB LittleFS (0x390000-0x400000)
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

### Gravar depois de uma atualizacao OTA

Depois que a placa se atualiza sozinha, ela passa a dar boot pelo `app1`. Gravar so o app em
`0x10000` (app0) nao muda nada. Use `pio run -t upload` (que tambem grava `boot_app0.bin` em
`0xe000`) ou inclua esse arquivo no esptool:

```bash
esptool.py --chip esp32 write_flash -z \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/cyd/firmware.bin
```

### Particionamento (cyd_4mb.csv)

```
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1C0000,
app1,     app,  ota_1,   0x1D0000, 0x1C0000,
spiffs,   data, spiffs,  0x390000, 0x70000,
```

---

## 2. Login do Spotify (pagina de retorno)

O Spotify so devolve o login para enderecos HTTPS publicos, e a placa serve HTTP na rede local.
A pagina estatica `ShiftDeck-CYD/flasher/callback.html` (publicada junto com o instalador) recebe
`?code=...&state=<IP da placa>` e manda o navegador para `http://<IP>/code?code=...`.

- URL fixa (Redirect URI do app no Spotify): `https://brachynuspro.github.io/shiftdeck-flasher/callback.html`
- No firmware: `OAUTH_REDIRECT_URI` em `src/config.h`.
- So aceita IPs de rede local (10.x, 172.16-31.x, 192.168.x) no `state`.
- Substituiu a ponte antiga no Google Apps Script (removida na 1.3.0).

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
3. Copiar o **Client ID** (o Client Secret nao e usado: login PKCE)
4. Em **Settings** → **Redirect URIs** → adicionar:
   ```
   https://brachynuspro.github.io/shiftdeck-flasher/callback.html
   ```
5. Clicar **Save**

### Configurar no Dispositivo

1. Conectar ao WiFi do dispositivo
2. Acessar `http://<IP_DO_ESP>/` (ex: `http://192.168.0.19`)
3. Preencher:
   - **Client ID**
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

### Erro: redirect_uri mismatch no Spotify
- A Redirect URI do app no Spotify Dashboard precisa ser **exatamente** `https://brachynuspro.github.io/shiftdeck-flasher/callback.html`
