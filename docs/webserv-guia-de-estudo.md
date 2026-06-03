# Webserv — Guia de Conceitos para Estudo

*Documento de estudo coletivo para o projeto Webserv (grupo de 3)*

> Este guia mapeia **apenas os conceitos específicos** que o projeto exige. Pré-requisitos de C++ (classes, templates, STL, herança, etc.) ficam de fora — assumimos domínio prévio. O objetivo é que os 3 cheguem ao mesmo nível antes de começar a dividir tarefas, e que tenhamos uma referência comum durante a implementação.

---

## Como usar este documento

1. Leiam todos os 4 níveis antes de discutir a divisão de tarefas. Vocês precisam do mesmo modelo mental.
2. Cada nível é um pré-requisito do seguinte. Não pulem.
3. As seções **"Por quê isso importa no Webserv"** existem para conectar teoria → exigência do subject. Não ignorem.
4. No fim tem um roteiro semanal sugerido e a lista de recursos.

---

## Visão geral — como tudo se conecta

Um servidor HTTP é a junção de três domínios técnicos que precisam funcionar em harmonia:

1. **Rede (sockets TCP)** — como o servidor escuta uma porta e aceita conexões.
2. **Protocolo HTTP** — como interpretar bytes recebidos como requisição e produzir bytes válidos como resposta.
3. **Arquitetura não-bloqueante (I/O multiplexing)** — como atender múltiplos clientes em um único processo, sem threads, sem bloquear.

Sobre esses três pilares, o subject empilha:

- **CGI** — execução de scripts externos (PHP, Python) para conteúdo dinâmico, integrado ao event loop.
- **Parser de configuração** — leitura de um arquivo estilo NGINX que define os servers, ports, locations, regras.
- **Manipulação de arquivos** — servir conteúdo estático, listar diretórios, receber uploads.

Visualmente, em runtime:

```
        config file (parseado uma vez no startup)
                       │
                       ▼
       ┌───────────────────────────────────┐
       │  Lista de "servers" (host:port)   │
       │  com suas locations e regras      │
       └───────────────┬───────────────────┘
                       │
                       ▼
   socket() → bind() → listen()  para cada host:port
                       │
                       ▼
           ┌───────────────────────┐
           │     EVENT LOOP        │
           │   while(running) {    │
           │     poll(...)         │
           │     for cada fd      │
           │       pronto: ...     │
           │   }                   │
           └─────┬──────┬──────┬───┘
                 │      │      │
                 ▼      ▼      ▼
         listen fd  client fd  CGI pipe fd
         accept()   recv()     read() output
         → novo     → parser   → empacota
           client     HTTP       resposta
                    → router
                    → handler
                       │
                       ▼
                    send() resposta
                    (também passa pelo poll)
```

A regra de ouro do subject: **toda leitura e toda escrita passa por `poll()` (ou equivalente)**. Quem violar isso tira zero. Internalizem isso desde já porque vai moldar TODA a arquitetura.

---

## Nível 1 — Fundamentos

Pré-requisitos teóricos. Sem isso, nada do resto faz sentido.

### 1.1 Modelo TCP/IP e o conceito de socket

- Pilha TCP/IP: aplicação (HTTP) sobre transporte (TCP) sobre rede (IP).
- O que é uma conexão TCP: handshake de 3 vias, garantia de entrega ordenada, fluxo de bytes (não preserva "mensagens" — isso é problema da camada de aplicação resolver).
- IP + porta = endpoint. Um servidor escuta em `(interface:porta)`.
- Socket = abstração do SO para um endpoint de comunicação. Do ponto de vista do programa é um file descriptor.
- Socket de servidor (passivo, faz `listen()`) vs socket de cliente conectado (ativo, retornado por `accept()`).
- `struct sockaddr_in` para IPv4: family, port, address.
- Byte order: rede usa big-endian, host pode ser little-endian. Daí `htons`/`htonl`/`ntohs`/`ntohl`.

### 1.2 Protocolo HTTP — estrutura básica

- HTTP é texto (ASCII) sobre TCP. Separador universal: `\r\n` (CRLF).
- HTTP/1.0 vs HTTP/1.1 — recomendado mirar 1.1, mas saber que 1.0 é a base mental mais simples.
- **Estrutura de uma requisição:**
  ```
  <MÉTODO> <URI> <VERSÃO>\r\n
  Header1: valor1\r\n
  Header2: valor2\r\n
  \r\n
  <body opcional>
  ```
- **Estrutura de uma resposta:**
  ```
  <VERSÃO> <CÓDIGO> <FRASE>\r\n
  Header1: valor1\r\n
  ...
  \r\n
  <body opcional>
  ```
