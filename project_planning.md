--------------------------------------------------
MILESTONE 1 — FOUNDATION & NETWORKING
--------------------------------------------------

### Issue #01 — chore: project structure
- Story Points: 2
- Branch: `chore/project-structure`
- Depends on: —
- Blocked by: —
- Commit: `chore: create initial project structure`
- PR title: `chore: project structure scaffold`
- PR description: Cria diretórios base (`srcs/`, `includes/`, `config/`, `www/`, `tests/`) e placeholders mínimos.

Scope:
- Criar a árvore de diretórios do projeto

Acceptance:
- Build ainda pode falhar (sem código), mas estrutura existe e está organizada

---

### Issue #02 — chore: Makefile C++98
- Story Points: 2
- Branch: `chore/makefile`
- Depends on: #01
- Blocked by: —
- Commit: `chore: add C++98 Makefile with required rules`
- PR title: `chore: C++98 Makefile`
- PR description: Makefile com regras `all/clean/fclean/re` e sem relink quando nada muda.

Scope:
- Makefile sem globbing
- Flags obrigatórias

Acceptance:
- `make`, `make clean`, `make fclean`, `make re` funcionam

---

### Issue #03 — feat: basic Logger
- Story Points: 1
- Branch: `feat/logger`
- Depends on: #01
- Blocked by: —
- Commit: `feat: add minimal logger utility`
- PR title: `feat: logger`
- PR description: Logger simples para debug (info/warn/error) sem acoplar no resto.

Scope:
- Logger utilitário

Acceptance:
- Mensagens aparecem em stdout/stderr sem crash

---

### Issue #04 — feat: socket base helpers
- Story Points: 5
- Branch: `feat/socket-base`
- Depends on: #01
- Blocked by: —
- Commit: `feat: add socket helpers for bind/listen`
- PR title: `feat: socket base helpers`
- PR description: Wrapper/Helpers para `socket/setsockopt/bind/listen/close` com tratamento de erro.

Scope:
- Criação de socket TCP
- `SO_REUSEADDR`

Acceptance:
- Funções encapsuladas compilam e reportam erros claramente

---

### Issue #05 — feat: listening socket
- Story Points: 3
- Branch: `feat/listen-socket`
- Depends on: #04
- Blocked by: #04
- Commit: `feat: implement listening socket creation`
- PR title: `feat: listening socket`
- PR description: Cria socket de escuta pronto para aceitar conexões.

Scope:
- `listen()` + backlog

Acceptance:
- Porta fica aberta e aceita conexão TCP (ainda sem HTTP)

---

### Issue #06 — feat: non-blocking sockets
- Story Points: 2
- Branch: `feat/non-blocking-sockets`
- Depends on: #05
- Blocked by: #05
- Commit: `feat: set sockets to non-blocking mode`
- PR title: `feat: non-blocking sockets`
- PR description: Aplica `O_NONBLOCK` em listening e client sockets.

Scope:
- `fcntl(..., O_NONBLOCK)`

Acceptance:
- Server não trava em `accept/recv/send` quando não há dados

---

### Issue #07 — feat: event loop with poll()
- Story Points: 8
- Branch: `feat/event-loop`
- Depends on: #06
- Blocked by: #06
- Commit: `feat: add single poll() event loop skeleton`
- PR title: `feat: poll-based event loop`
- PR description: Implementa loop principal com **uma única** call-site de `poll()`.

Scope:
- Estrutura do loop
- `pollfd` para listening sockets

Acceptance:
- Rodar sem bloquear; logar readiness

---

### Issue #08 — feat: accept multiple clients
- Story Points: 5
- Branch: `feat/client-accept`
- Depends on: #07
- Blocked by: #07
- Commit: `feat: accept clients and register in poll`
- PR title: `feat: accept multiple clients`
- PR description: `accept()` em `POLLIN` do listening fd e adiciona client fd no poll.

Scope:
- Aceitar múltiplos clients
- Remover/fechar corretamente no erro

Acceptance:
- Múltiplos `nc`/`telnet` conectam sem travar

---

### Issue #09 — feat: Connection state struct
- Story Points: 3
- Branch: `feat/connection-class`
- Depends on: #08
- Blocked by: #08
- Commit: `feat: add Connection state for per-client buffers`
- PR title: `feat: connection state`
- PR description: Estrutura por conexão (buffers, estado do parser, timestamps e estado CGI).

