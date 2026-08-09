# Webserv — Subject Compliance Findings

> **Documento superado.** Descreve a árvore em `43b19e5` (06/08/2026). A auditoria válida é
> `AUDIT-2026-08-08.md`, que cruza o código com a régua **e** com o subject, roda os testers
> oficiais do intra e registra o estado de cada uma das 13 issues daqui. Mantido como histórico.

Audit of this repository against the 42 evaluation scale (`Intra Projects Edit.pdf`, scale_teams/9464839).

Each finding below is written to be filed directly as a GitHub issue on the project board.

- **Date:** 2026-08-06
- **Branch:** `development` @ `43b19e5`
- **Platform tested:** macOS (Darwin 25.5.0, arm64), `c++ -std=c++98 -Wall -Wextra -Werror`
- **Method:** static review + runtime exercise (`curl`, `nc`, `siege 4.1.7`, `leaks`)
- **Build:** clean, no relink issues, no warnings

## Summary

| # | Finding | Severity | Scale section |
|---|---------|----------|---------------|
| 1 | `README.md` is missing every required element | **Blocker — grade 0** | README e Verificação de Conformidade |
| 2 | `errno` is checked after `recv`/`send`/`read`/`write` | **Blocker — grade 0** | Verifique o código e faça perguntas |
| 3 | `limit_except` is never parsed — per-route method limits do nothing | High | Configuração |
| 4 | `return` (redirect) is never parsed — redirects 404 | High | Configuração / Navegador |
| 5 | POST ignores `upload_store` and overwrites any file under the root | High | Verificações básicas |
| 6 | CGI script root is hardcoded to `cgi-bin` — `location` root ignored | High | Verificar CGI |
| 7 | `conf/default.conf` upload route is broken end-to-end | High | Verificações básicas |
| 8 | A second `poll()` loop exists outside the main loop | Medium | Verifique o código |
| 9 | Location matching ignores path boundaries | Medium | Configuração |
| 10 | `read`/`write` on file descriptors that never pass through `poll()` | Medium (strict reading) | Verifique o código |
| 11 | Unsupported config directives are silently ignored | Medium | Configuração |
| 12 | `www/errors/404.html` is byte-identical to the built-in fallback | Low | Configuração |
| 13 | Idle and half-open connections are never reaped | Medium | Siege & teste de estresse |

## What already passes

Recorded so the defense can lean on it, and so no one "fixes" a working area:

- **Siege stress — passes comfortably.** `siege -b -c 25 -t120S http://localhost:8080/` → **630,528 transactions, availability 100.00%, 0 failed, longest transaction 550 ms**. A prior `-t15S` run gave 94,689 hits at 100.00%.
- **No memory growth.** RSS sampled every 20 s across the 120 s run: 1472 → 1472 → 1472 → 1472 → 1456 → 1440 KB. Flat/declining.
- **No leaks — static and CGI paths both.** `leaks <pid>` after ~725,000 static requests: `0 leaks for 0 total leaked bytes`. Re-run against a fresh process after exercising all three CGI teardown paths — 200 clean runs (`finishCgi`), 3 client-aborts mid-CGI (`abortCgi`), 2 infinite-loop timeouts (`timeoutCgi`) — also `0 leaks`. `delete proc` is present on every exit path (`src/server/EventLoop.cpp:461,534,565,584`).
- **Survives repeated siege without restart** — three consecutive runs, same process.
- **Survives clients disconnecting mid-CGI.** Repeated `curl -m 1` against a 2 s CGI: process stays alive, still answers `200`, no orphan interpreters.
- **No relink.** A second `make` with no changes reports `Nothing to be done for 'all'.`
- **No crash under malformed input.** `GARBAGE`, `GET /` with no version, `HTTP/9.9`, empty request, missing `Host` → all `400`, process alive.
- **CGI works** (via `/cgi-bin/…`), GET and POST, and **handles broken CGI correctly**: infinite-loop script → `504` after the 5 s timeout, no orphan processes, server still serving. Bad interpreter → `502`.
- **Virtual hosting works.** `conf/multiplename.conf`: `Host: portfolio.com` and `Host: site-da-escola.com` on `:8088` serve different sites; unknown host falls back to the first block.
- **Port conflict handled cleanly.** A second instance on a bound port logs `bind() fail in port 8088` and exits; the original keeps serving.
- **Custom error pages work** when the file differs from the fallback (verified with a sentinel page).
- **Also verified:** `413` over `client_max_body_size`, `405` + `Allow` for unknown methods, `autoindex on` listing, `index` file for directory requests, `DELETE` → `204`, multiple ports in one config (`:8090` + `:8091`).

