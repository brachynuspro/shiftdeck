# ShiftDeck - Jornada de Desenvolvimento

## Cronologia de Tentativas, Falhas e Sucessos

Este documento registra todas as abordagens tentadas durante o desenvolvimento do ShiftDeck, documentando o que funcionou, o que falhou e as razoes tecnicas por tras de cada decisao.

---

## Fase 1: Portabilidade do Projeto Original

### Objetivo
Portar o codigo do **Color Kit Grande Spotify Controller** (ThingPulse, ILI9488 480x320, FT6236 touch) para o **ESP32-2432S028 CYD** (ILI9341 320x240, XPT2046 touch).

### Mudancas de Hardware

| Componente | Original | CYD |
|---|---|---|
| Display Driver | ILI9488 | ILI9341 |
| Resolucao | 480x320 | 320x240 |
| Touch | FT6236 (I2C, capacitivo) | XPT2046 (SPI, resistivo) |
| Display Pins | MOSI=18, MISO=19, SCLK=5, CS=15, DC=2, RST=4 | MOSI=13, MISO=12, SCLK=14, CS=15, DC=2, RST=-1 |
| Backlight | GPIO 32 | GPIO 21 |

### Resultado: SUCESSO ✅
- Display ILI9341 inicializando corretamente com TFT_eSPI
- Touch XPT2046 respondendo via SPI
- Resolucao 320x240 com grid 8px adaptado

---

## Fase 2: Interface e Telas

### Objetivo
Recriar a interface completamente conforme especificacao do `PROJETO_SPOTIFY_CAR_ESP32_v1.2.md` com 5 telas.

