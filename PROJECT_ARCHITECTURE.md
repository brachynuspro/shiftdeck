# ShiftDeck - Arquitetura do Projeto

## Visao Geral

ShiftDeck e um controlador inteligente para Spotify, similar ao Spotify Car Thing, construido sobre ESP32 + Display Touch de 2.8". Funciona como extensao do celular, focado em reproducao de musica com interface rapida e navegacao simples.

### Caracteristicas Principais
- Grandes areas de toque, poucos cliques
- Inicializacao inferior a 5 segundos
- Tema Dark com destaque verde Spotify (#1ED760)
- Grid de 8px, margens 12px
- Foco em reproducao
- Configuracao 100% via rede WiFi

---

## Hardware

| Componente | Especificacao |
|---|---|
| **Board** | ESP32-2432S028 (Cheap Yellow Display / CYD) |
| **MCU** | ESP32-D0WD-V3, Dual Core 240MHz |
| **Display** | ILI9341 320x240, 2.8" TFT |
| **Touch** | XPT2046 resistivo, interface SPI |
| **Flash** | 4MB |
| **RAM** | 320KB |
| **Conectividade** | WiFi 2.4GHz b/g/n |

### Pinagem

| Funcao | GPIO | Interface |
|---|---|---|
| TFT_MOSI | 13 | HSPI |
| TFT_MISO | 12 | HSPI |
| TFT_SCLK | 14 | HSPI |
| TFT_CS | 15 | HSPI |
| TFT_DC | 2 | HSPI |
| TFT_BL | 21 | PWM |
| Touch CLK | 25 | VSPI |
| Touch MOSI | 32 | VSPI |
| Touch MISO | 39 | VSPI |
| Touch CS | 33 | VSPI |
| Touch IRQ | 36 | VSPI |

---

## Estrutura do Projeto

```
ShiftDeck-CYD/
├── platformio.ini              # Config build: ILI9341 driver, libs, pinos
├── partitions/cyd_4mb.csv      # 3MB app + 1MB LittleFS
├── generate_compile_time.py    # Script pre-build
├── data/
│   ├── default_cover.jpg       # Capa de album fallback
│   └── appscript_template.gs   # Template Google Apps Script
├── src/
│   ├── main.cpp                # Entry point, fluxo de boot, task handler
│   ├── config.h                # Tema, cores, layout, defines
│   ├── scui.h                  # Mensagens FreeRTOS (SCUI queue)
│   ├── logTags.h               # Tags de logging
│   │
│   ├── SCLogger.h/cpp          # Logger tag-based
│   ├── Monitor.h/cpp           # Monitor de heap, metricas
│   ├── Vault.h/cpp             # Configuracoes JSON no LittleFS
│   │
│   ├── SpotifyPlayer.h/cpp     # Controle de playback, search, playlists
│   ├── SpotifyArtMgr.h/cpp     # Cache LRU de capas de album
│   ├── SpotifyOAuth.h/cpp      # Fluxo OAuth (Apps Script bridge)
│   ├── SpotifyConfigServer.h/cpp # Pagina web de configuracao Spotify
│   ├── SpotifyCertWrapper.h/cpp  # Certificados SSL
│   ├── PlayingMetadata.h/cpp   # DTO de metadados da musica atual
│   │
│   ├── WiFiSetup.h/cpp         # Captive portal WiFi (AP + DNS)
│   ├── DisplayUI.h/cpp         # Renderizacao ILI9341 via TFT_eSPI
│   ├── XPT2046Touch.h/cpp      # Driver touch resistivo
│   │
│   ├── UIView.h/cpp            # Base abstrata para views
│   ├── UIViewManager.h/cpp     # Navegacao por stack de views
│   │
│   └── Views/
│       ├── WiFiSetupView.h/cpp # Tela de configuracao WiFi (QR code)
│       ├── SpotifyConfigView.h/cpp # Tela de config Spotify credentials
│       ├── BootView.h/cpp      # Splash com checklist de inicializacao
│       ├── HomeView.h/cpp      # Player principal (capa, controles)
│       ├── SearchView.h/cpp    # Busca com teclado touch
│       ├── PlaylistView.h/cpp  # Lista de playlists
│       └── SettingsView.h/cpp  # WiFi, brilho, dispositivo, diagnostico
```

---

## Tema e Layout

### Cores (RGB565, 16-bit)

| Nome | Hex | RGB565 | Uso |
|---|---|---|---|
| Background | #121212 | 0x1082 | Fundo principal |
| Cards | #1B1B1B | 0x18C3 | Cartoes, modais |
| Buttons | #222222 | 0x2104 | Botoes secundarios |
| Spot Green | #1ED760 | 0x1F39 | Destaque, botoes primarios |
| White | #FFFFFF | 0xFFFF | Texto primario |
| Secondary | #B3B3B3 | 0xBDF7 | Texto secundario |

### Grid System
- Base: **8px**
- Margens externas: **12px**
- Espacamento: 8, 16, 24px
- Progress bar: 8px altura, 4px raio
- Botoes: raio 8-16px

---

## Arquitetura de Software

### Dual-Core (FreeRTOS Tasks)

```
┌─────────────────────────────────┐
│           ESP32                  │
│                                  │
│  Core 0 (PRO CPU)               │
│  ┌────────────────────────────┐ │
│  │ uiHandlerTask (~60 FPS)    │ │
│  │ • Touch input processing   │ │
│  │ • SCUI queue dispatch      │ │
│  │ • View rendering (dirty)   │ │
│  │ • Performance metrics      │ │
│  └────────────────────────────┘ │
│                                  │
│  Core 1 (APP CPU)               │
│  ┌────────────────────────────┐ │
│  │ refreshCurrentSongTask     │ │
│  │ • Spotify API polling      │ │
│  │ • Album art download       │ │
│  │ • Cache management         │ │
│  ├────────────────────────────┤ │
│  │ loop()                     │ │
│  │ • WiFi portal handleLoop   │ │
│  │ • Heap monitoring          │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘
```

### Inter-Thread Communication
- **SCUI Queue** (FreeRTOS): mensagens entre Spotify refresh task e UI task
- Tipos: `UM_IDLE`, `UM_STATUS_BOX`, `UM_MARK_DIRTY`, `UM_PLAYER_REFRESH`
- **Semaphores**: `xSemaphoreDisplay` (TFT), `xSemaphoreNetwork` (Spotify API), `xSemaphoreDataCopy` (metadata)

### Padroes de Design
- **Singleton**: SpotifyPlayer, Vault, UIViewManager, SCLogger, Monitor, SpotifyArtMgr
- **Observer**: Callbacks da API Spotify notificam SpotifyPlayer
- **Strategy**: Renderizadores de UIElement
- **Producer-Consumer**: SCUI queue entre threads

---

## Fluxo de Inicializacao (Boot)

```
┌──────────────────────────────────────────────────────────────────┐
│                         POWER ON                                  │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  FASE 1: Hardware Init                                            │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ ILI9341 display → XPT2046 touch → LittleFS mount            │ │
│  │ Vault::initialize() (load config.json)                      │ │
│  │ SpotifyPlayer::initialize()                                 │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           ↓                                       │
│  FASE 2: WiFi Setup                                               │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ Check: vault.getWiFiSSID()                                   │ │
│  │ ├── Empty → WiFiSetupView + captive portal (espera config)   │ │
│  │ └── OK → WiFi.begin()                                        │ │
│  │         ├── Fail → WiFiSetupView (reconfigurar)              │ │
│  │         └── OK → continue                                    │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           ↓                                       │
│  FASE 3: Spotify Credentials                                      │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ Check: client_id != default && apps_script_url != empty      │ │
│  │ ├── Fail → SpotifyConfigView + web server (espera config)    │ │
│  │ └── OK → continue                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           ↓                                       │
│  FASE 4: Boot Checklist                                           │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ BootView: Display [ok] → Touch [ok] → WiFi [ok]             │ │
│  │ → Backend [ok] → Spotify [loading...]                       │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           ↓                                       │
│  FASE 5: Spotify OAuth                                            │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ Check: isRefreshTokenAvailable()                             │ │
│  │ ├── Yes → login() → skip to Home                             │ │
│  │ └── No → OAuth screen → web page http://IP                   │ │
│  │         → User opens → clique Autorizar → login Spotify     │ │
│  │         → Apps Script (HTTPS) salva code                     │ │
│  │         → User copia code → cola no form → POST /savecode    │ │
│  │         → ESP32: HTTP POST /api/token → refresh_token        │ │
│  │         → Vault::save() → login() → HomeView                 │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           ↓                                       │
│  FASE 6: Run                                                      │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ viewMgr.gotoView(HomeView)                                   │ │
│  │ startBackgroundRefreshes() → Core 1 task                     │ │
│  │ Create SCUI queue + UI handler → Core 0 task                 │ │
│  │ loop() → idle                                                │ │
│  └─────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

---

## Telas da Interface

### 1. WiFi Setup
- QR code para conexao + SSID/Senha do AP
- Instrucoes passo a passo
- IP do captive portal

### 2. Spotify Config
- URL de configuracao no navegador
- Input: Client ID, Client Secret, Apps Script URL
- Salva em config.json → restart

### 3. Boot (Splash)
- Logo ShiftDeck
- Checklist: Display, Touch, WiFi, Backend, Spotify
- Progress bar animada
- Auto-avanca para Home

### 4. Home (Player)
- Topo: indicadores WiFi + Spotify
- Centro: capa album, nome musica, artista, album
- Botao like (coracao)
- Progresso: barra + tempo atual/total
- Rodape: anterior, play/pause, proxima

### 5. Search
- Campo de texto + teclado on-screen
- Lista de resultados com tipo (track/album/artist/playlist)
- Voltar + buscar

### 6. Playlists
- Lista scrollavel com capa, nome, qtd musicas
- Destaque verde no item selecionado
- Botao Tocar

### 7. Settings
- WiFi: scan, senha, reconectar
- Brilho: slider 0-100%
- Dispositivo Spotify: selecao
- Diagnostico: testes touch, display, WiFi
- Salvar/Voltar

---

## Integracao Spotify

### OAuth Flow (Google Apps Script Bridge)

```
Celular (Browser)                  ESP32                  Google Apps Script
┌──────────────────┐             ┌──────────┐             ┌───────────────────┐
│ http://ESP_IP     │───HTTP──→  │ WebServer│             │                   │
│                   │             │ :80      │             │                   │
│ AUTORIZAR SPOTIFY│             │ /        │             │                   │
│     ↓             │             │ /savecode│             │                   │
│ Spotify Login     │───HTTPS──→ accounts.spotify.com     │                   │
│     ↓             │             └──────────┘             │                   │
│ Redirect →        │───HTTPS──→                           │ ?code=XXX         │
│                   │             ┌───────────────────────→│ &state=ESP_IP     │
│                   │             │                        │                   │
│                   │             │  Salva code            │                   │
│                   │             │  Mostra na tela        │                   │
│                   │             │                        │                   │
│ Usuario copia     │             │                        │                   │
│ code da tela      │             │                        │                   │
│     ↓             │             │                        │                   │
│ Cola no form      │───HTTP──→   │ POST /savecode         │                   │
│                   │             │ code=XXX...            │                   │
│                   │             │                        │                   │
│                   │             │ HTTP POST              │                   │
│                   │             │ /api/token → refresh   │                   │
│                   │             │ accounts.spotify.com   │                   │
└──────────────────┘             └──────────┘             └───────────────────┘
```

### APIs Utilizadas
- `GET /v1/me/player/currently-playing` - Musica atual
- `PUT /v1/me/player/play` - Play/retomar
- `PUT /v1/me/player/pause` - Pausar
- `POST /v1/me/player/next` - Proxima
- `POST /v1/me/player/previous` - Anterior
- `GET /v1/search` - Busca
- `GET /v1/me/playlists` - Playlists do usuario

### Cache de Capas (SpotifyArtMgr)
- LRU (Least Recently Used) com maximo 8 arquivos
- Index persistido em `/cache_idx.txt`
- Eviction do mais antigo quando cheio
- Fallback: `/default_cover.jpg` se download falhar

---

## Configuracao (config.json)

```json
{
  "wifi_ssid": "MinhaRede",
  "wifi_pass": "minha_senha",
  "spotify_client_id": "SEU_CLIENT_ID",
  "spotify_client_secret": "SEU_CLIENT_SECRET",
  "spotify_refresh_token": "AQAB...",
  "apps_script_url": "https://script.google.com/macros/s/AKfycb.../exec",
  "brightness": 80,
  "timezone": "UTC-3"
}
```

---

## Dependencias

| Biblioteca | Versao | Uso |
|---|---|---|
| TFT_eSPI | ^2.5.43 | Driver display ILI9341 |
| TJpg_Decoder | ^1.1.0 | Decodificacao JPEG |
| ArduinoJson | ^7.2.1 | Parse/serialize JSON |
| SpotifyArduino | git | Cliente API Spotify |
| QRCode | ^0.0.1 | QR code WiFi setup |
| TaskScheduler | ^3.8.5 | Agendamento de tarefas |
| ESP32Encoder | ^0.11.0 | Encoder (futuro) |

### Bibliotecas Nativas ESP32
- **WiFi** - Conexao e AP mode
- **WebServer** - Servidor HTTP (OAuth, config)
- **DNSServer** - Captive portal DNS hijack
- **HTTPClient** - Requisicoes HTTPS
- **WiFiClientSecure** - SSL/TLS
- **LittleFS** - Sistema de arquivos
- **ESPmDNS** - Multicast DNS

---

## Performance

| Metrica | Alvo | Status |
|---|---|---|
| Boot time | < 5s | ~2s (sem OAuth) |
| FPS | 30+ | 60 FPS (16ms por frame) |
| Screen switch | < 150ms | Ok (redraw sincrono) |
| Busca | < 500ms | Pendente |
| Troca musica | Real-time | Pendente |
| RAM livre | > 200KB | ~210KB (17% usado) |
| Flash usado | < 80% | ~37% (1.1MB de 3MB) |

---

## Recursos Planejados
- [ ] Reproducao funcional com Spotify API
- [ ] Controles de volume
- [ ] Shuffle e repeat
- [ ] Fila de reproducao
- [ ] Favoritos / liked songs
- [ ] Atualizacao OTA
- [ ] Suporte a SD Card (cache expandido)
- [ ] Modo carro (interface simplificada)
- [ ] Sleep mode
- [ ] Apple Music / YouTube Music