---

# Issues to file

---

## Issue 1 — README.md is missing every element the scale requires

**Labels:** `compliance`, `blocker`, `documentation`
**Severity:** Blocker — this alone sets the grade to 0

### Scale requirement

> O repositório contém um arquivo README.md em sua raiz, e o arquivo inclui todo o seguinte?
> A primeira linha está em itálico e formatada exatamente como: *Este projeto foi criado como parte da 42 por \<login1\>[, \<login2\>[, \<login3\>]]*.
> Uma seção "Descrição" […] Uma seção "Instruções" […] Uma seção "Recursos" […]
> **Se quaisquer dos elementos requeridos acima estiver faltando, a nota é 0.**

### Evidence

`README.md` contains exactly one line:

```
# 42-web-server
```

Missing: the italic first line, `Descrição`, `Instruções`, `Recursos`.

### Acceptance criteria

- [ ] First line is italic and matches exactly: `*Este projeto foi criado como parte da 42 por <login1>, <login2>, <login3>.*`
- [ ] `## Descrição` — purpose and short overview of the project
- [ ] `## Instruções` — build, install and run details (`make`, `./webserv conf/default.conf`)
- [ ] `## Recursos` — references used, each stating **what task / which part of the project** it was used for
- [ ] File is at the repository root

---

## Issue 2 — `errno` is checked after `recv`/`send`/`read`/`write`

**Labels:** `compliance`, `blocker`, `networking`
**Severity:** Blocker — explicit auto-zero in the scale

### Scale requirement

> **Se errno é verificado após read/recv/write/send, a nota é 0** e o processo de avaliação termina imediatamente (errno pode ser usado apenas para informações de log).

### Evidence

| File:line | Call it follows |
|---|---|
| `src/server/EventLoop.cpp:188` | `recv()` via `Connection::receive_data()` |
| `src/server/EventLoop.cpp:331` | `send()` via `Connection::send_data()` |
| `src/cgi/CgiProcess.cpp:119` | `read()` on the CGI output pipe |
| `src/cgi/CgiProcess.cpp:137` | `write()` on the CGI body pipe |
| `src/cgi/CgiHandler.cpp:280` | `write()` on the CGI body pipe |

`src/server/EventLoop.cpp:188`:

```cpp
ssize_t n = _clients[fd].receive_data();
...
if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return true;
```

`src/server/EventLoop.cpp:331`:

```cpp
ssize_t sent = conn.send_data();

if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return true; // try again later
```

### Related — `send()` returning 0 is not handled

While fixing the above, `EventLoop::handleSend` (`src/server/EventLoop.cpp:331-334`) branches on `sent == -1` twice but never on `sent == 0`. A zero return falls through to the `has_data_to_send()` check and returns `true`, so a socket that repeatedly returns 0 spins without progress and the connection is never closed. The scale is explicit that both values matter:

> verifique se o valor retornado é verificado corretamente (apenas valores -1 ou 0 não é suficiente, **ambos devem ser verificados**).

### Notes

- `src/server/EventLoop.cpp:651` checks `errno == EINTR` after `poll()`, and `src/config/ConfigLoader.cpp:462` checks `ERANGE` after `strtol`. **Neither is in the prohibited list** (`read/recv/write/send`), so they are out of scope for this issue. Consider the `poll()` one a judgment call to discuss at defense.

### Acceptance criteria

- [ ] No `errno` read on any path reachable after `read`, `recv`, `write` or `send`
- [ ] The return value alone drives the decision: `> 0` data, `== 0` peer closed, `-1` remove the client
- [ ] On `-1` the client is closed and removed from the poll set — since sockets are non-blocking and registered in `poll()`, treating `-1` as fatal is the compliant behaviour
- [ ] `grep -rn errno src/` shows no hit within an I/O result branch