### O que foi feito
- Tema dark (#121212, #1B1B1B, #1ED760)
- Grid 8px, margens 12px
- 5 telas: WiFi Setup, Boot, Home, Search, Playlists, Settings
- Fontes TFT_eSPI built-in (evita OpenFontRender)
- UIViewManager com stack de navegacao

### Resultado: SUCESSO ✅

---

## Fase 3: Configuracao WiFi

### Objetivo
Permitir que o usuario configure a rede WiFi do dispositivo sem precisar editar codigo fonte.

### Tentativa 1: WiFiManager (AP + Captive Portal)
- Cria AP "ShiftDeck-Setup"
- DNS hijack redireciona todas requisicoes para pagina web
- Pagina com scan de redes + input de senha
- Salva em config.json no LittleFS

### Resultado: SUCESSO ✅
- QR code na tela para facilitar conexao
- Senha no AP (12345678)
- Pagina web tema dark estilizada

---

## Fase 4: Configuracao Spotify Credentials

### Objetivo
Evitar hardcoding de client_id e client_secret. Permitir configuracao via web.

### Implementacao
- SpotifyConfigServer na porta 80 (mesma rede WiFi)
- Pagina web com campos: Client ID, Client Secret, Apps Script URL
- Salva em config.json → ESP.restart()

### Resultado: SUCESSO ✅

---

## Fase 5: Autenticacao Spotify (OAuth) - A Grande Saga

Esta foi a parte mais complexa. Foram necessarias **9 tentativas diferentes** ate encontrar uma abordagem funcional.

---

### Tentativa 1: mDNS Local (shiftdeck.local)

**Abordagem**: Servidor web local no ESP32 acessivel via `http://shiftdeck.local`.

**Fluxo**: Usuario abre `http://shiftdeck.local` → pagina com link de autorizacao → Spotify OAuth → redirect para `http://shiftdeck.local/callback/`

**Problema**: 
- mDNS nao funciona em todas as redes
- Spotify Dashboard exige HTTPS para redirect URIs em producao

**Resultado**: ❌ FALHOU

---

### Tentativa 2: Redirect HTTP via Apps Script

**Abordagem**: Usar Google Apps Script como ponte HTTPS. Apps Script recebe o code do Spotify e redireciona para o ESP32 via HTTP.

**Fluxo**: Spotify → Apps Script (HTTPS, salva code) → redireciona navegador para `http://192.168.0.19/code?code=XXX`

**Problema**:
- `HtmlService.createHtmlOutput()` sanitiza HTML e remove `<meta http-equiv="refresh">`
- `window.location.href` em JavaScript nao funciona no sandbox do HtmlService

**Resultado**: ❌ FALHOU

---

### Tentativa 3: JSONP (Script Tag Injection)

**Abordagem**: Pagina servida pelo ESP32 insere `<script src="https://script.google.com/...?callback=gotCode">`. Browser carrega como script (isento de CORS). Apps Script retorna `gotCode("CODE")` como JavaScript. Funcao gotCode envia POST para ESP32.

**Problema**: 
- Chrome/Safari bloqueiam "mixed content": carregar script HTTPS em pagina HTTP
- ESP32 teria que servir HTTPS (certificado self-signed gera warning no browser)

**Resultado**: ❌ FALHOU

---

### Tentativa 4: Same-Tab Redirect com Meta Refresh

**Abordagem**: Usuario abre `http://ESP_IP` → clica AUTORIZAR → navega para Spotify (mesma aba) → autoriza → Spotify redireciona para Apps Script → Apps Script retorna HTML com `<meta http-equiv="refresh" content="0;url=http://ESP_IP/code?code=XXX">` → navegador segue redirect → ESP32 recebe code.

**Problema**:
- `HtmlService.createHtmlOutput().setXFrameOptionsMode(...)` sanitiza o meta refresh
- `ContentService.createTextOutput().setMimeType(HTML)` tambem falha com browser bloqueando HTTPS→HTTP redirect

**Resultado**: ❌ FALHOU

---

### Tentativa 5: Codigo Visivel + Copy-Paste (ContentService MIME HTML)

**Abordagem**: Apps Script mostra o codigo de autorizacao VISUALMENTE na tela usando `ContentService.createTextOutput()` com MIME `text/html` (sem sanitizacao). Usuario copia o codigo da tela do celular.

**Problema**: 
- O codigo e exibido, mas nao ha mecanismo para o usuario COLA-LO no ESP32
- Nenhum input field na pagina do ESP32

**Resultado**: ❌ FALHOU (parcial - codigo visivel, mas sem input)

---

### Tentativa 6: Codigo Visivel + Input Manual (fetch API)

**Abordagem**: Apps Script mostra o codigo na tela. Pagina do ESP32 tem campo de texto + botao SALVAR. JavaScript `fetch()` envia POST `/savecode` para o ESP32 com o codigo.

**Problema**:
- `fetch()` falha silenciosamente. O ESP32 WebServer nunca recebe o POST.
- Nenhum erro visivel no navegador (o `fetch` retorna mas a resposta nao chega)

**Resultado**: ❌ FALHOU (POST nunca chega)

---

### Tentativa 7: Formulario HTML Puro (sem JavaScript)

**Abordagem**: Substituir JavaScript `fetch()` por formulario HTML tradicional:

```html
<form method="POST" action="/savecode">
  <input type="text" name="code" required>
  <button type="submit">SALVAR</button>
</form>
```

**Vantagens**: Zero JavaScript. O mecanismo mais basico da web. Funciona em qualquer navegador.

**Resultado**: ✅ FUNCIONOU (codigo chega no ESP32)

---

### Tentativa 8: Token Exchange via Biblioteca (requestAccessTokens)

**Abordagem**: Usar `spotify.requestAccessTokens(code, redirect_uri)` da biblioteca SpotifyArduino para trocar o codigo de autorizacao por refresh token.

**Problema**:
- **Buffer overflow**: O codigo de autorizacao do Spotify tem ~571 caracteres, mas a biblioteca aloca apenas **500 bytes** no buffer
- `sprintf(body, requestAccessTokensBody, code, ...)` estoura o buffer
- Resultado: **STACK SMASHING** → CRASH → REBOOT

**Log**:
```
Stack smashing protect failure!
Backtrace: 0x400839c5:0x3ffb1ef0 0x4008f07d:0x3ffb1f10 ...
Rebooting...
```

**Resultado**: ❌ FALHOU (buffer overflow → crash)

---

### Tentativa 9: Token Exchange Direto (HTTPClient)

**Abordagem**: Substituir `spotify.requestAccessTokens()` por chamada HTTP direta com buffer dimensionado dinamicamente (usando `String` da Arduino):

```cpp
String body = "grant_type=authorization_code"
              "&code=" + authCode +
              "&redirect_uri=" + redirectUri +
              "&client_id=" + cid +
              "&client_secret=" + cse;

WiFiClientSecure ssl;
HTTPClient http;
http.begin(ssl, "accounts.spotify.com", 443, "/api/token", true);
int status = http.POST(body);
```

**Vantagens**: Sem limite de buffer. Sem crash. Controle total sobre a requisicao.

**Resultado**: ✅ FUNCIONOU (token exchange sem crash)

---

## Fase 6: Conexao SSL - Outra Saga

### Tentativa 1: Certificado Corrompido

**Problema**: O arquivo `SpotifyCertWrapper.cpp` continha um certificado `spotify_server_cert` truncado com `________________________________________` no meio. Copiado incorretamente do projeto original.

**Resultado**: ❌ FALHOU (certificado invalido)

---

### Tentativa 2: DigiCert Global Root G2 (Certificado Correto)

**Abordagem**: Substituir pelo certificado correto da biblioteca SpotifyArduino (`SpotifyArduinoCert.h`).

**Resultado**: ✅ FUNCIONOU (conexao SSL com Spotify API estabelecida)

---

### Tentativa 3: setInsecure() para Google Apps Script

**Abordagem**: Usar `WiFiClientSecure::setInsecure()` para pular verificacao SSL ao conectar com `script.google.com`.

**Problema**: 
- `setInsecure()` seta `MBEDTLS_SSL_VERIFY_NONE`
- O `ssl_client.cpp` do ESP32 Arduino 3.x tem um bug: mesmo com `VERIFY_NONE`, apos o handshake, `mbedtls_ssl_get_verify_result()` ainda retorna flags nao-zero
- Linha 292-296 verifica os flags e falha mesmo com insecure=true

**Log**:
```
(-9984) X509 - Certificate verification failed
```

**Resultado**: ❌ FALHOU (bug no ssl_client.cpp)

---

### Tentativa 4: Certificado GTS Root R1 para Google

**Abordagem**: Adicionar o certificado Google Trust Services Root R1 ao `gCombinedCerts` para verificar `script.google.com`.

**Problema**:
- GTS Root R1 usa algoritmo **RSA-PSS** (RSASSA-PSS)
- A build do mbedtls neste framework ESP32 nao suporta RSA-PSS para validacao de cadeia de certificados
- `mbedtls_x509_crt_parse()` falha com erro `PK - The pubkey tag or value is invalid (only RSA and EC are supported)`

**Log**:
```
(-15202) PK - The pubkey tag or value is invalid (only RSA and EC are supported)
```

**Resultado**: ❌ FALHOU (RSA-PSS nao suportado)

---

### Tentativa 5: Cert Bundle Nativo do ESP32

**Abordagem**: Usar `WiFiClientSecure::setCACertBundle(nullptr)` com o bundle de certificados embutido no ESP32. Adicionar `-DCONFIG_MBEDTLS_CERTIFICATE_BUNDLE=1` ao build.

**Problema**:
- O bundle `x509_crt_bundle` nao esta embedado no binario
- Erro de link: `x509_crt_bundle' not found`
- Apenas `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` nao e suficiente; precisa do arquivo binario do bundle

**Resultado**: ❌ FALHOU (bundle nao disponivel)

---

### Conclusao SSL para Google

**O ESP32 NAO consegue conectar via HTTPS ao `script.google.com` neste framework.** Todas as tentativas de verificacao SSL falharam:
1. `setInsecure()` → bug no ssl_client
2. Certificado GTS Root R1 → RSA-PSS nao suportado
3. Cert bundle → nao embedado

**Solucao final**: Nao fazer HTTPS do ESP32 para Google. Usar o **navegador do celular** para toda comunicacao HTTPS (Spotify OAuth, Apps Script). O ESP32 apenas serve HTTP local e recebe o codigo via formulario HTML.

---

## Fase 7: Playback Spotify - Status Atual

### Objetivo
Obter dados da musica atual (`/v1/me/player/currently-playing`) e exibir no display.

### O que esta implementado
- `SpotifyPlayer::refreshCurrentTrack()` - polling a cada ~3s
- `SpotifyPlayer::getCurrentlyPlayingCallback()` - callback da API
- `PlayingMetadata` - DTO para armazenar track, artist, album, progress
- `HomeView::draw()` - renderiza capa, nome, artista, album, progresso

### Problema atual
Os logs mostram que a API do Spotify retorna erro pois o `refresh_token` esta vazio:

```
grant_type=refresh_token&refresh_token=&client_id=...
Status: HTTP/1.0 400 Bad Request
getAuthToken error{"error":"invalid_request","error_description":"refresh_token must be supplied"}
```

**Causa raiz**: O token obtido na troca (Fase 5, Tentativa 9) precisa ser salvo corretamente no Vault e carregado pelo SpotifyPlayer no boot subsequente. O fluxo de salvar/carregar o refresh_token precisa ser verificado.

### Proximos passos
1. Verificar se o token esta sendo salvo corretamente no `config.json`
2. Verificar se o `Vault::load()` esta lendo `spotify_refresh_token` do JSON
3. Verificar se o `SpotifyPlayer::login()` esta usando o token correto
4. Testar a chamada `GET /v1/me/player/currently-playing`

---

## Resumo: O Que Funcionou

| Componente | Status |
|---|---|
| Display ILI9341 320x240 | ✅ |
| Touch XPT2046 | ✅ |
| LittleFS filesystem | ✅ |
| Captive portal WiFi com QR code | ✅ |
| Configuracao web de credenciais Spotify | ✅ |
| Apps Script bridge (OAuth) | ✅ |
| Exibicao do codigo de autorizacao | ✅ |
| Formulario HTML puro (input code) | ✅ |
| Certificado SSL Spotify (DigiCert G2) | ✅ |
| Token exchange direto (HTTPClient) | ✅ |
| Interface 5 telas (UIViewManager) | ✅ |
| Dual-core (UI + Spotify refresh) | ✅ |
| Tema dark + grid 8px | ✅ |

## Resumo: O Que Falhou

| Componente | Motivo |
|---|---|
| mDNS (shiftdeck.local) | Nao confiavel em todas as redes |
| Redirect HTTP (Spotify) | Spotify exige HTTPS em producao |
| JSONP polling | Mixed content bloqueado |
| Meta refresh redirect | Sanitizado pelo HtmlService |
| JavaScript fetch() | Falha silenciosa no POST |
| setInsecure() SSL | Bug no ssl_client.cpp (VERIFY_NONE) |
| Certificado GTS Root R1 | RSA-PSS nao suportado pelo mbedtls |
| Cert bundle ESP32 | Bundle nao embedado no binario |
| requestAccessTokens() lib | Buffer overflow (500 bytes vs 571) |
| Spotify playback display | refresh_token vazio (em andamento) |

---

## Licoes Aprendidas

1. **Formulario HTML > JavaScript fetch()**: Para aplicacoes embarcadas, mecanismos basicos da web sao mais confiaveis que APIs modernas.

2. **Nao confie em bibliotecas de terceiros**: O buffer overflow no `SpotifyArduino::requestAccessTokens()` so foi descoberto porque o codigo de autorizacao era maior que 500 bytes. Sempre verifique limites de buffer.

3. **SSL em embarcados e complexo**: Diferentes versoes de mbedtls suportam diferentes algoritmos. O que funciona no PC pode nao funcionar no ESP32.

4. **Google Apps Script e uma excelente ponte**: Permite contornar limitacoes de HTTPS em dispositivos embarcados usando o navegador do usuario como intermediario.

5. **Sempre adicione logs detalhados**: Os logs seriais foram essenciais para diagnosticar o buffer overflow (stack smashing) e o erro de refresh_token vazio.

6. **Teste com dados reais**: O codigo de autorizacao do Spotify e maior do que se imagina (~571 caracteres). Testes com valores curtos nao teriam revelado o bug.
