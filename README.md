# ShiftDeck

Controle do Spotify com tela touch, no estilo Car Thing, rodando numa placa barata: a **ESP32-2432S028** ("Cheap Yellow Display", tela de 2,8" 320×240 com touch resistivo).

Mostra a música que está tocando com capa e progresso, controla play/pause/próxima/anterior, curte faixas, abre suas playlists, faz busca com teclado na tela e troca o dispositivo onde o Spotify toca. Fica sozinha na mesa ou no carro, ligada só na USB.

<p align="center"><img src="docs/screens/home.png" width="480" alt="Tela principal: capa, música, artista, progresso e controles"></p>

<p align="center"><a href="https://www.paypal.com/donate/?hosted_button_id=Q3SECNZ9V6744"><img src="https://img.shields.io/badge/Apoie_o_projeto-PayPal-0070ba?logo=paypal&logoColor=white" alt="Doar com PayPal"></a></p>

## Telas

<table>
<tr>
<td align="center"><img src="docs/screens/boot.png" width="320"><br><sub>Inicialização: checklist de tela, touch, Wi-Fi e Spotify</sub></td>
<td align="center"><img src="docs/screens/home.png" width="320"><br><sub>Tocando agora: toque na capa, no coração ou nos controles</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/playlists.png" width="320"><br><sub>Playlists, com "Curtidas" no topo</sub></td>
<td align="center"><img src="docs/screens/busca-resultados.png" width="320"><br><sub>Busca de músicas, artistas, álbuns e playlists</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/busca-teclado.png" width="320"><br><sub>Teclado na tela</sub></td>
<td align="center"><img src="docs/screens/configuracoes.png" width="320"><br><sub>Configurações: Wi-Fi, brilho, dispositivo, versão, diagnóstico</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/dispositivos.png" width="320"><br><sub>Escolher onde o Spotify toca</sub></td>
<td align="center"><img src="docs/screens/home-offline.png" width="320"><br><sub>Sem conexão: a tela avisa e continua tentando</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/boas-vindas.png" width="320"><br><sub>Primeira vez: o que é a ShiftDeck</sub></td>
<td align="center"><img src="docs/screens/requisitos.png" width="320"><br><sub>O que você vai precisar, com QR para este guia</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/atualizacao.png" width="320"><br><sub>Versão nova encontrada: atualizar agora ou depois</sub></td>
<td align="center"><img src="docs/screens/atualizando.png" width="320"><br><sub>Atualização pela internet, sem cabo</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screens/config-atualizacao.png" width="320"><br><sub>Configurações › Atualização: versão instalada e a mais recente</sub></td>
<td align="center"><img src="docs/screens/conta-spotify.png" width="320"><br><sub>Configurações › Conta Spotify: sair e entrar com outra conta</sub></td>
</tr>
</table>

As imagens saem do simulador do projeto (`ShiftDeck-CYD/sim`), que roda o mesmo código de desenho do firmware.

## Do que você precisa

- Uma placa **ESP32-2432S028** (CYD). Existem versões com micro-USB, USB-C ou as duas; todas servem.
- Um **cabo USB de dados**. Muitos cabos de carregador só carregam: a placa acende, mas o computador não a enxerga.
- Uma conta **Spotify Premium**. A API do Spotify só deixa controlar a reprodução em contas Premium.
- **Chrome** ou **Edge** no computador, para o instalador web.

## Como funciona

```
                      Wi-Fi da sua casa
 ┌───────────┐  HTTPS (certificado verificado)  ┌──────────────────────┐
 │ ShiftDeck │ ───────────────────────────────▶ │ api.spotify.com      │
 │  (ESP32)  │ ◀─────────────────────────────── │ accounts.spotify.com │
 └─────▲─────┘        música, capa, comandos    └──────────────────────┘
       │ http://IP-da-placa/code (só no primeiro login)
 ┌─────┴─────┐   login no Spotify   ┌────────────────────────────────┐
 │  celular  │ ───────────────────▶ │ página de retorno (GitHub Pages)│
 └───────────┘ ◀─ volta p/ a placa ─└────────────────────────────────┘
```

- A placa fala direto com a API do Spotify. Não existe servidor do projeto no meio.
- **Página de retorno.** O Spotify só devolve o login para endereços HTTPS públicos, e a placa só serve HTTP dentro da sua rede. Por isso o Spotify devolve o celular para [`callback.html`](ShiftDeck-CYD/flasher/callback.html), uma página estática no GitHub Pages que só repassa o código para `http://IP-da-placa/code`. Ela não guarda nem envia nada para outro lugar, e só aceita endereços de rede local. É usada uma vez, no primeiro login.
- Depois do login, a placa guarda um *refresh token* e renova o acesso sozinha. Wi-Fi, Client ID e o token ficam só na memória da placa (LittleFS), nunca neste repositório.
- **Sem Client Secret.** O login usa PKCE, o método do Spotify para aparelhos: a placa cria um código aleatório a cada login e prova que é ela quem pediu, então só o Client ID é necessário e nenhum segredo fica gravado.
- **Atualizações.** A placa confere o instalador web ao ligar e a cada 6 horas. Quando sai versão nova, pergunta se quer atualizar; a versão nova é baixada por HTTPS para um segundo espaço da memória e só passa a valer depois de chegar inteira. Wi-Fi e conta continuam salvos.

## Instalação

### 1. Gravar o firmware

1. Abra **https://brachynuspro.github.io/shiftdeck-flasher/** no Chrome ou no Edge.
2. Ligue a placa no computador com o cabo de dados.
3. Clique em **Instalar do zero** (placa nova, ou vindo de uma versão anterior à 1.3.0) ou **Atualizar firmware** (mantém Wi-Fi e conta).
4. Escolha a porta (`USB Serial`, `CH340`, `usbserial`…) e clique em **Install**. Se não conectar, segure **BOOT** enquanto clica.
5. Quando terminar, aperte **RST**.

Drivers: no **macOS** e no **Linux** não precisa instalar nada (o macOS já traz os drivers CH340/CP210x). No **Windows**, se a porta não aparecer, instale o [driver CH340](https://www.wch-ic.com/downloads/CH341SER_EXE.html).

A partir da 1.3.0 a memória da placa tem dois espaços de programa (para as atualizações automáticas). Quem vem de uma versão anterior precisa usar **Instalar do zero** uma vez e configurar de novo. Depois disso, as atualizações chegam sozinhas.

### 2. Criar o app no Spotify

Tem um **[guia passo a passo para celular](https://brachynuspro.github.io/shiftdeck-flasher/guia)**, que também abre pelo QR da tela de boas-vindas. Resumo:

1. Entre em [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard) e clique em **Create app**.
2. Em **Redirect URIs**, adicione exatamente:
   ```
   https://brachynuspro.github.io/shiftdeck-flasher/callback.html
   ```
3. Marque **Web API** e salve.
4. Em **Settings**, copie o **Client ID**. O Client Secret não é necessário.
5. Enquanto o app estiver em modo de desenvolvimento, adicione o e-mail da sua conta Spotify em **User Management**.

### 3. Configurar a placa

Na primeira vez, a ShiftDeck mostra o que ela é e o que você vai precisar, e depois guia você por três telas. Em todas é só apontar a câmera do celular para o QR.

<table>
<tr>
<td align="center"><img src="docs/screens/setup-1-wifi.png" width="300"></td>
<td align="center"><img src="docs/screens/setup-2-spotify.png" width="300"></td>
<td align="center"><img src="docs/screens/setup-3-autorizar.png" width="300"></td>
</tr>
<tr>
<td><b>Passo 1: Wi-Fi.</b> O QR conecta o celular à rede <code>Shiftdeck-Spotify</code> e a página de configuração abre sozinha. Escolha a sua rede, digite a senha e salve. A placa reinicia.</td>
<td><b>Passo 2: Spotify.</b> Com o celular na mesma rede, o QR abre a página da placa. A página tem um botão para abrir o Spotify Dashboard e outro para copiar a Redirect URI. Cole o Client ID e salve. A placa reinicia.</td>
<td><b>Passo 3: Autorizar.</b> O QR abre a página de login. Toque em <b>Autorizar Spotify</b>, entre na sua conta e aceite. O celular volta sozinho para a placa, a tela mostra <b>Conectado!</b> e abre a música.</td>
</tr>
</table>

## Usando

| Onde tocar | O que acontece |
|---|---|
| Play/pause, ◀◀, ▶▶ | Controla a música no aparelho onde o Spotify está tocando |
| Coração | Curte ou descurte a faixa |
| Capa | Abre as playlists |
| Nome da música | Abre a busca |
| Barra de cima: Wi-Fi / Spotify / Dispositivo | Configurações / Playlists / Escolher dispositivo |

Se aparecer **"Nenhum dispositivo ativo"**, abra o Spotify em algum aparelho (celular, PC, caixa de som) uma vez; depois a ShiftDeck consegue tocar e trocar de aparelho sozinha.

## Problemas comuns

| Sintoma | Causa e solução |
|---|---|
| A placa acende, mas a porta não aparece no instalador | Cabo só de carga. Troque por um cabo de dados (teste: com ele o celular aparece no computador). |
| A porta some quando a tela acende | Falta energia: hub sem fonte ou conector frouxo. Ligue direto no computador ou num hub com fonte. |
| "Spotify Offline" na tela | Confira a internet do Wi-Fi. Se continuar, refaça o Passo 3 (a autorização pode ter sido revogada na conta Spotify). |
| A placa reinicia ao tocar em **Começar** ou quando a tela acende | Falta energia: o Wi-Fi e o brilho da tela puxam picos de corrente. Use um carregador de tomada (5 V, 1 A) ou um hub com fonte. |
| O Spotify mostra "INVALID_CLIENT: Invalid redirect URI" | A Redirect URI do seu app no Spotify não está igual à do Passo 2. Copie de novo, sem espaço nem barra no final. |
| Quero apagar tudo e configurar de novo | No instalador, use **Instalar do zero**: apaga Wi-Fi, conta e calibração. |

## Segurança e privacidade

- Nenhuma senha, token ou ID pessoal fica no código. Cada pessoa cria o próprio app no Spotify.
- As conexões HTTPS da placa verificam o certificado do servidor com uma lista de autoridades raiz embutida no firmware (`ShiftDeck-CYD/certs/`, gerada por `tools/gen_crt_bundle.py`).
- A página de retorno roda só no navegador: lê o código da URL e manda o celular para a placa. Esse código sozinho não serve para nada: trocá-lo por um token exige o código aleatório (PKCE) que só a placa tem.
- As atualizações vêm só do instalador oficial, por HTTPS verificado. Se o download falhar, a versão atual continua instalada.

## Desenvolver

Requisitos: [PlatformIO](https://platformio.org/) (`pip install platformio`) e Python 3 com Pillow e `cryptography` para as ferramentas.

```bash
cd ShiftDeck-CYD
pio run -e cyd                 # compila
pio run -e cyd -t upload       # grava via USB (só o app; mantém as configurações)
sh sim/run.sh                  # renderiza todas as telas em sim/out/*.png, sem placa
sh tools/release.sh --publish  # gera o firmware do instalador e publica a página
```

**Não** rode `pio run -t uploadfs` numa placa já configurada: ele sobrescreve a partição LittleFS onde ficam Wi-Fi, conta do Spotify e calibração.

Para atualizar a lista de certificados raiz (no macOS):

```bash
security find-certificate -a -p /System/Library/Keychains/SystemRootCertificates.keychain > roots.pem
python3 tools/gen_crt_bundle.py roots.pem certs/x509_crt_bundle.bin
```

### Publicar uma versão nova

1. Suba `VERSION` em `ShiftDeck-CYD/src/config.h`.
2. `sh tools/release.sh --publish` (dentro de `ShiftDeck-CYD/`, com o `gh` logado). A página do instalador atualiza em ~1 minuto, e as placas ligadas oferecem a versão nova em até 6 horas (ou na hora, em Configurações › Atualização › Verificar).

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `ShiftDeck-CYD/src` | Firmware (PlatformIO/Arduino): telas em `Views/`, desenho em `DisplayUI`, Spotify em `SpotifyPlayer`, configurações em `Vault` |
| `ShiftDeck-CYD/sim` | Simulador das telas no computador (TFT_eSPI real + emulador do ILI9341) |
| `ShiftDeck-CYD/flasher` | Instalador web, página de retorno do login (`callback.html`) e imagens das atualizações, publicados no repositório `shiftdeck-flasher` |
| `ShiftDeck-CYD/tools` | Fontes (`make_fonts.py`), certificados (`gen_crt_bundle.py`) e release (`release.sh`) |
| `ShiftDeck-CYD/certs` | Lista de autoridades raiz usada no HTTPS |
| `docs/screens` | Imagens das telas usadas neste README |
| `IMGS` | Mockups de referência da interface |

## Documentação

- [Mapa de UX](ShiftDeck-CYD/MAPA_UX.md): mockups × firmware, decisões e próximos passos
- [Especificação da interface](PROJETO_SPOTIFY_CAR_ESP32_v1.2.md)
- [Arquitetura](PROJECT_ARCHITECTURE.md) · [Ferramentas e fluxo](TOOLS_AND_WORKFLOW.md) · [Histórico](DEVELOPMENT_JOURNEY.md)

## Apoie o projeto

A ShiftDeck é gratuita e de código aberto. Se ela te ajudou e você quiser apoiar o desenvolvimento (placas para teste, novas funções, tempo), qualquer valor ajuda.

<p align="center">
  <a href="https://www.paypal.com/donate/?hosted_button_id=Q3SECNZ9V6744"><img src="https://img.shields.io/badge/Doar_com-PayPal-0070ba?logo=paypal&logoColor=white&style=for-the-badge" alt="Doar com PayPal"></a><br><br>
  <img src="docs/doar-paypal-qr.png" width="180" alt="QR code para doar com PayPal"><br>
  <sub>Ou aponte a câmera do celular para o QR</sub>
</p>

## Licenças de terceiros

- Fonte Inter (`ShiftDeck-CYD/tools/fonts`): SIL Open Font License 1.1, https://github.com/rsms/inter
- SpotifyArduino (`ShiftDeck-CYD/lib/SpotifyArduino`): veja a licença na própria pasta.
- Spotify é marca da Spotify AB. Este projeto não é afiliado ao Spotify.