---

## Issue 3 — `limit_except` is never parsed; per-route method limits have no effect

**Labels:** `compliance`, `bug`, `config`
**Severity:** High — fails a scale bullet outright

### Scale requirement

> Configure uma lista de métodos aceitos para uma rota específica (por exemplo, tente DELETE sem permissão).

### Evidence

`grep -rn "limit_except\|allowed_methods" src/ include/` returns **nothing**. `ConfigLoader` handles only `autoindex`, `root`, `index`, `client_max_body_size`, `cgi_pass` inside a `location` (`src/config/ConfigLoader.cpp:355-363`). `LocationConfig::allowedMethods` exists (`src/config/ServerConfig.cpp:16`) but is never written or read.

Reproduced with a config declaring `limit_except GET`:

```
server {
    listen 8090;
    root <dir>;
    location / { index index.html; limit_except GET; }
}
```

```
$ curl -s -o /dev/null -w "%{http_code}\n" -X DELETE http://localhost:8090/victim.txt
204
$ ls <dir>/victim.txt
No such file or directory
```

The file was deleted on a route that only permits `GET`. The `405` seen elsewhere comes from `Router::resolveHandler`'s hardcoded handler table, not from config.

### Acceptance criteria

- [ ] `ConfigLoader` parses `limit_except` into `LocationConfig::allowedMethods`
- [ ] `Router` rejects a method absent from the matched location with `405` and an `Allow` header
- [ ] An empty/absent `limit_except` keeps current behaviour (all implemented methods allowed)
- [ ] `DELETE` on a `limit_except GET` route returns `405` and leaves the file on disk

---

## Issue 4 — `return` directive is never parsed; redirects answer 404

**Labels:** `compliance`, `bug`, `config`
**Severity:** High

### Scale requirement

> Tente um URL redirecionado.

### Evidence

`conf/default.conf` declares:

```
location /redirect-target {
    return 302 /;
}
```

```
$ curl -s -i http://localhost:8080/redirect-target | head -1
HTTP/1.1 404 Not Found
```

`ServerConfig` carries `returnCode` / `returnUrl` (`src/config/ServerConfig.cpp:17`) but `ConfigLoader` never assigns them and no code reads them. The directive is silently dropped.

### Acceptance criteria

- [ ] `ConfigLoader` parses `return <code> <url>;` into `returnCode` / `returnUrl`
- [ ] A matched location with a return produces that status plus a `Location` header, before any filesystem lookup
- [ ] `curl -i http://localhost:8080/redirect-target` → `302` with `Location: /`
- [ ] `curl -L` follows through to the index page

---

## Issue 5 — POST ignores `upload_store` and overwrites any file under the document root

**Labels:** `compliance`, `bug`, `security`, `http`
**Severity:** High

### Scale requirement

> Requisições GET, POST e DELETE devem funcionar. […] Envie alguns arquivos para o servidor e recupere-os.
> Limite o tamanho do corpo da requisição do cliente (use: `curl -X POST -H "Content-Type: plain/text" -d "…"`).

### Evidence

`StaticFileHandler::handlePost` (`src/handlers/StaticFileHandler.cpp:484`) resolves the URI straight to a filesystem path and writes the body there via `saveFile`, with no reference to `upload_store`:

```cpp
std::string resolvedPath = rslv_req_realpath(request.getUri());
...
response.setStatusCode(saveFile(resolvedPath, request.getBody()));
```

**Symptom A — arbitrary overwrite.** A plain POST replaced the served homepage:

```
$ curl -s -o /dev/null -w "%{http_code}\n" -X POST -d "abc" http://localhost:8080/index.html
200
$ cat www/index.html
abc
```

`www/index.html` and `www/test.html` were both truncated to `abc`; restored with `git checkout`.

**Symptom B — the scale's own command fails.** `saveFile` returns `400` when the target is a directory (`src/handlers/StaticFileHandler.cpp:351`), so the exact command in the scale returns an error for an under-limit body:

```
$ curl -s -o /dev/null -w "%{http_code}\n" -X POST -H "Content-Type: plain/text" -d "SHORT" http://localhost:8080/
400
```

(The over-limit case correctly returns `413`.)

**Symptom C —** `grep -rn upload_store src/ include/` returns nothing; `LocationConfig::uploadStore` is never written or read.

### Acceptance criteria

- [ ] `ConfigLoader` parses `upload_store` into `LocationConfig::uploadStore`
- [ ] POST to a location with `upload_store` writes **into that directory**, never over the served tree
- [ ] POST to a location **without** `upload_store` does not create or truncate files — return `405`
- [ ] `curl -X POST -d "abc" http://localhost:8080/index.html` leaves `www/index.html` untouched
- [ ] `curl -X POST -H "Content-Type: plain/text" -d "SHORT" http://localhost:8080/` returns a non-error status, and the same command with an oversized body still returns `413`
- [ ] Upload then retrieve round-trips: POST a file, then GET it back

---

## Issue 6 — CGI script root is hardcoded to `cgi-bin`; the `location` root is ignored

**Labels:** `compliance`, `bug`, `cgi`
**Severity:** High

### Scale requirement

> O servidor deve funcionar corretamente com CGI. O CGI deve ser executado no diretório correto para acesso a arquivos de caminho relativo.

### Evidence

`CgiHandler` defaults `_cgiRoot` to the relative string `"cgi-bin"` (`src/cgi/CgiHandler.cpp:27`) and **`setCgiRoot()` is never called anywhere in `src/`** — `EventLoop::_cgiHandler` keeps the default forever. `CgiHandler::resolvePath` joins the URI onto that hardcoded root, so `location /cgi { root www/cgi; }` has no effect on where scripts are found.

Against the shipped `conf/default.conf`:

```
$ curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/cgi/echo.py
404
```

The same script resolves only through the hardcoded path:

```
$ curl -s -i http://localhost:8080/cgi-bin/echo.py | head -1
HTTP/1.1 200 OK
$ curl -s -i -X POST -d "name=42" http://localhost:8080/cgi-bin/post_echo.py | tail -1
name=42
```

Setting `root` to a valid absolute directory in a `location /cgi` block still 404s — the root is genuinely ignored, not merely misconfigured. Consequence: CGI only works when the server is launched from a working directory that happens to contain `cgi-bin/`, and the URI must start with `/cgi-bin`.

### Acceptance criteria

- [ ] The CGI root comes from the matched location's `root`, applied per request
- [ ] `location /cgi { root www/cgi; }` serves `/cgi/echo.py` from `www/cgi/`
- [ ] CGI still resolves correctly when the server is started from any working directory
- [ ] The child process `chdir`s to the script's directory so relative-path file access works (scale wording)
- [ ] Path traversal outside the CGI root stays blocked

---

## Issue 7 — `conf/default.conf` upload route is broken end-to-end

**Labels:** `compliance`, `bug`, `config`
**Severity:** High

### Scale requirement

> Envie alguns arquivos para o servidor e recupere-os.

### Evidence

`conf/default.conf`:

```
location /uploads {
    root www/;
    limit_except POST;
    autoindex off;
    upload_store www/uploads;
}
```

- `www/uploads/` **does not exist** in the repository (`ls www/` shows no `uploads`).
- Every upload attempt 404s:

```
$ curl -s -o /dev/null -w "%{http_code}\n" -X POST -H "Content-Type: text/plain" -d "hello" http://localhost:8080/uploads/f.txt
404
$ curl -s -o /dev/null -w "%{http_code}\n" -F "file=@up.txt" http://localhost:8080/uploads
404
$ curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/uploads/
404
```

- Even once Issue 5 is fixed, `limit_except POST` on this route forbids `GET`, so the "and retrieve them" half of the requirement cannot pass as configured.
- An empty `uploads/` directory exists at the repo root but is not what the config points at.

### Acceptance criteria

