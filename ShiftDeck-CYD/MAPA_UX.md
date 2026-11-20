# ShiftDeck — Mapa de UX (mockups `IMGS/` × firmware)

Revisão de 2026-11-20. Referência visual: `../IMGS/*.png`. Alvo: ESP32-2432S028 (320×240).

## Ver as telas sem gravar a placa

```bash
sh sim/run.sh
```

Compila o TFT_eSPI **real** + `DisplayUI` + as telas no Mac, com um emulador do ILI9341 no lugar do SPI.
Saída em `sim/out/`: uma PNG por estado e `compare_*.png` (mockup à esquerda, firmware à direita).
Precisa de `pio run` uma vez (para baixar o TFT_eSPI) e do Pillow.

## 1. O que estava errado e o que mudou

### Base visual (todas as telas)

| Antes | Agora |
|---|---|
| Títulos em TomThumb (fonte 3×5 px, ilegível) | Inter suavizada (anti-aliasing), 8 estilos, 12–28 px |
| FreeSans 7-bit **sem acentos**: "Configurações" virava "Configuraes" | Latin-1 completo (ç ã é ó ú…); aspas curvas, travessões e "•" do Spotify convertidos; emoji descartado |
| Formas e ícones serrilhados, ícones primitivos (Wi-Fi em barras, Spotify = círculo) | Primitivas suavizadas do TFT_eSPI; 33 ícones redesenhados no estilo dos mockups |
| Cores aproximadas (card ≈ fundo) | Paleta amostrada dos mockups (`config.h`) |
| Corte de texto por nº de caracteres (`> 20 → "..."`) | Corte por largura real em pixels, seguro para UTF-8 |
| Texto que encostava na borda continuava em x=0 (bug do TFT_eSPI com quebra de linha) | Quebra de linha desligada |

Fontes geradas por `tools/make_fonts.py` a partir de `tools/fonts/Inter-Variable.woff2` (licença SIL OFL).

### Navegação

| Antes | Agora |
|---|---|
| Busca **inalcançável** (nenhuma tela levava a ela) | Home: tocar no título/artista abre a Busca |
| "Playlists" mostrava **Recentes** (últimas faixas) | Playlists de verdade: "Curtidas" + `GET /me/playlists` |
| Configurações via texto "Configuração" na barra | Home: segmento Wi-Fi → Configurações; Spotify ou capa → Playlists; Dispositivo → Configurações › Dispositivo |
| Toque disparava no "encostar" (causava toques acidentais) | Toque dispara ao soltar; arrastar rola listas e cancela o toque |
| Sem retorno de erro | Toast na base da tela ("Nenhum dispositivo ativo", "Não foi possível tocar"…) |

### Por tela

**Inicialização** (`inicializacao.png`)
- Checklist era encenado (marcava tudo "Conectado" com `delay`). Agora cada linha é um teste real: Tela, Touch (calibrado), Wi-Fi (conecta com timeout de 12 s), Backend (renova o token), Spotify (primeira leitura do player).
- Estados: Aguardando, Conectando/Carregando (spinner), Conectado, Erro, Timeout. Falha de Wi-Fi/credenciais leva ao portal de configuração.
- Logo da marca, barra de progresso com marcador, painel com divisórias, rodapé `v1.2` / `ESP32-2432S028`.
- A Home já abre com a música atual (antes abria vazia por ~3 s).

**Home / player** (`principal.png`)
- Barra de status em 3 segmentos (Wi-Fi, Spotify, dispositivo ativo do Spotify Connect: "Celular", "Computador"…).
- Card com capa arredondada, título, artista, álbum, **coração (curtir) de volta**, progresso com marcador, tempos, anterior / play-pause circular / próxima.
- Capa da faixa anterior não aparece mais na faixa nova (placeholder até a capa certa chegar).
- Só a troca de faixa redesenha a tela inteira; progresso, play/pause, coração, capa e status redesenham só a própria área (sem piscar).
- Estados vazios: "Nada tocando", Wi-Fi "Sem rede", Spotify "Offline".

**Playlists** (`Playlists.png`)
- "Curtidas" (bloco roxo com coração) + playlists com miniatura, nome, "N músicas".
- Rolagem por arraste, barra de rolagem, linha selecionada em verde, "Tocar" (ou tocar de novo na linha selecionada).
- Esqueleto enquanto carrega, mensagem de erro com "tocar para tentar de novo", lista vazia.
- Miniaturas baixadas em segundo plano e cacheadas no LittleFS; aparecem uma a uma.

**Busca** (`busca.png`)
- Resultados **nunca eram desenhados** (só "Buscando..."). Agora: lista mista Música / Artista / Álbum / Playlist com miniatura (artista redonda), ▶ ou ›.
- Tecla **X apagava o texto** (o "X" era usado como backspace) e "_" / "<" digitavam lixo. Teclado QWERTY novo: letras, 123, espaço, apagar, Buscar, com realce ao tocar.
- Campo com borda verde, placeholder, cursor, botão limpar; rodapé Voltar / teclado / Buscar.
- Tocar um resultado: música toca dentro do álbum (continua na próxima faixa); artista/álbum/playlist tocam o contexto.

**Configurações** (`config.png`)
- Cabeçalho, 5 linhas com ícone/valor/seta, Voltar / Salvar — como o mockup.
- Cada linha abre um painel real:
  - **Wi-Fi**: rede, sinal (dBm + qualidade), IP; Reconectar; Trocar rede (pede confirmação, reinicia no portal).
  - **Brilho**: slider com prévia ao vivo; só grava em Salvar; Voltar desfaz. (Antes o brilho salvo **nunca era aplicado no boot**.)
  - **Dispositivo**: dispositivos Spotify Connect, ativo marcado, tocar para transferir a reprodução.
  - **Atualização**: versão, data de compilação, placa.
  - **Diagnóstico**: Wi-Fi, API do Spotify, memória livre/mínima, tempo ligado, Calibrar toque.

