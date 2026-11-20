# ShiftDeck
## Especificação da Interface
Versão 1.2

---

# Objetivo

Criar um controlador inteligente para Spotify utilizando ESP32 + Display Touch.

O dispositivo funciona como uma extensão do celular, semelhante ao Spotify Car Thing.

Características principais:

- Interface extremamente rápida
- Grandes áreas de toque
- Poucos cliques
- Foco em reprodução de música
- Navegação simples
- Inicialização inferior a 5 segundos
- Tema Dark
- Destaques em verde Spotify (#1ED760)

---

# Fluxo Principal

Inicialização

↓

Home (Player)

↓

Playlist
ou

Busca
ou

Configurações

---

# Resolução

Projeto pensado para display:

480x320 Landscape

Toda interface deve ser baseada em Grid de 8px.

Margens:

16px externas

Espaçamento:

8
16
24

---

# Tema

## Background

#121212

## Cartões

#1B1B1B

## Botões

#222222

## Selecionado

Spotify Green

#1ED760

## Texto Primário

Branco

#FFFFFF

## Texto Secundário

#B3B3B3

---

# Fonte

Sugestão

SF Pro Display

ou

Inter

Pesos

Regular

Medium

Bold

---

# Tela 1
Inicialização

Objetivo

Mostrar status do hardware.

Componentes

Logo ShiftDeck

Texto

Inicializando

Progress Bar

Checklist:

Tela

Touch

WiFi

Backend

Spotify

Rodapé:

Versão Firmware

Modelo ESP

Estados possíveis

Conectando

Conectado

Erro

Carregando

Timeout

Após todos OK

Abrir Home automaticamente.

---

# Tela 2
Home (Player)

Objetivo

Controlar música atual.

Estrutura

Topo

WiFi

Spotify

Dispositivo conectado

Centro

Capa do álbum

Nome da música

Nome do artista

Álbum

Curtir

Progresso

Barra da música

Tempo Atual

Tempo Total

Rodapé

Anterior

Play/Pause

Próxima

Funções

Play

Pause

Next

Previous

Like

Atualização automática da barra

Atualização automática dos textos

Troca da capa

Troca do artista

Troca do álbum

Indicadores

WiFi conectado

Spotify conectado

Dispositivo ativo

---

# Tela 3
Busca

Objetivo

Pesquisar músicas rapidamente.

Componentes

Título

Buscar

Campo de pesquisa

Lista de resultados

Resultado

Imagem

Nome

Tipo

Botão ação

Tipos

Música

Artista

Álbum

Playlist

Rodapé

Voltar

Teclado

Buscar

Fluxo

Usuário digita

↓

Backend consulta Spotify

↓

Lista atualizada

↓

Selecionar item

↓

Executar

---

# Tela 4
Playlists

Objetivo

Selecionar playlists favoritas.

Componentes

Topo

Voltar

WiFi

Spotify

Lista

Imagem

Nome

Quantidade músicas

Scroll

Item selecionado

Verde Spotify

Rodapé

Botão

Tocar

Ações

Abrir playlist

Executar playlist

Scroll

Atualizar lista

---

# Tela 5
Configurações

Objetivo

Gerenciar dispositivo.

Itens

WiFi

Selecionar rede

Senha

Reconectar

Brilho

Slider

0-100%

Dispositivo

Selecionar celular

Atualização

Versão

Buscar firmware

Diagnóstico

Teste Touch

Teste Display

Teste WiFi

Rodapé

Voltar

Salvar

---

# Componentes

## Status Card

Ícone

Título

Valor

Seta

---

## Botão Primário

Cor:

Spotify Green

Texto Branco

Raio:

16px

---

## Botão Secundário

Cinza

Texto Branco

---

## Progress Bar

Altura

8px

Cantos arredondados

Animação suave

---

## Lista

Imagem

Título

Descrição

Indicador

Seta

ou

Play

---

# Estados

Loading

Skeleton

Erro

Offline

Sem internet

Sem Spotify

Reconectando

---

# Navegação

Inicialização

↓

Home

↓

Playlist

↓

Home

↓

Busca

↓

Resultado

↓

Home

↓

Configurações

↓

Home

---

# Backend ESP32

Módulos

Display Manager

Touch Manager

Spotify Client

WiFi Manager

Settings Manager

Cache Manager

API Client

Event Manager

---

# Atualização da UI

Eventos

MusicChanged

PlaybackChanged

DeviceChanged

NetworkChanged

PlaylistLoaded

SearchLoaded

BrightnessChanged

---

# Cache

Salvar localmente

Última música

Última playlist

Brilho

WiFi

Token

Dispositivo

---

# Integração Spotify

Necessário

OAuth

Refresh Token

Player API

Search API

Playlist API

Devices API

Album API

Current Playback

---

# Performance

FPS

30+

Troca de telas

<150ms

Boot

<5s

Busca

<500ms

Troca música

Tempo real

---

# Recursos futuros

Favoritos

Volume

Shuffle

Repeat

Fila de reprodução

Histórico

Comandos por encoder

Tema claro

Widgets

OTA

Bluetooth

Apple Music

YouTube Music

Modo carro

Sleep Mode

Atualização OTA automática