- [ ] `www/uploads/` exists in the repository (with a tracked `.gitkeep`) or is created at startup
- [ ] `limit_except` on the upload route permits `GET` alongside `POST` (and `DELETE` if intended)
- [ ] POST a file to `/uploads`, then GET it back → `200` with identical bytes
- [ ] Both raw-body and `multipart/form-data` uploads land in `upload_store`

---

## Issue 8 — A second `poll()` loop exists outside the main event loop

**Labels:** `compliance`, `refactor`, `cgi`
**Severity:** Medium

### Scale requirement

> Pergunte se eles usam apenas um select() (ou equivalente) […] O select() (ou equivalente) deve estar no loop principal.

### Evidence

`pumpCgiIo` in `src/cgi/CgiHandler.cpp:263` runs its own blocking loop:

```cpp
if (poll(fds, count, -1) == -1)
```

**This is not the runtime path** — the event loop drives CGI through `CgiProcess` fds registered in the main `poll()` (`src/server/EventLoop.cpp:648`), which is correct. `CgiHandler::execute()`, the only caller of `pumpCgiIo`, is referenced solely from `tests/`. But the function is compiled into the `webserv` binary, so an evaluator grepping for `poll(` finds two call sites — one of them blocking with an infinite timeout — and the burden falls on the team to explain it mid-defense.

### Acceptance criteria

- [ ] `pumpCgiIo` / `CgiHandler::execute()` are removed from the shipped binary, or moved behind a test-only translation unit
- [ ] Affected tests are ported onto the `CgiProcess` non-blocking API
- [ ] `grep -rn "poll(" src/` shows exactly one call site, in the main loop

---

## Issue 9 — Location matching ignores path boundaries

**Labels:** `bug`, `routing`
**Severity:** Medium

### Evidence

`Router::matchLocation` does a raw prefix compare with no separator check:

```cpp
if (uri.compare(0, locPath.size(), locPath) != 0)
    continue;
```

So `location /cgi` also matches `/cgi-bin/hello.php`, `/cgifoo`, `/cgi.txt`. Observed live: a request for `/cgi-bin/hello.php` picked up the `cgi_pass .php` binding from the `location /cgi` block in `conf/default.conf` and attempted to execute it. Likewise `location /uploads` would match `/uploadsomething`.

### Acceptance criteria

- [ ] A location prefix matches only on a `/` boundary or exact equality
- [ ] `/cgi-bin/x.php` no longer matches `location /cgi`
- [ ] Longest-prefix selection among genuinely matching locations is preserved
- [ ] Regression test covering `/cgi` vs `/cgi-bin` and `/uploads` vs `/uploadsomething`

---

## Issue 10 — `read`/`write` on file descriptors that never pass through `poll()`

**Labels:** `compliance`, `discussion`
**Severity:** Medium — depends on how strictly the evaluator reads the rule

### Scale requirement

> Escrever ou ler QUALQUER descritor de arquivo sem passar pelo select() (ou equivalente) é PROIBIDO.

### Evidence

| File:line | Operation |
|---|---|
| `src/handlers/StaticFileHandler.cpp:103` | `read(fd, …)` — static file being served |
| `src/handlers/StaticFileHandler.cpp:363` | `write(fd, …)` — `saveFile` |
| `src/server/Router.cpp:327` | `read(fd, …)` — error page file |

All three are regular files, opened, drained in a loop and closed without ever being registered in the poll set.

**Read this one carefully before acting.** The scale text is literal ("QUALQUER descritor"), but regular files are always poll-ready — `poll()` on them is a formality — and most accepted webserv implementations read static files this way. The risk is an evaluator applying the sentence to the letter.

### Acceptance criteria

- [ ] Team agrees a position and can defend it in one sentence, **or**
- [ ] Regular-file descriptors are registered in the main `poll()` set before being read/written
- [ ] Whichever path is chosen, no descriptor is read or written while blocking the event loop

---

## Issue 11 — Unsupported config directives are silently ignored

**Labels:** `enhancement`, `config`, `dx`
**Severity:** Medium

### Evidence

`ConfigLoader` recognises `listen`, `server_name`, `root`, `index`, `autoindex`, `client_max_body_size`, `error_page` at server level and `autoindex`, `root`, `index`, `client_max_body_size`, `cgi_pass` at location level (`src/config/ConfigLoader.cpp:278-363`). Anything else is dropped without a warning.