Scope:
- `fd`, `read_buffer`, `write_buffer`
- `last_activity`
- placeholders de estado (parser + CGI)

Acceptance:
- Server consegue manter estado por conexão

--------------------------------------------------
MILESTONE 2 — HTTP CORE
--------------------------------------------------

### Issue #10 — feat: HttpRequest model
- Story Points: 2
- Branch: `feat/http-request`
- Depends on: #09
- Blocked by: #09
- Commit: `feat: add HttpRequest data model`
- PR title: `feat: HttpRequest`
- PR description: Define struct/classe HttpRequest com campos essenciais.

Scope:
- method, uri/path, query, version, headers, body

Acceptance:
- Compila em C++98

---

### Issue #11 — feat: HttpResponse model
- Story Points: 2
- Branch: `feat/http-response`
- Depends on: #09
- Blocked by: #09
- Commit: `feat: add HttpResponse data model`
- PR title: `feat: HttpResponse`
- PR description: Define struct/classe HttpResponse e helpers de status.

Scope:
- status, headers, body

Acceptance:
- Compila em C++98

---

### Issue #12 — feat: incremental HTTP request parser
- Story Points: 8
- Branch: `feat/request-parser`
- Depends on: #10
- Blocked by: #10
- Commit: `feat: implement incremental HttpRequestParser state machine`
- PR title: `feat: incremental HTTP request parser`
- PR description: Parser incremental com estados request-line → headers → body.

Scope:
- Parser alimentado por chunks (recv fragmentado)
- Manter estado por conexão

Acceptance:
- Parseia request-line simples e detecta fim de headers (`\r\n\r\n`)

---

### Issue #13 — feat: HTTP header parsing
- Story Points: 5
- Branch: `feat/header-parser`
- Depends on: #12
- Blocked by: #12
- Commit: `feat: parse HTTP headers (case-insensitive)`
- PR title: `feat: header parsing`
- PR description: Parseia headers `Name: Value` e normaliza keys.

Scope:
- Validar `Host` (HTTP/1.1)
- Capturar `Content-Length`, `Content-Type`, `Connection`, `Transfer-Encoding`

Acceptance:
- Headers acessíveis case-insensitive

---

### Issue #14 — feat: request body via Content-Length
- Story Points: 5
- Branch: `feat/request-body`
- Depends on: #13
- Blocked by: #13
- Commit: `feat: read request body using Content-Length`
- PR title: `feat: Content-Length body support`
- PR description: Após headers, lê exatamente N bytes de body sem bloquear.

Scope:
- Acumular body incrementalmente
- Respeitar limite de tamanho (config)

Acceptance:
- POST com `Content-Length` chega completo no handler

---

### Issue #15 — feat: HTTP response builder
- Story Points: 5
- Branch: `feat/response-builder`
- Depends on: #11
- Blocked by: #11
- Commit: `feat: add HttpResponseBuilder with common headers`
- PR title: `feat: response builder`
- PR description: Builder de resposta com Status-Line e headers padrão.

Scope:
- `Content-Length`, `Content-Type`, `Date`, `Server`, `Connection`

Acceptance:
- Respostas válidas HTTP/1.1 (CRLF correto)

---

### Issue #16 — feat: MIME types
- Story Points: 2
- Branch: `feat/mime-types`
- Depends on: #15
- Blocked by: #15
- Commit: `feat: add MIME type resolver by extension`
- PR title: `feat: MIME types`
- PR description: Mapeia extensão → Content-Type.

Scope:
- `.html`, `.css`, `.js`, `.png`, `.jpg`, fallback

Acceptance:
- Response builder seta Content-Type correto para arquivos comuns

---

### Issue #17 — feat: basic Router
- Story Points: 5
- Branch: `feat/router`
- Depends on: #12
- Blocked by: #12
- Commit: `feat: route requests to handlers`
- PR title: `feat: router`
- PR description: Roteia request → handler por método/path (ainda sem config avançada).

Scope:
- Resolver path físico a partir de root padrão

Acceptance:
- GET / retorna algum recurso (ou 404 controlado)