- Métodos exigidos: `GET`, `POST`, `DELETE`. Saber também que existem `PUT`, `HEAD`, `OPTIONS`, `PATCH` (mesmo que não sejam obrigatórios — o servidor precisa responder algo razoável a métodos não suportados, como 405).
- Famílias de status: 1xx informativo, 2xx sucesso, 3xx redirect, 4xx erro do cliente, 5xx erro do servidor. Conhecer pelo menos: 200, 201, 204, 301, 302, 400, 403, 404, 405, 408, 411, 413, 414, 500, 501, 502, 504, 505.
- Headers que VÃO aparecer no projeto: `Host`, `Content-Length`, `Content-Type`, `Connection`, `Transfer-Encoding`, `Location`, `Server`, `Date`, `Content-Disposition` (uploads).
- URL/URI: `scheme://host:port/path?query#fragment`. O servidor recebe na requisição apenas `path?query` (a request-URI).
- Percent-encoding: `%20` = espaço, `%2F` = `/`, etc. Precisa decodificar.

### 1.3 File descriptors no Unix

- Tudo é fd: socket, pipe, arquivo regular, stdin/stdout/stderr.
- `read()`, `write()`, `close()` funcionam em qualquer fd.
- Tabela de fds é por processo. `fork()` herda; `execve()` mantém (a menos que `FD_CLOEXEC` esteja setado).
- Limite por processo: `ulimit -n`. Servidor real precisa lidar com isso.

**Por quê isso importa no Webserv:** todo o event loop é uma máquina que rastreia o estado de muitos fds simultâneos — connect listening, conexões de cliente, pipes do CGI, eventualmente fds de arquivo.

---

## Nível 2 — Implementação central

A espinha dorsal do servidor. Sem isso o projeto não roda.

### 2.1 API de sockets (socket layer)

Funções obrigatórias do subject que você vai usar aqui:

- `socket()` — cria fd de socket. Família (`AF_INET`), tipo (`SOCK_STREAM` para TCP).
- `setsockopt()` — configurar opções. **Crucial:** `SO_REUSEADDR` antes do `bind()` para não ter que esperar o `TIME_WAIT` ao reiniciar o servidor.
- `bind()` — associa o socket a `(interface:porta)`.
- `listen()` — coloca em modo passivo. O `backlog` (segundo parâmetro) é o tamanho da fila de conexões pendentes.
- `accept()` — aceita uma conexão pendente. **Retorna um NOVO fd** (não substitui o fd que está em listen). Esse novo fd representa o cliente conectado.
- `recv()` / `send()` — receber/enviar dados. Em sockets, prefira sobre `read`/`write` porque aceitam flags úteis (ex.: `MSG_NOSIGNAL`).
- `close()` — encerra o fd. Em socket, dispara o handshake de fechamento.
- `getaddrinfo()` / `freeaddrinfo()` — resolver endereços de forma portável.
- `getsockname()` — descobrir a quem o socket está realmente bound (útil quando bindou em porta 0).

Ciclo de vida do servidor: `socket()` → `setsockopt()` → `bind()` → `listen()` → loop de `accept()`.

### 2.2 I/O não-bloqueante e multiplexação

**Esta é a seção mais importante do nível 2.** A regra "uma única chamada de poll para tudo" molda toda a arquitetura.

- **Bloqueante vs não-bloqueante:** num fd bloqueante, `read()` espera até ter dados. Num não-bloqueante, retorna imediatamente. Se não há dados, retorna `-1` com `EAGAIN`/`EWOULDBLOCK` (no nosso caso é proibido inspecionar errno depois de read/write — então só faça operações quando o `poll` disse que dá).
- `fcntl(fd, F_SETFL, O_NONBLOCK)` — coloca o fd em modo não-bloqueante. No macOS, lembrem da restrição: só podem usar as flags `F_SETFL`, `O_NONBLOCK` e `FD_CLOEXEC`.
- **Multiplexação** — uma única chamada do SO que diz "destes N fds, esses estão prontos para ler/escrever":
  - `select()` — antigo, portável, mas tem limite (FD_SETSIZE, geralmente 1024) e copia bitsets a cada chamada.
  - `poll()` — mesmo conceito, sem limite de fds, API mais limpa (array de `struct pollfd`).
  - `epoll()` (Linux) — muito mais escalável, mantém estado entre chamadas, suporta edge-triggered.
  - `kqueue()` (BSD/macOS) — equivalente em performance ao epoll.
- **Padrão event loop:**
  ```
  while (running) {
      poll(fds, nfds, timeout);
      for each fd com revents != 0:
          if é listening fd:        accept e adiciona client ao poll
          else if pode ler (POLLIN): recv, parse incremental, talvez gerar resposta
          else if pode escrever (POLLOUT): send do que tiver pendente
          else if erro/HUP:          fechar e remover do poll
  }
  ```
- **Atenção a writes parciais:** `send()` pode escrever menos do que você pediu. Você precisa manter um buffer de saída por conexão e continuar tentando quando o `poll` reportar `POLLOUT` de novo.
- **Atenção a reads fragmentados:** uma única requisição HTTP pode chegar em múltiplos `recv()`s. O parser precisa ser incremental (alimentado em pedaços, mantendo estado).

**Por quê isso importa no Webserv:** o subject diz literalmente que se você fizer read/recv ou write/send sem passar por `poll`, sua nota é 0. Toda decisão de arquitetura depende disso.

### 2.3 Parser HTTP (de requisição)

Construa como uma máquina de estados que aceita bytes e avança por fases:

1. **Request line** — lê até o primeiro `\r\n`. Extrai método, URI, versão.
2. **Headers** — lê linha por linha até encontrar uma linha vazia (`\r\n\r\n`). Cada header é `Nome: valor`.
3. **Body** — só existe se houver `Content-Length` ou `Transfer-Encoding: chunked`. Tamanho conhecido → ler N bytes. Chunked → loop de chunks.

Cuidados:

- O buffer pode chegar fragmentado entre vários `recv()`s. O parser tem que aceitar "alimentação parcial".
- O buffer pode trazer pedaço da requisição seguinte (HTTP/1.1 com keep-alive). Não estraguem o que sobra.
- Limites duros: tamanho máximo de URI, tamanho máximo de header, tamanho máximo de body (este último vem da config: `client_max_body_size`).
- Decodificar percent-encoding na URI antes de resolver caminho.
- Parse e validação separados — primeiro extrair, depois validar (e responder 4xx se inválido).

### 2.4 Builder de resposta HTTP

- Status line correto com a frase padrão (`200 OK`, `404 Not Found`, etc.).
- Headers que vocês quase sempre querem incluir:
  - `Content-Length` (sempre que possível — facilita keep-alive).
  - `Content-Type` baseado em MIME type da extensão do arquivo.
  - `Date` (formato HTTP-date, RFC 7231).
  - `Server` (qualquer string, ex.: `webserv/1.0`).
  - `Connection: close` ou `keep-alive` conforme negociação.
- Body em bytes brutos. Imagens/binários NÃO são texto.
- Cuidado: ou `Content-Length` ou `Transfer-Encoding: chunked`, nunca os dois.

### 2.5 Servir arquivos estáticos

- Resolução de caminho: pegar a URI da requisição, casar com a `location` apropriada, juntar com `root` da location.
- **Path traversal** — bloquear `..` resolvendo o caminho final e verificando que ainda está dentro de `root`. Sem isso, qualquer um lê `/etc/passwd`.
- `stat()` para verificar: existe? é arquivo regular? é diretório? tem permissão?
- Mapeamento extensão → MIME type: `.html`/`.htm` → `text/html`, `.css` → `text/css`, `.js` → `application/javascript`, `.png` → `image/png`, `.jpg` → `image/jpeg`, `.pdf` → `application/pdf`, sem extensão / desconhecido → `application/octet-stream`.
- Páginas de erro padrão (404, 500, etc.) — HTML simples embutido no binário, usado quando a config não definir `error_page`.

### 2.6 Parser do arquivo de configuração

- Decisão de design coletiva: sintaxe própria simples ou inspirada em NGINX. **Recomendo NGINX** porque é o que o subject sugere e porque vocês já vão estar lendo docs do NGINX para outras coisas.
- Estrutura mínima que vocês precisam suportar (exemplo):
  ```nginx
  server {
      listen 8080;
      server_name example.com;
      root /var/www;
      client_max_body_size 1m;
      error_page 404 /404.html;
      
      location / {
          allowed_methods GET POST;
          autoindex off;
          index index.html;
      }
      
      location /uploads {
          allowed_methods POST DELETE;
          upload_store /var/uploads;
      }
      
      location /old {
          return 301 /new;
      }
      
      location ~ \.php$ {
          cgi_pass /usr/bin/php-cgi;
      }
  }
  ```
- Implementação: tokenizer (lexer) → parser que constrói uma árvore/estrutura interna → validação semântica (ports válidas, paths existem se aplicável, métodos reconhecidos, etc.).
- A estrutura interna vai ser consultada por requisição em runtime para escolher o bloco `server`, depois o bloco `location`, depois aplicar regras.

---

## Nível 3 — Recursos avançados

Tudo aqui é exigência do subject. São "avançados" no sentido de virem depois do core funcionar.

### 3.1 Múltiplas portas / múltiplos servers

- Cada bloco `server` da config produz um (ou mais) socket em listen.
- Todos os listening sockets ficam no mesmo `poll()`.
- Quando uma conexão é aceita, ela é "associada" ao server que tem aquele `host:port`.

### 3.2 Match de location

Algoritmo padrão (inspirado em NGINX, simplificado):

1. Pegue a request-URI.
2. Procure a location com o **prefixo mais longo** que casa com o início da URI.
3. Aplique as regras dessa location (allowed_methods, root, autoindex, redirect, cgi, upload, etc.).
4. Override de cima pra baixo: o que estiver na location ganha do que estiver no server.

### 3.3 Listagem de diretório (autoindex)

- `opendir()` → loop de `readdir()` → `closedir()`.
- Para cada entrada, `stat()` para descobrir tamanho e tipo.
- Gerar HTML simples com `<a href>` para cada entrada.
- Só ativa se a config tiver `autoindex on` E o request for um diretório E não houver `index` definido (ou o arquivo de index não existir).

### 3.4 Upload de arquivos (POST com multipart/form-data)

- O `Content-Type` da requisição é `multipart/form-data; boundary=----WebKitFormBoundary...`.
- O body é dividido por `--<boundary>`. A última fronteira é `--<boundary>--`.
- Cada parte tem seus próprios headers (`Content-Disposition: form-data; name="..."; filename="..."` e `Content-Type`), uma linha vazia, e o conteúdo.
- Salvar o conteúdo no diretório configurado (`upload_store` ou similar).
- Cuidado: o body pode ser grande. Não carrega tudo em memória se der pra evitar.