### Robustez (bugs que não aparecem no visual)

| Problema | Correção |
|---|---|
| `SCUIMessage` levava `String` numa fila FreeRTOS (cópia por `memcpy` → double free latente; só não quebrava porque as strings eram curtas) | `char str[48]` |
| Busca, play de playlist e "Recentes" faziam HTTPS na thread da UI — o mesmo padrão que o código diz causar o crash de `pbuf_free` | Fila de comandos executada só no Core 1; a UI nunca bloqueia |
| Curtir/checar curtida usavam endpoints removidos pelo Spotify em fev/2026 | `PUT/DELETE /me/library`, `GET /me/library/contains`; playlists leem `items.total` (com fallback `tracks.total`); busca respeita o limite novo (≤ 10 por tipo) |
| `playAdvanced` da biblioteca (caminho de escrita que falhava) | Um único helper `api()` com `HTTPClient` + HTTP/1.0 + filtro JSON |
| Login renovava o token duas vezes | Uma vez (boot mais rápido) |
| Play sem dispositivo ativo falhava em silêncio | Acorda o primeiro dispositivo (celular preferido) e avisa se não houver |
| Fila cheia deixava busca/playlists em "carregando" para sempre | Fecha o ciclo com erro |

## 2. Decisões que tomei — revise

1. **Navegação da Home** usa os próprios elementos do mockup (barra de status, capa, título), porque o mockup não mostra botões de navegação. Se preferir botões visíveis ou gestos, é uma troca localizada em `HomeView::onTap`.
2. **Busca**: o cabeçalho tem seta de voltar em vez da lupa — com o teclado aberto o rodapé "Voltar" fica escondido.
3. **Resultados com ›** (artista/álbum/playlist) tocam direto; telas de detalhe ficam para depois.
4. **Fontes um pouco maiores que nos mockups**: os mockups foram desenhados para 480×320 (spec), o CYD tem 320×240; reduzir mais compromete a leitura no carro.
5. **Verde** `#1ED760` da especificação (os mockups usam um verde mais suave). Botão Salvar em verde escuro com texto branco, como no mockup.
6. Versão exibida passou para **v1.2** (`VERSION` em `config.h`), como nos mockups.

## 3. Não verificado

- **Nada rodou na placa ainda.** Compila (RAM 17,4 %, flash 47,5 %) e as telas foram conferidas no simulador, que usa o TFT_eSPI real; toque, rede, decodificação de JPEG e desempenho reais precisam de teste no hardware.
- Endpoints novos seguem a documentação atual do Spotify, não foram chamados com a sua conta.
- "Curtidas" toca o contexto `spotify:user:<id>:collection`; se a conta recusar, cai para as 50 curtidas mais recentes.
- Capas de playlist enviadas pelo usuário só existem em 640 px; o cache para de baixar acima de 80 % do LittleFS (essas ficam com o ícone padrão) e é limpo no boot acima de 60 %.

### Roteiro de teste na placa

1. Boot sem Wi-Fi salvo → portal; com Wi-Fi errado → "Timeout" e portal.
2. Boot normal: checklist todo verde em < 5 s e Home já com a música.
3. Home: play/pause, próxima, anterior (spinner some), coração liga/desliga e aparece no app.
4. Barra de status: Wi-Fi → Configurações; Spotify → Playlists; Dispositivo → lista de dispositivos.
5. Playlists: rolar arrastando, selecionar, Tocar, "Curtidas".
6. Busca: digitar, Buscar, rolar, tocar música / artista / álbum / playlist.
7. Brilho: arrastar (muda ao vivo), Voltar desfaz, Salvar persiste após reiniciar.
8. Dispositivo: transferir para o PC e voltar para o celular.
9. Desligar o roteador: status "Sem rede"/"Offline", nada trava; religar: volta sozinho.

## 4. Próximos passos para nível profissional

**P1 — antes de entregar**
- Rodar o roteiro acima na placa e ajustar toque/tempos.
- Telas de detalhe (álbum, artista, playlist) a partir da Busca e das Playlists.
- Aviso persistente na Home quando não há dispositivo ativo, com atalho para escolher um.

**P2 — produto**
- OTA: a tabela de partições atual (3 MB app + 1 MB FS) não tem slot OTA; "Atualização" hoje só informa a versão.
- Trocar de Wi-Fi pela própria tela (scan + teclado) em vez do portal.
- Volume, shuffle, repeat, fila (recursos futuros da spec).
- Unificar a marca nas telas de configuração inicial (ainda usam o ícone antigo).

**P3 — engenharia**
- Remover código morto: `SpotifyArtMgr`, monitor de GPIO de depuração no `setup()`.
- Atualizar `PROJECT_ARCHITECTURE.md` (tema, fontes, fila de comandos).
- Usar o simulador como teste de regressão visual (comparar PNGs entre versões).
- Incluir o texto da licença OFL do Inter junto de `tools/fonts/`.

## Arquivos

| Área | Arquivos |
|---|---|
| Tema, fontes | `src/config.h`, `src/fonts/InterFonts.h` (gerado), `tools/make_fonts.py` |
| Desenho | `src/DisplayUI.*` |
| Navegação/toque | `src/UIView.*`, `src/UIViewManager.*`, `src/main.cpp` |
| Telas | `src/Views/{Boot,Home,Playlist,Search,Settings}View.*` |
| Spotify | `src/SpotifyPlayer.*`, `src/scui.h` |
| Simulador | `sim/` |