---

### Issue #18 — feat: static file serving (GET)
- Story Points: 5
- Branch: `feat/static-files`
- Depends on: #17
- Blocked by: #17
- Commit: `feat: serve static files with GET`
- PR title: `feat: static files`
- PR description: Servir arquivos estáticos com `stat/open/read`.

Scope:
- Arquivo regular
- Diretório com index (se configurado)

Acceptance:
- GET em arquivo existente retorna 200 + body

---

### Issue #19 — feat: default error pages
- Story Points: 3
- Branch: `feat/error-pages`
- Depends on: #18
- Blocked by: #18
- Commit: `feat: add default HTML error pages`
- PR title: `feat: default error pages`
- PR description: Bodies padrão para erros mínimos.

Scope:
- 400, 403, 404, 405, 500

Acceptance:
- Erros retornam HTML simples e status correto

---

### Issue #20 — fix: path traversal protection
- Story Points: 3
- Branch: `fix/path-traversal`
- Depends on: #18
- Blocked by: #18
- Commit: `fix: prevent path traversal outside root`
- PR title: `fix: path traversal protection`
- PR description: Normaliza path e bloqueia `..` escapando do root.

Scope:
- Normalização de caminho e validação

Acceptance:
- GET `/../etc/passwd` retorna 403

--------------------------------------------------
MILESTONE 3 — CONFIG SYSTEM (NGINX-LIKE)
--------------------------------------------------

### Issue #21 — feat: config lexer
- Story Points: 5
- Branch: `feat/config-lexer`
- Depends on: #01
- Blocked by: —
- Commit: `feat: implement config lexer`
- PR title: `feat: config lexer`
- PR description: Lexer tokeniza palavras, `{}`, `;`, ignora espaços e comentários `#`.

Scope:
- Tokens com posição (linha/coluna)

Acceptance:
- Tokenização correta em configs simples

---

### Issue #22 — feat: config parser (AST/struct)
- Story Points: 8
- Branch: `feat/config-parser`
- Depends on: #21
- Blocked by: #21
- Commit: `feat: implement nginx-like config parser`
- PR title: `feat: config parser`
- PR description: Parser para blocos `server { ... }` e `location <path> { ... }`.

Scope:
- Erros com linha/coluna

Acceptance:
- Parse de um server básico sem crash

---

### Issue #23 — feat: ServerConfig + LocationConfig
- Story Points: 3
- Branch: `feat/config-structures`
- Depends on: #22
- Blocked by: #22
- Commit: `feat: add config structs for server/location`
- PR title: `feat: config structures`
- PR description: Estruturas completas usadas em runtime.

Scope (mínimo):
- `listen`, `server_name`, `root`, `index`
- `error_page`, `client_max_body_size`
- `autoindex`, `limit_except`
- `upload_store`, `cgi_pass`, `return`

Acceptance:
- Compila e é consumível pelo servidor

---

### Issue #24 — feat: parse listen
- Story Points: 3
- Branch: `feat/config-listen`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse listen directive`
- PR title: `feat: config listen`
- PR description: `listen host:port;` com validação.

Acceptance:
- Abre porta correta no runtime

---

### Issue #25 — feat: parse server_name
- Story Points: 2
- Branch: `feat/config-server-name`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse server_name directive`
- PR title: `feat: config server_name`
- PR description: `server_name ...;` para match por header Host.

Acceptance:
- Host header escolhe server correto (quando múltiplos)

---

### Issue #26 — feat: parse root
- Story Points: 3
- Branch: `feat/config-root`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse root directive`
- PR title: `feat: config root`
- PR description: `root /path;` em server/location.

Acceptance:
- GET resolve path físico com root

---

### Issue #27 — feat: parse index
- Story Points: 2
- Branch: `feat/config-index`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse index directive`
- PR title: `feat: config index`
- PR description: `index index.html;` para diretórios.

Acceptance:
- GET em diretório serve index quando existir

---

### Issue #28 — feat: parse error_page
- Story Points: 3
- Branch: `feat/config-error-page`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse error_page directive`
- PR title: `feat: config error_page`
- PR description: `error_page 404 /errors/404.html;` com validação.

Acceptance:
- Erros usam páginas configuradas quando presentes

---

### Issue #29 — feat: parse client_max_body_size
- Story Points: 3
- Branch: `feat/config-body-size`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse client_max_body_size with suffixes`
- PR title: `feat: config client_max_body_size`
- PR description: Suporta sufixos K/M/G e aplica limite no parser.