### 3.5 Transfer-Encoding: chunked

Formato de cada chunk no body:

```
<tamanho em hex>\r\n
<dados do chunk>\r\n
```

Chunk de tamanho 0 marca o fim:

```
0\r\n
\r\n
```

- Vocês têm que SABER LER (request body chunked enviado por cliente).
- Podem opcionalmente PRODUZIR (response body chunked) — útil quando não dá pra saber `Content-Length` antecipadamente.
- Importante: ao fazer dispatch para CGI, "des-chunkar" antes (o subject menciona isso explicitamente: o CGI espera EOF como fim do body).

### 3.6 Redirecionamento HTTP

- Status: 301 (permanente), 302 (temporário, padrão), 303, 307, 308. Para o projeto, 301 e 302 já cobrem.
- Header `Location: <nova-URL>` obrigatório.
- Configurável por location (ex.: `return 301 /new-path;`).

### 3.7 CGI (Common Gateway Interface)

**Esta é a parte mais delicada do projeto.** Integrar `fork()` + `pipe()` com o event loop não-bloqueante exige cuidado.

Conceito: o servidor invoca um programa externo para gerar a resposta. O programa lê a request via stdin + variáveis de ambiente, e escreve a resposta no stdout.

Fluxo:

1. Servidor identifica que a URL deve ser tratada por CGI (regra na config baseada em extensão, ex.: `.php`).
2. Cria dois pipes: um para mandar o body do request pro CGI (stdin do filho), outro pra ler a resposta (stdout do filho).
3. `fork()`.
4. **Filho:**
   - `dup2()` para redirecionar stdin/stdout para os pipes.
   - Fechar fds que não vai usar.
   - Popular variáveis de ambiente (vetor `envp`).
   - `chdir()` para o diretório do script (importante para paths relativos no script funcionarem).
   - `execve()` do interpretador (`/usr/bin/php-cgi`, `/usr/bin/python3`, etc.) com o caminho do script como argv[1].
5. **Pai:**
   - Fecha os fds do lado do filho.
   - Adiciona os pipes ao `poll()`.
   - Quando `POLLOUT` no pipe-de-entrada-do-filho: escreve pedaço do body.
   - Quando `POLLIN` no pipe-de-saída-do-filho: lê pedaço da resposta.
   - Quando o pipe-de-saída fecha (EOF): coleta com `waitpid()` (use `WNOHANG` para não bloquear) e finaliza.

Variáveis de ambiente CGI (RFC 3875) que o subject espera:

- `REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_TYPE`, `CONTENT_LENGTH`
- `SCRIPT_NAME`, `SCRIPT_FILENAME`, `PATH_INFO`, `PATH_TRANSLATED`
- `SERVER_NAME`, `SERVER_PORT`, `SERVER_PROTOCOL`, `GATEWAY_INTERFACE` (`CGI/1.1`)
- `REDIRECT_STATUS` (necessário para o php-cgi não recusar)
- `HTTP_*` para cada header HTTP (ex.: `User-Agent` → `HTTP_USER_AGENT`)

Saída do CGI: começa com headers próprios (geralmente `Content-Type` e às vezes `Status: 200 OK`), uma linha vazia (`\r\n\r\n` ou `\n\n`), e depois o body. Vocês têm que parsear esses headers e mesclar com os seus antes de mandar a resposta pro cliente. Se o CGI não mandar `Status`, é 200.

Cuidados extras:

- O subject diz: "se nenhum content_length for retornado do CGI, EOF marca o final dos dados". Significa que se o CGI não mandar `Content-Length` na saída, vocês precisam ler até o pipe fechar.
- Timeout no CGI — se o script trava, você precisa matar (`kill()`) e responder 504.
- Os pipes do CGI também precisam estar em modo não-bloqueante e dentro do event loop. **Não façam `read()` em pipe sem passar por `poll()` — isso quebra a regra do subject e tira a nota.**

### 3.8 Sinais

- `signal(SIGINT, handler)` para encerrar limpo no Ctrl+C (fechar sockets, salvar estado se houver).
- **`SIGPIPE` é crítico:** quando o cliente fecha a conexão e você tenta `send()`, o SO manda SIGPIPE e por padrão isso mata o processo. Soluções: ignorar globalmente (`signal(SIGPIPE, SIG_IGN)`) ou usar `MSG_NOSIGNAL` em todo `send()`. Escolham uma e sejam consistentes.
- `waitpid()` para coletar processos filhos (CGI). Sem isso, viram zombies.

### 3.9 Robustez e timeouts

- Cliente que conecta e não envia nada → timeout de leitura, depois fecha.
- Keep-alive: cliente pode reusar conexão. Você decide o timeout de idle.
- Limites: `client_max_body_size`, tamanho máximo de URI, número máximo de headers, etc.
- **Nenhum erro pode crashar o servidor.** Toda condição de erro vira uma resposta HTTP de erro ou um close limpo da conexão. O subject grifa isso: "A resiliência é fundamental. Seu servidor deve permanecer operacional em todos os momentos."