`conf/default.conf` ships three directives that fall through this gap — `limit_except`, `upload_store`, `return` (Issues 3, 4, 5) — and the server starts reporting success, so the configuration looks applied when it is not. This is what makes those three bugs invisible until a request is made.

### Acceptance criteria

- [ ] Unknown directives produce a startup error, or at minimum a `[Warning]` naming the directive and line
- [ ] Starting with `conf/default.conf` emits no unknown-directive diagnostics once Issues 3–5 land
- [ ] Config parse errors keep reporting the offending line number

---

## Issue 12 — `www/errors/404.html` is byte-identical to the built-in fallback

**Labels:** `compliance`, `test-gap`
**Severity:** Low

### Scale requirement

> Configure uma página de erro padrão (tente modificar a página de erro 404).

### Evidence

`www/errors/404.html` contains exactly the markup the server emits when no error page is configured, so a `404` looks identical whether or not `error_page` worked. During this audit the mechanism had to be verified with a separate sentinel file before it could be confirmed working — an evaluator running the obvious test learns nothing.

The feature itself is fine: with `error_page 404 /errs/my404.html` pointing at a distinct file, the custom body was served correctly.

### Acceptance criteria

- [ ] `www/errors/404.html` is visually and textually distinct from the built-in fallback
- [ ] Same for `www/errors/500.html`
- [ ] Requesting a missing path clearly shows the custom page

---

## Issue 13 — Idle and half-open connections are never reaped

**Labels:** `compliance`, `bug`, `networking`
**Severity:** Medium

### Scale requirement

> Verifique se não há conexões pendentes.

### Evidence

`Connection::last_activity()` (`src/network/Connection.cpp:136`) computes the idle time and `_time` is stamped on every recv, send and reset — but **the accessor has no callers**. `grep -n "last_activity" src/server/EventLoop.cpp` returns nothing. Nothing ever closes an idle connection.

Three clients opened and left hanging — one sending a partial request header with no terminating blank line, one sending nothing at all, one partial:

```
$ (printf 'GET / HTTP/1.1\r\nHost: x\r\n'; sleep 200) | nc localhost 8080 &
$ (sleep 200) | nc localhost 8080 &
$ (printf 'GET / HTTP/1.1\r\nHost: x\r\n'; sleep 200) | nc localhost 8080 &

t+15s  established=3   GET=200
t+45s  established=3   GET=200
t+75s  established=3   GET=200
t+105s established=3   GET=200
```

All three slots were still held after nearly two minutes. The server keeps serving other clients, so this is not an outage at this scale — but each stalled client permanently consumes a descriptor, and a slowloris-style client set would exhaust the table. An evaluator checking for "conexões pendentes" during the siege section will see them.

### Acceptance criteria

- [ ] The main loop sweeps connections each iteration and closes any whose `last_activity()` exceeds a configured idle timeout
- [ ] The `poll()` timeout accounts for the next connection deadline, so the sweep runs even with no traffic
- [ ] A client that opens a socket and sends nothing is closed within the timeout
- [ ] A client that sends a partial request header is closed within the timeout
- [ ] Keep-alive connections doing real work are never closed early — re-run `siege -b -c 25 -t60S` and confirm availability stays at 100%

---

## Reproduction environment

```
make && make                            # clean build, then confirms no relink
./webserv conf/default.conf             # :8080
./webserv conf/multiplename.conf        # :8088, virtual hosts
brew install siege
siege -b -c 25 -t120S http://localhost:8080/
leaks $(pgrep webserv)
```

Start the server detached from the shell running the tests (`nohup … &`, or a separate terminal). A server launched as a child of the test shell dies with it when that shell is interrupted, which looks exactly like a crash and is not one. `setsid` is not available on macOS.

Note: this repository is checked out under a path containing `#` (`milestone#5`). The config lexer treats `#` as a comment, so absolute paths containing it cannot be used inside a config file. That is standard nginx-style behaviour, not a defect — but it will bite anyone writing absolute roots on this machine.