Acceptance:
- Body acima do limite retorna 413

---

### Issue #30 — feat: parse allowed methods (limit_except)
- Story Points: 3
- Branch: `feat/config-methods`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse limit_except allowed methods`
- PR title: `feat: config limit_except`
- PR description: `limit_except GET POST;` por location.

Acceptance:
- Métodos não permitidos retornam 405 + Allow

---

### Issue #31 — feat: parse autoindex
- Story Points: 2
- Branch: `feat/config-autoindex`
- Depends on: #23
- Blocked by: #23
- Commit: `feat: parse autoindex on/off`
- PR title: `feat: config autoindex`
- PR description: `autoindex on|off;` por location.

Acceptance:
- Diretórios sem index geram listing quando on

---

### Issue #32 — feat: location matching (longest prefix)
- Story Points: 5
- Branch: `feat/location-matching`
- Depends on: #17
- Blocked by: #17
- Commit: `feat: match locations using longest prefix rule`
- PR title: `feat: location matching`
- PR description: Implementa algoritmo de match de location tipo NGINX (prefixo mais longo).

Depends on #23 (config structs) e #30/#31 (diretivas) implicitamente.

Acceptance:
- Paths escolhem a location correta

---

### Issue #33 — feat: HTTP redirects (return 301/302)
- Story Points: 2
- Branch: `feat/http-redirects`
- Depends on: #32
- Blocked by: #32
- Commit: `feat: implement return 301/302 redirects`
- PR title: `feat: redirects`
- PR description: `return 301 /new;` em location.

Acceptance:
- Resposta com status + header Location

--------------------------------------------------
MILESTONE 4 — ADVANCED HTTP FEATURES
--------------------------------------------------

### Issue #34 — feat: POST method plumbing
- Story Points: 3
- Branch: `feat/http-post`
- Depends on: #14
- Blocked by: #14
- Commit: `feat: implement POST handler flow`
- PR title: `feat: POST support`
- PR description: Dispatch do método POST, validações e status codes básicos.

Acceptance:
- POST retorna 201/200 conforme handler

---

### Issue #35 — feat: DELETE method
- Story Points: 3
- Branch: `feat/http-delete`
- Depends on: #17
- Blocked by: #17
- Commit: `feat: implement DELETE for regular files`
- PR title: `feat: DELETE support`
- PR description: Remove arquivo regular quando permitido.

Acceptance:
- DELETE retorna 204 quando remove

---

### Issue #36 — feat: multipart/form-data upload parsing
- Story Points: 8
- Branch: `feat/multipart-upload`
- Depends on: #34
- Blocked by: #34
- Commit: `feat: parse multipart/form-data and extract file parts`
- PR title: `feat: multipart upload parsing`
- PR description: Parseia `Content-Type: multipart/form-data; boundary=...` e extrai arquivo(s).

Scope:
- Extrair `filename` e conteúdo do arquivo
- Validar limites e boundary

Acceptance:
- Upload via `<form enctype=multipart/form-data>` salva arquivo correto

---

### Issue #37 — feat: upload storage integration
- Story Points: 3
- Branch: `feat/upload-storage`
- Depends on: #36
- Blocked by: #36
- Commit: `feat: save uploaded files to upload_store`
- PR title: `feat: upload storage`
- PR description: Escreve arquivos em `upload_store` configurado e retorna Location.

Acceptance:
- Arquivo aparece no diretório configurado

---

### Issue #38 — feat: autoindex response
- Story Points: 3
- Branch: `feat/autoindex`
- Depends on: #18
- Blocked by: #18
- Commit: `feat: generate directory listing when autoindex on`
- PR title: `feat: autoindex`
- PR description: Gera HTML listando entradas de diretório.

Acceptance:
- GET em diretório sem index retorna listing quando on

---

### Issue #39 — feat: Transfer-Encoding chunked parsing
- Story Points: 8
- Branch: `feat/chunked-parser`
- Depends on: #14
- Blocked by: #14
- Commit: `feat: implement chunked request body decoding`
- PR title: `feat: chunked request decoding`
- PR description: Suporta request body com `Transfer-Encoding: chunked`.

Acceptance:
- POST chunked chega com body decodificado

---

### Issue #40 — feat: keep-alive
- Story Points: 5
- Branch: `feat/keep-alive`
- Depends on: #15
- Blocked by: #15
- Commit: `feat: implement HTTP keep-alive connection handling`
- PR title: `feat: keep-alive`
- PR description: Reutiliza conexão para múltiplas requests quando permitido.

Acceptance:
- Uma conexão envia 2 requests sequenciais e recebe 2 respostas

---

### Issue #41 — feat: connection timeouts
- Story Points: 3
- Branch: `feat/connection-timeouts`
- Depends on: #40
- Blocked by: #40
- Commit: `feat: close idle connections on timeout`
- PR title: `feat: connection timeouts`
- PR description: Fecha conexões ociosas para evitar vazamento de recursos.

Acceptance:
- Conexões ociosas são encerradas após N segundos

---

### Issue #42 — fix: SIGPIPE handling
- Story Points: 1
- Branch: `fix/sigpipe-handling`
- Depends on: #07
- Blocked by: #07
- Commit: `fix: ignore SIGPIPE to prevent server crash`
- PR title: `fix: SIGPIPE handling`
- PR description: Ignora SIGPIPE para evitar crash ao escrever em socket fechado.

Acceptance:
- Server não encerra quando client fecha abruptamente

--------------------------------------------------
MILESTONE 5 — CGI SYSTEM
--------------------------------------------------

### Issue #43 — feat: CgiHandler skeleton
- Story Points: 5
- Branch: `feat/cgi-handler`
- Depends on: #07
- Blocked by: #07
- Commit: `feat: add CgiHandler interface and validation`
- PR title: `feat: CGI handler skeleton`
- PR description: Define detecção de request CGI e validações básicas.

Acceptance:
- Requests CGI são identificadas (sem executar ainda)

---

### Issue #44 — feat: CGI pipes
- Story Points: 5
- Branch: `feat/cgi-pipes`
- Depends on: #43
- Blocked by: #43
- Commit: `feat: create stdin/stdout pipes for CGI processes`
- PR title: `feat: CGI pipes`
- PR description: Cria pipes para comunicar body → CGI e output ← CGI.

Acceptance:
- Pipes criados e fechados corretamente

---

### Issue #45 — feat: fork + execve for CGI
- Story Points: 8
- Branch: `feat/cgi-exec`
- Depends on: #44
- Blocked by: #44
- Commit: `feat: spawn CGI process with fork/execve`
- PR title: `feat: CGI process execution`
- PR description: `fork()` e `execve()` com redirecionamento de stdin/stdout.

Acceptance:
- Script simples executa e escreve algo

---

### Issue #46 — feat: CGI environment variables
- Story Points: 5
- Branch: `feat/cgi-env`
- Depends on: #45
- Blocked by: #45
- Commit: `feat: build CGI env vars per RFC 3875`
- PR title: `feat: CGI env vars`
- PR description: Monta envs: REQUEST_METHOD, CONTENT_LENGTH, QUERY_STRING, SCRIPT_FILENAME, SERVER_PROTOCOL, etc.

Acceptance:
- CGI recebe variáveis corretas

---

### Issue #47 — feat: integrate CGI pipes into poll()
- Story Points: 8
- Branch: `feat/cgi-event-loop`
- Depends on: #46
- Blocked by: #46
- Commit: `feat: drive CGI IO via poll()`
- PR title: `feat: CGI integration with event loop`
- PR description: Registra fds de pipe no poll e faz IO incremental.

Acceptance:
- Server continua responsivo com CGI rodando

---

### Issue #48 — feat: incremental CGI output parsing
- Story Points: 8
- Branch: `feat/cgi-output-parser`
- Depends on: #47
- Blocked by: #47
- Commit: `feat: parse CGI output to HTTP response incrementally`
- PR title: `feat: CGI output parsing`
- PR description: Converte output CGI (headers + body) em resposta HTTP do servidor.

Acceptance:
- CGI retorna status/headers/body corretamente

---

### Issue #49 — feat: CGI timeout
- Story Points: 3
- Branch: `feat/cgi-timeout`
- Depends on: #47
- Blocked by: #47
- Commit: `feat: enforce CGI execution timeout`
- PR title: `feat: CGI timeout`
- PR description: Mata CGI travado e responde 504/500 conforme política.

Acceptance:
- CGI que dorme demais é encerrado

---

### Issue #50 — feat: PHP CGI support
- Story Points: 2
- Branch: `feat/php-cgi`
- Depends on: #48
- Blocked by: #48
- Commit: `feat: support php-cgi execution via cgi_pass`
- PR title: `feat: PHP CGI`
- PR description: Suporte de execução via handler configurado.

Acceptance:
- `.php` responde via php-cgi

---

### Issue #51 — feat: Python CGI support
- Story Points: 2
- Branch: `feat/python-cgi`
- Depends on: #48
- Blocked by: #48
- Commit: `feat: support python CGI execution via cgi_pass`
- PR title: `feat: Python CGI`
- PR description: Suporte para scripts Python.

Acceptance:
- `.py` responde via python

--------------------------------------------------
MILESTONE 6 — TESTING & STABILITY
--------------------------------------------------

### Issue #52 — test: HTTP test suite
- Story Points: 5
- Branch: `test/http-suite`
- Depends on: #18
- Blocked by: #18
- Commit: `test: add HTTP integration suite`
- PR title: `test: HTTP suite`
- PR description: Suite de testes com curl/python para GET/headers/status.

Acceptance:
- Testes automatizados rodam localmente

---

### Issue #53 — test: upload suite
- Story Points: 3
- Branch: `test/upload-suite`
- Depends on: #36
- Blocked by: #36
- Commit: `test: add multipart upload tests`
- PR title: `test: upload suite`
- PR description: Testes para multipart e armazenamento.

Acceptance:
- Upload test passa e valida arquivo salvo

---

### Issue #54 — test: CGI suite
- Story Points: 3
- Branch: `test/cgi-suite`
- Depends on: #48
- Blocked by: #48
- Commit: `test: add CGI integration tests`
- PR title: `test: CGI suite`
- PR description: Testa php/python cgi e edge cases.

Acceptance:
- Respostas CGI conformes

---

### Issue #55 — test: stress test
- Story Points: 5
- Branch: `test/stress-test`
- Depends on: #07
- Blocked by: #07
- Commit: `test: add stress test scripts (ab/wrk/curl)`
- PR title: `test: stress tests`
- PR description: Cenários para carga, many clients, timeouts.

Acceptance:
- Rodar stress não derruba o servidor

---

### Issue #56 — test: compare against NGINX
- Story Points: 3
- Branch: `test/nginx-compare`
- Depends on: #15
- Blocked by: #15
- Commit: `test: add nginx behavior comparison cases`
- PR title: `test: nginx compare`
- PR description: Casos que comparam headers/status com NGINX.

Acceptance:
- Diferenças justificadas/documentadas

---

### Issue #57 — fix: memory leaks (valgrind)
- Story Points: 5
- Branch: `fix/memory-leaks`
- Depends on: #56
- Blocked by: #56
- Commit: `fix: address memory leaks and FD leaks`
- PR title: `fix: leaks`
- PR description: Fecha fds, libera memória e corrige ownership.

Acceptance:
- Valgrind sem leaks relevantes

---

### Issue #58 — fix: cleanup and graceful shutdown
- Story Points: 5
- Branch: `fix/cleanup`
- Depends on: #57
- Blocked by: #57
- Commit: `fix: add centralized cleanup and shutdown paths`
- PR title: `fix: cleanup`
- PR description: Cleanup central (fds, CGI, buffers) sem duplicação.

Acceptance:
- Encerrar servidor não deixa recursos pendentes

---

### Issue #59 — docs: demo configs + final README
- Story Points: 2
- Branch: `docs/demo-configs`
- Depends on: #22
- Blocked by: #22
- Commit: `docs: add demo configs and final README`
- PR title: `docs: demo configs and README`
- PR description: Adiciona `static.conf`, `upload.conf`, `cgi.conf`, `multiport.conf` e README final.

Acceptance:
- Repositório pronto para demo/avaliação