---

## Nível 4 — Testes e validação

### 4.1 Ferramentas manuais

- `telnet host porta` — manda requisição HTTP "na mão", linha por linha. Excelente pra testar parsing de casos estranhos.
- `curl` com flags úteis: `-v` (verbose), `-X METHOD`, `-H 'Header: val'`, `-d 'body'`, `--data-binary @arquivo`, `-F 'campo=@arquivo'` (multipart upload), `-i` (mostra headers da resposta).
- Navegador real (Firefox, Chrome) — **obrigatório** pelo subject. Especialmente útil pra verificar que páginas com CSS/JS/imagens carregam (testa keep-alive, MIME types, paralelismo).
- NGINX rodando em paralelo — comparar headers (`curl -v`) e comportamento de edge cases.

### 4.2 Testes automatizados

O subject recomenda fortemente. Escrevam uma suite em Python (urllib, requests, ou socket cru pra casos malucos):

- Cada método HTTP em cada cenário (GET de arquivo, GET de diretório, POST com upload, DELETE, método não permitido).
- Status codes corretos em cada caso de erro.
- Headers de resposta consistentes.
- Edge cases: URL muito longa, body sem `Content-Length`, body maior que `client_max_body_size`, header malformado, request line inválida.
- Stress: muitas conexões simultâneas (use `ab`, `wrk`, ou um script Python com asyncio).
- CGI básico (script que ecoa env vars).
- Upload de arquivo com verificação de hash.

---

## Como tudo se conecta — modelo mental

Quando uma requisição entra no seu servidor, esses são os componentes que ela toca, em ordem:

```
[bytes brutos do socket]
        │
        ▼
[Buffer de leitura por-conexão]    ← alimenta incremental, espera mais bytes se incompleto
        │
        ▼
[Parser HTTP]                       ← máquina de estados: request-line → headers → body
        │
        ▼
[Estrutura HttpRequest]             ← método, URI, headers, body
        │
        ▼
[Router]                            ← escolhe server (host:port) → escolhe location (prefixo)
        │
        ▼
[Handler]                           ← decide: arquivo estático? CGI? upload? redirect? autoindex? erro?
        │
        ▼
[Estrutura HttpResponse]            ← status, headers, body
        │
        ▼
[Buffer de escrita por-conexão]    ← serializa para bytes
        │
        ▼
[Event loop manda send() quando POLLOUT]
        │
        ▼
[bytes para o socket]
```

Pra cada conexão, vocês mantêm uma estrutura `Connection` com:

- O fd
- Buffer de leitura (entrada parcial acumulada)
- Estado do parser (em qual fase está)
- Buffer de escrita (resposta parcial pendente)
- Possivelmente: ponteiro pro processo CGI ativo
- Timestamp da última atividade (pra timeout)

E o event loop é uma máquina que itera sobre essas Connections, alimentando bytes quando o poll diz que pode ler, e drenando bytes quando o poll diz que pode escrever.

---

## Convenções de código e fluxo de trabalho

Antes de qualquer um escrever uma linha de código, alinhem tudo desta seção numa reunião. Trabalhar em 3 sem convenções vira merge conflict crônico, código inconsistente e dor na defesa.

### Estrutura de pastas

```
webserv/
├── Makefile
├── README.md
├── .gitignore
├── .clang-format         (opcional, mas recomendado)
├── .editorconfig         (opcional)
│
├── src/                  (.cpp)
│   ├── main.cpp
│   ├── config/           (parser de configuração)
│   │   ├── ConfigParser.cpp
│   │   ├── Lexer.cpp
│   │   └── ServerConfig.cpp
│   ├── http/             (protocolo HTTP)
│   │   ├── HttpRequest.cpp
│   │   ├── HttpResponse.cpp
│   │   ├── RequestParser.cpp
│   │   └── MimeTypes.cpp
│   ├── network/          (camada de socket)
│   │   ├── Socket.cpp
│   │   ├── ListenSocket.cpp
│   │   └── Connection.cpp
│   ├── server/           (event loop, orquestração)
│   │   ├── Server.cpp
│   │   ├── EventLoop.cpp
│   │   └── Router.cpp
│   ├── cgi/              (CGI)
│   │   ├── CgiHandler.cpp
│   │   └── CgiEnv.cpp
│   ├── handlers/         (handlers por tipo de requisição)
│   │   ├── StaticFileHandler.cpp
│   │   ├── UploadHandler.cpp
│   │   ├── DeleteHandler.cpp
│   │   ├── AutoIndexHandler.cpp
│   │   └── RedirectHandler.cpp
│   └── utils/
│       ├── Logger.cpp
│       └── StringUtils.cpp
│
├── include/              (.hpp — espelha src/)
│   ├── config/
│   ├── http/
│   ├── network/
│   ├── server/
│   ├── cgi/
│   ├── handlers/
│   └── utils/
│
├── conf/                 (configs de exemplo para demo/avaliação)
│   ├── default.conf
│   ├── multi-port.conf
│   ├── cgi.conf
│   └── upload.conf
│
├── www/                  (arquivos estáticos servidos em demo)
│   ├── index.html
│   ├── style.css
│   ├── upload-form.html
│   └── errors/
│       ├── 404.html
│       └── 500.html
│
├── cgi-bin/              (scripts CGI de exemplo)
│   ├── hello.py
│   ├── env.php
│   └── form-echo.py
│
├── uploads/              (destino de uploads em runtime — só .gitkeep no git)
│
├── tests/                (suite Python recomendada pelo subject)
│   ├── test_basic.py
│   ├── test_methods.py
│   ├── test_upload.py
│   ├── test_cgi.py
│   ├── test_stress.py
│   └── README.md
│
└── docs/
    └── webserv-guia-de-estudo.md   (este arquivo)
```

Pontos importantes:

- **`src/` e `include/` espelham a mesma estrutura.** Para cada `src/http/HttpRequest.cpp` existe `include/http/HttpRequest.hpp`. O Makefile usa `-I include` para resolver headers.
- **Configs, www, cgi-bin separados do código** — durante a defesa, quando o avaliador pedir pra demonstrar um recurso, vocês têm um arquivo `.conf` pronto pra cada cenário. Isso impressiona e poupa tempo.
- **`uploads/`** entra no git só com um `.gitkeep` (a pasta precisa existir, mas o conteúdo é runtime).
- **`docs/`** é onde vão documentos da equipe (este guia, decisões arquiteturais, ata de decisões importantes se vocês quiserem rastrear).

`.gitignore` mínimo:

```
# binário
webserv

# build artifacts
*.o
*.d
*.dSYM/

# editor
.vscode/
.idea/
*.swp

# OS
.DS_Store
Thumbs.db

# uploads em runtime
uploads/*
!uploads/.gitkeep
```

### Convenções de nomenclatura (C++)

Decidam isso na primeira reunião e mantenham. Sugestão alinhada com convenções comuns de C++ moderno e legível pra quem revisa:

| Elemento | Convenção | Exemplo |
|---|---|---|
| Classes / structs | `PascalCase` | `HttpRequest`, `ConfigParser`, `EventLoop` |
| Métodos | `camelCase` | `parseRequest()`, `handleConnection()` |
| Variáveis locais | `camelCase` | `requestBody`, `clientFd`, `bytesRead` |
| Variáveis-membro | `_camelCase` (underscore inicial) | `_socket`, `_buffer`, `_config` |
| Parâmetros | `camelCase` | `void setStatus(int statusCode)` |
| Constantes / macros | `SCREAMING_SNAKE_CASE` | `MAX_BUFFER_SIZE`, `DEFAULT_PORT` |
| Enums | tipo `PascalCase`, valores `PascalCase` | `enum HttpMethod { Get, Post, Delete };` |
| Arquivos `.cpp`/`.hpp` | `PascalCase` igual à classe | `HttpRequest.hpp`, `HttpRequest.cpp` |
| Pastas | `lowercase` ou `kebab-case` | `src/`, `cgi-bin/` |
| Diretivas de configuração | `snake_case` (estilo NGINX) | `client_max_body_size`, `allowed_methods` |

**Por que prefixar variáveis-membro com `_`:** elimina a ambiguidade clássica em construtores e setters. Em vez de `name = name;` (que não funciona), vira `_name = name;` — sem precisar de `this->` o tempo todo. É opinião, mas pelo menos sejam consistentes.

**Header guards:** usem `#ifndef`/`#define`/`#endif`. Não usem `#pragma once` — não é C++98 padrão e o subject exige conformidade estrita.

```cpp
#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

class HttpRequest {
    // ...
};

#endif // HTTP_REQUEST_HPP
```

**Ordem de includes** (separadas por linha em branco) — ajuda a detectar dependências escondidas:

```cpp
// 1. header da própria classe (só no .cpp)
#include "HttpRequest.hpp"

// 2. outros headers do projeto
#include "RequestParser.hpp"
#include "MimeTypes.hpp"

// 3. STL
#include <string>
#include <map>
#include <vector>

// 4. C standard library
#include <cstring>
#include <cstdlib>

// 5. system headers
#include <sys/socket.h>
#include <unistd.h>
```

### Convenção de commits

Adotem **Conventional Commits**. Trabalhar em 3 sem isso vira inferno quando alguém precisa entender o que mudou em duas semanas atrás durante o debug pré-defesa.

Formato: `<tipo>(<escopo>): <descrição curta no imperativo, sem ponto final>`

**Tipos:**
- `feat` — nova funcionalidade (handler novo, recurso de config novo)
- `fix` — correção de bug
- `refactor` — mudança que não altera comportamento externo
- `docs` — só documentação (README, comentários)
- `test` — adição ou ajuste de testes
- `chore` — manutenção (Makefile, .gitignore, dependências)
- `style` — formatação, não muda lógica
- `perf` — otimização de performance

**Escopo (opcional mas recomendado):** o módulo afetado — `config`, `http`, `cgi`, `network`, `server`, `tests`, `make`, etc.

**Bons:**

```
feat(http): parsear chunked transfer encoding em request body
fix(cgi): coletar processo filho com waitpid para evitar zombies
refactor(network): extrair Socket de Connection
docs(readme): adicionar instruções de build no macOS
test(upload): cobrir multipart com múltiplos campos
chore(make): adicionar regra re sem religação desnecessária
fix(http): retornar 413 quando body excede client_max_body_size
```

**Ruins (não façam):**

```
update                          (vago)
fix bug                         (qual bug?)
WIP                             (não vai pro main)
asdf                            (sem comentários)
"final final v3 agora vai"      (você sabe o que fez)
```

**Commit multi-linha** quando precisar contextualizar uma decisão:

```
feat(cgi): integrar pipes do CGI ao event loop principal

Antes os pipes do CGI eram lidos com read() bloqueante depois do
fork, o que violava a regra do subject de toda I/O passar pelo poll.
Agora os fds dos pipes são adicionados ao poll com POLLIN/POLLOUT
e drenados nos ciclos do event loop normal, junto com sockets de
cliente.

Closes #14
```

### Fluxo de Git (3 pessoas)

**Não trabalhem direto em `main`.** Mesmo em time pequeno isso quebra demais.

Modelo (Git Flow simplificado):

- `main` — só recebe merges quando algo está testado e funcional. Tags em cada milestone: `v0.1-static`, `v0.2-config`, `v0.3-cgi`, etc.
- `dev` — branch de integração onde os trabalhos individuais se juntam.
- `feat/<nome-curto>` — uma branch por feature. Ex.: `feat/config-parser`, `feat/cgi-handler`, `feat/upload-multipart`.
- `fix/<nome-curto>` — para correções pontuais.

**Fluxo de cada feature:**

1. `git checkout -b feat/config-parser dev`
2. Trabalha, commita seguindo Conventional Commits.
3. `git push origin feat/config-parser` e abre Pull Request `feat/config-parser → dev`.
4. **Pelo menos um dos outros 2 revisa** antes do merge. Sempre. Mesmo que pareça óbvio. Dois motivos: (a) o subject obriga que qualquer um do grupo saiba explicar qualquer parte na defesa — se você nunca leu a parte do colega, vai vacilar; (b) revisar é onde mais se aprende.
5. Squash merge se a branch tem muitos commits "WIP", merge commit normal se tem histórico limpo.
6. No fim de cada milestone, merge `dev → main` e tag.

**Regras práticas:**
- Branch nunca passa de ~3 dias sem ser mergeada (evita conflitos gigantes).
- PR tem descrição mínima: o que foi feito + como testar.
- Conflitos: quem abre o PR resolve, não o reviewer.

### Code review entre os 3

A defesa exige que **qualquer um do grupo saiba explicar qualquer parte do código**. Isso só acontece com revisão cruzada desde o dia 1.

Regras:

- Toda PR tem pelo menos 1 reviewer. Idealmente, em features grandes, os 2 outros membros revisam.
- Reviewer pergunta "por quê" sempre que não entender — não basta ler, tem que entender.
- Se você precisou explicar algo no PR para o reviewer, considere se isso não devia ser um comentário no código.
- Discussões longas no PR → migrem pra chamada síncrona. Não percam 2h trocando texto.
- Reviews começam pelo design (a estrutura faz sentido?) antes de ir pro detalhe (esse `for` poderia ser `while`?).

### Documentação no código

C++98 não tem documentação obrigatória, mas comentários estratégicos salvam a defesa:

- **Topo de cada classe:** uma frase dizendo *qual o propósito da classe*. Não é resumo do código, é o porquê dela existir.
- **Funções não-óbvias:** comentário explicando *por que*, não *o que*. O "o que" o código já mostra.
- **Decisões arquiteturais:** sempre que escolherem algo não-trivial, comentem. Exemplos:
  - "Usamos `poll()` em vez de `epoll()` para portabilidade Linux/macOS sem `#ifdef` espalhado."
  - "Buffer de leitura é `std::string` em vez de `char[]` porque precisamos crescer dinamicamente até `client_max_body_size`."
  - "Header `Connection: close` forçado nas respostas de erro para garantir que cliente desconecte e não fique pendurado."

Esses comentários salvam você quando o avaliador perguntar "por que assim e não assado".

### Ferramentas que ajudam (não obrigatórias)

- **`.editorconfig`** — alinha tabs/espaços/charset entre os 3 editores. Sem isso, PR fica cheio de ruído de whitespace.
- **`.clang-format`** com config compartilhada — rodando antes de commitar, fim das discussões de estilo.
- **CI básico (GitHub Actions)** rodando `make` a cada PR — pega quebra de build sem precisar baixar a branch.
- **Issues do GitHub ou qualquer board (Trello, Notion, Linear)** pra rastrear quem está fazendo o quê e evitar 2 pessoas começarem a mesma feature.

---

## Roteiro sugerido (~3-4 semanas, paralelo ao código)

### Semana 1 — Fundamentos + warm-up

**Todos:**
- Ler RFC 7230 e 7231 (HTTP/1.1) — pelo menos as seções "Message Format", "Methods" e "Status Codes".
- Ler Beej's Guide to Network Programming (capítulos 1–6).
- Sentar 2h numa sala virtual e fazer requisições com `telnet` contra o NGINX local. Anotar tudo que viram nos headers.

**Cada um, individualmente (não vai pro projeto, é exercício):**
- Escrever um echo server TCP em C++98, single-client, blocking. (~1 dia)
- Evoluir esse echo server para multi-client, não-bloqueante, com `poll()`. (~1 dia)

### Semana 2 — Core do servidor (divisão de tarefas)

A partir daqui o trabalho diverge. Sugestão de divisão:

- **Pessoa A:** parser de configuração + estrutura de dados de servers/locations.
- **Pessoa B:** parser HTTP de requisição + builder de resposta + servir arquivo estático.
- **Pessoa C:** socket layer (listen sockets, accept) + event loop + integração inicial.

Critério de fim de semana 2: `curl http://localhost:8080/` baixa um `index.html` estático.

### Semana 3 — Recursos avançados

- **Pessoa A:** location matching completo + autoindex + redirects + páginas de erro custom.
- **Pessoa B:** upload (multipart/form-data) + DELETE + chunked transfer encoding.
- **Pessoa C:** CGI (a parte mais delicada — fork + pipes + integração com o poll).

Critério de fim de semana 3: navegador abre o site, carrega CSS/JS/imagens, formulário de upload funciona, script CGI responde.

### Semana 4 — Testes, polimento, README

- Suite de testes em Python.
- Stress test (1000+ conexões simultâneas, sem crashar).
- Comparações ponto-a-ponto com NGINX.
- README com seções exigidas + arquivos de configuração de exemplo cobrindo todos os recursos.
- Sessão coletiva de troca de revisão de código.

---

## Recursos recomendados

### Leitura obrigatória

- **RFC 7230** — HTTP/1.1: Message Syntax and Routing. https://datatracker.ietf.org/doc/html/rfc7230
- **RFC 7231** — HTTP/1.1: Semantics and Content. https://datatracker.ietf.org/doc/html/rfc7231
- **RFC 3875** — The Common Gateway Interface (CGI/1.1). https://datatracker.ietf.org/doc/html/rfc3875
- **Beej's Guide to Network Programming** — gratuito, didático, é A referência. https://beej.us/guide/bgnet/

### Documentação NGINX

- nginx.org/en/docs/ — especialmente as diretivas das seções `server` e `location`.
- A configuração default do NGINX local (`/etc/nginx/nginx.conf`) é um ótimo exemplo prático.

### Man pages essenciais

`socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`, `recv(2)`, `send(2)`, `poll(2)`, `select(2)`, `epoll(7)`, `kqueue(2)`, `fcntl(2)`, `fork(2)`, `execve(2)`, `pipe(2)`, `dup2(2)`, `waitpid(2)`, `stat(2)`, `opendir(3)`, `signal(2)`, `getaddrinfo(3)`.

### Referências práticas

- **MDN Web Docs — HTTP:** muito bom pra entender headers, status codes, métodos do ponto de vista do cliente. https://developer.mozilla.org/en-US/docs/Web/HTTP
- **HTTP Made Really Easy:** introdução curta, didática. https://www.jmarshall.com/easy/http/

### Inspiração de código (LER, não copiar)

- `tinyhttpd` (GitHub) — servidor HTTP minimalista em C, ~500 linhas. Bom pra ver o esqueleto.
- `mongoose` — embedded HTTP server em C, mais complexo mas com event loop e CGI.

⚠️ **Lembrete sobre IA (do subject):** podem usar para ajudar a entender conceitos, projetar parser, debugar. **Não podem** entregar código que não saibam explicar linha por linha — isso reprova na defesa. A regra que cada um deve aplicar: "se eu fui questionado sobre essa função e travei, é porque copiei sem entender".

---

## Checklist final antes da defesa

- [ ] Compila com `c++ -Wall -Wextra -Werror -std=c++98` sem warnings.
- [ ] Makefile com regras `all`, `clean`, `fclean`, `re`, e sem religação desnecessária.
- [ ] Sem bibliotecas externas. Sem Boost.
- [ ] **Toda** operação de read/write passa por poll (auditoria coletiva no código!).
- [ ] Servidor sobrevive a stress test sem crashar nem vazar memória.
- [ ] Navegador abre o site corretamente (HTML + CSS + JS + imagens).
- [ ] GET, POST, DELETE funcionam.
- [ ] Upload de arquivo via formulário HTML funciona.
- [ ] CGI (PHP ou Python) funciona.
- [ ] Status codes batem com NGINX em casos comuns.
- [ ] Páginas de erro padrão aparecem quando config não define custom.
- [ ] Múltiplas portas escutando simultaneamente, com conteúdos diferentes.
- [ ] README completo com todas as seções exigidas + descrição honesta de uso de IA.
- [ ] Cada um dos 3 sabe explicar TODA a base de código, não só sua parte.

---

*Bom projeto. Quando empacar, lembre que o subject explicitamente sugere usar `telnet` e `NGINX` como referências — eles cobrem 90% das dúvidas de comportamento.*
