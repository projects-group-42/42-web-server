# Evaluation compliance findings

Audit of this repository against the official webserv evaluation scale
(`Intra Projects webserv Edit.pdf`, scale_teams/9464839). Generated 2026-08-15
by reading the event loop, CGI, config, and HTTP/Makefile code paths, and
verifying the claims below against the built binary (curl, valgrind, siege)
rather than relying on static reading alone.

**Source note:** the PDF's checklist text is clipped mid-sentence at a fixed
container width (confirmed identically across `markitdown`, `pdftotext`,
`pdftotext -layout`, and `pdftotext -raw` — the truncation is baked into the
source PDF, not an extraction artifact). The visible fragments match the
standard 42 webserv evaluation template closely enough to be read with
confidence, and every finding below that matters was independently verified
against the actual running server rather than taken on the PDF text alone.

Legend: 🔴 real bug / eval risk · 🟡 latent risk (not currently triggered) · 🟢 cosmetic only

**Status as of 2026-08-15:** all six findings below (1 through 6) are fixed on
`development`, each on its own branch, merged after `make test` plus targeted
live verification (curl, valgrind, and — for finding 1 — a full siege run) on
every fix. See the ✅ **Fixed** line under each finding for the commit and
what was verified. The "what's confirmed compliant" and "not independently
verified" sections below are unchanged by these fixes and still reflect the
original audit.

---

## 🔴 1. Multi-interface config doesn't route by interface, only by port + Host

**Where:** `src/server/EventLoop.cpp` — `getServerConfigForRequest` (~line 1124-1146),
`Connection::getLocalPort()` (`src/network/Connection.cpp:299-307`)

The config format and socket binding correctly support listening on distinct
interfaces (`conf/default.conf:51,64` binds `127.0.0.1:8081` and `127.0.0.2:8081`
as two separate sockets). But at request-routing time, the server block is
picked using **only the port** — `Connection::getLocalPort()` calls
`getsockname()` and keeps just `ntohs(address.sin_port)`, discarding the local
IP entirely. There is no `getLocalHost()` anywhere in the codebase.

**Impact:** a client connecting directly to `127.0.0.2:8081` whose `Host:`
header doesn't literally match that site's `server_name` (e.g. a plain `curl
http://127.0.0.2:8081/`, which sends `Host: 127.0.0.2:8081`) falls through to
the *first* declared config for that port — i.e. site1's config — even though
it physically connected to site2's socket. This is exactly the scenario the
eval explicitly drives with a browser ("Configure vários sites em diferentes
interfaces e portas ... use o navegador para verificar se a configuração
funciona corretamente e serve o site apropriado").

**Fix direction:** track the local IP the connection was accepted on (already
available via `getsockname()`) and match it in `getServerConfigForRequest`
before falling back to Host-header/first-block logic.

**Empirically confirmed** (built the binary, ran `./webserv conf/default.conf`,
curled directly):
```
$ curl -s http://127.0.0.1:8081/ | head -3      # site1, as expected
<h1>Site 1</h1>
$ curl -s http://127.0.0.2:8081/ | head -3      # site2's own socket...
<h1>Site 1</h1>                                  # ...but serves site1's content
$ curl -s -H "Host: site2" http://127.0.0.1:8081/ | head -3   # only an explicit Host: fixes it
<h1>Site 2</h1>
```

✅ **Fixed** (`ce950a0`, merged `2f8e7c0`): added `Connection::getLocalHost()`
and matched server blocks in two tiers — exact local-address match first,
`0.0.0.0`/wildcard blocks only when no exact match exists for that port — so
a connection accepted on one interface is only ever answered by that
interface's own blocks. Re-ran the exact commands above post-fix: `127.0.0.2:8081`
now correctly returns `<h1>Site 2</h1>` in every case, including with a
`Host:` header naming the *other* site (the interface now wins). Virtual
hosting on a genuinely shared port (`conf/multiplename.conf`) verified
unaffected. Four new `host_selection_test` cases added covering this
directly; full suite green.

---

## 🔴 2. Unbounded CGI output can OOM-crash the whole server

**Where:** `src/cgi/CgiProcess.cpp:198-210` (`onReadable`, `_output.append(...)`),
`src/server/EventLoop.cpp:723` (`handleCgiIo` resets the deadline unconditionally)

`CgiProcess::onReadable` appends every chunk of CGI stdout to `_output` with no
size cap. `EventLoop::handleCgiIo` refreshes the 5s CGI deadline on *every*
call regardless of how much data has piled up, so a script that keeps
producing output indefinitely keeps resetting its own timeout while `_output`
grows without bound. The eventual allocation failure (`std::bad_alloc`, or an
OOM-kill from the Linux overcommit killer before that point is even reached)
is uncaught anywhere in the event loop and — in the `std::bad_alloc` case —
propagates to `main.cpp`'s top-level `catch`, which does `return 1`. Either
way the **entire process** dies, dropping every connection, not just the
offending one.

This directly violates "The server must never fail and an error must be
displayed if a problem occurs" for CGI. None of the shipped `cgi-bin/*.py`
scripts trigger it, so it won't surface in a casual defense, but an evaluator
who tries a deliberately pathological CGI script (which the subject explicitly
invites: "you can use a script containing an infinite loop or an error") could
hit it if the loop also writes output on each iteration.

**Fix direction:** cap `_output` size independently of the inactivity timeout;
answer 502/413 and kill the child once the cap is exceeded.

✅ **Fixed** (`74fe6ac`, merged `da13ae1`): `_output` is capped at 10MB;
crossing it drops the buffer, closes the read pipe, and has the event loop
kill the child outright (`finishCgiOverflow`, 502) instead of waiting for it
to finish. Verified live with a script that writes 64KB chunks forever: 502
in ~0.1s, the child is genuinely killed (confirmed absent from the process
table, not just poll-disabled), the server stays fully responsive to other
requests immediately after, and valgrind across two consecutive overflow
requests shows zero leaks.

---

## 🟡 3. Second `poll()` call outside the main loop (graceful shutdown drain)

**Where:** `src/server/EventLoop.cpp:1011` inside `drainPendingWrites()`, called
from `gracefulShutdown()` (`EventLoop.cpp:1036-1045`)

The main loop (`EventLoop::run`, `EventLoop.cpp:1061`) correctly has exactly
one `poll()` call multiplexing read and write together. However,
`drainPendingWrites()` — invoked once, after `while (g_running)` has already
exited, to flush pending client writes before shutting down — contains a
second, separate `poll()` call. It's architecturally a post-shutdown drain
(only polls `POLLOUT`, never accepts new work), not part of request serving,
but a grader who greps the whole codebase for `poll(` rather than reading only
the `while(g_running)` block could count this as a second multiplexer call
and flag it. Worth being ready to explain, or refactoring the drain to reuse
the main loop's `_fds`/dispatch structure.

✅ **Fixed** (`200a207`, merged `1ffcdfb`): `drainPendingWrites()` is gone.
`beginShutdownDrain()` now runs once, inline, the moment `g_running` goes
false — closes listeners, kills remaining CGI, marks every client closing,
arms `POLLOUT` only for those with data queued — and `run()`'s own loop keeps
polling the same `_fds` with a deadline-clamped timeout instead of a second
call. `grep -rn 'poll(' src/ include/` now shows exactly one real invocation
in the whole codebase. `shutdown_test`'s three drain-specific assertions
still pass; verified live that `SIGINT` sent mid-download of a rate-limited
transfer still delivers the file byte-identical before the process exits, and
valgrind across a request/CGI/upload sequence followed by `SIGINT` is clean.

---

## 🟡 4. Blocking `waitpid()` in `CgiProcess::reap()` could stall the event loop

**Where:** `src/cgi/CgiProcess.cpp:259` (`waitpid(_pid, &status, 0)`, no `WNOHANG`)

`reap()` is called once both CGI pipe directions report finished, which
normally coincides with the child actually exiting (the OS closes its fds on
exit), so the blocking wait returns immediately in practice. But a script that
explicitly closes/dups away stdout and stdin while continuing to run would
make `finished()` true while the child is still alive — and this single-
threaded event loop would then block in `waitpid` for as long as that child
keeps running, freezing every other connection. `CGI_TIMEOUT_MS` only guards
the read/write loop, not `reap()`. Not exercised by any shipped test script.

**Fix direction:** use `waitpid(_pid, &status, WNOHANG)` and only actually
reap once it returns non-zero; otherwise let the normal CGI timeout path kill
it.

✅ **Fixed** (`a6f7570`, merged `c4e542d`): `reap()` now tries
`waitpid(_pid, &status, WNOHANG)` first; if the child hasn't actually exited,
it's killed outright (`SIGKILL`, unblockable) rather than waited on, since a
script that closed its pipes without exiting is no longer honouring the CGI
contract by the time the server considers the exchange finished. Verified
live with a script that writes its response, closes both pipe ends, then
sleeps 5s: the request that used to stall the whole event loop for that long
now returns 502 in ~12ms, a concurrent request to a different route stays
immediately responsive, and no process is left behind. Normal CGI unaffected;
full suite and valgrind (this scenario plus a normal CGI request) both clean.

---

## 🔴 5. README first line is missing the required trailing period

**Where:** `README.md:1`

The eval's required format is: *"Este projeto foi criado como parte da 42 por
\<login1\>[, \<login2\>[, \<login3\>[...]]]."* (ends with a period). The
current first line is:

```
*Este projeto foi criado como parte da 42 por jucoelho, dajesus-, galves-a*
```

— no period before the closing `*`. Flagged 🔴 rather than cosmetic because
this checklist item is explicitly **zero-tolerance** in the subject ("Se
quaisquer dos elementos requeridos acima estiver faltando, a nota é 0") — the
fix costs one character but the downside of leaving it is the whole grade.

**Fix:** change to `*Este projeto foi criado como parte da 42 por jucoelho,
dajesus-, galves-a.*`

✅ **Fixed** (`d3cf34d`, merged `1191791`): trailing period added.

---

## 🟢 6. Dead line in Makefile

**Where:** `Makefile:2`

`SRC = $(shell find src -name "*.cpp")` runs a `find` shell-out on every
`make` invocation, but the result is immediately and unconditionally
overwritten by `SRC = $(addprefix src/, $(SRC_FILES))` at `Makefile:28`. Purely
wasteful, no functional effect — doesn't affect compilation, relinking, or the
42 norm's required `all`/`clean`/`fclean`/`re` targets (all present and
verified working, including correct incremental-rebuild behavior via
`-MMD -MP` dependency files).

✅ **Fixed** (`1f4279c`, merged `1db842f`): dead line removed. Verified
`all`/second-`make`-no-op/`re`/`clean`/`fclean` all still behave correctly.

---

## 🟢 Other latent notes (no action required, listed for completeness)

- `EventLoop.cpp:720-721` uses `operator[]` on `_pipeToClient`/`_cgi` maps
  instead of a checked `.find()`; currently safe because `releasePipeFd`/
  `releaseCgi` are always called together, but not defensively guarded.
- The per-request `try/catch` in `EventLoop.cpp:460-478` covers routing and
  response building, but not the entire `EventLoop::run()` loop — an exception
  thrown from poll-dispatch/accept/CGI bookkeeping code outside that block
  would propagate to `main()` and terminate the whole server rather than just
  the one connection. No such throwing code path was found in the areas
  reviewed (CGI code uses return codes, not exceptions).
- `CgiHandler::isCgiRequest` (`src/cgi/CgiHandler.cpp:165-168`) hardcodes
  `.py` and is dead code — actual CGI routing goes exclusively through
  `Router::resolveCgiInterpreter`, which is correctly config-driven per
  extension (`.py`/`.pl`/`.php`, verified live for all three, plus a custom
  extension in `conf/tester.conf`).
- `SCRIPT_FILENAME`/`PATH_TRANSLATED` are rewritten to a bare relative
  basename after the CGI `chdir()`, which deviates from RFC 3875's expectation
  of an absolute path — functionally correct for interpreters that resolve
  via cwd, just non-standard.
- `acceptClients()` drains the full `accept()` backlog in one `poll()`
  wake-up. This is accepting new connections, not reading/writing an
  established client, so it's outside the "one read/write per client per
  select()" rule as scoped, but a very literal grader might still ask about it.

---

## 🟢 Bonus: cookies/session and multiple CGI systems — both present and working

- **Cookies/session (bonus item 1):** `src/http/SessionStore.cpp` implements a
  real server-side session store (not just logic inside a CGI script) that
  issues a `SESSIONID` cookie and tracks a per-session visit counter, exposed
  to CGI via `SESSION_ID`/`SESSION_VISITS` env vars (`cgi-bin/session.py`
  demonstrates it). Verified live with a curl cookie jar: first request
  returns a fresh session id and `Requisições nesta sessão: 1`; two follow-up
  requests reusing the cookie return the *same* session id with the counter
  at 2 then 3.
- **Multiple CGI systems (bonus item 2):** already covered under compliant
  items below — `python3`/`perl`/`php-cgi` configured per-extension via
  `cgi_pass`, verified live for `.py` and `.php`.

---

## Empirically verified: leaks, stress, and process behavior

Ran the actual binary rather than relying on static reading for the parts of
the scale that require it:

- **Memory leaks** (`valgrind --leak-check=full --show-leak-kinds=all
  --track-origins=yes ./webserv conf/default.conf`), exercising static GET,
  multipart upload, GET of the uploaded file, DELETE, CGI GET, CGI POST, and a
  404, then a graceful `SIGINT` shutdown:
  ```
  HEAP SUMMARY:
      in use at exit: 0 bytes in 0 blocks
    total heap usage: 1,569 allocs, 1,569 frees, 518,482 bytes allocated
  All heap blocks were freed -- no leaks are possible
  ERROR SUMMARY: 0 errors from 0 contexts
  ```
  Clean. No leaks, no memcheck errors, across every code path exercised.

- **Siege stress test** (`siege -b -c 10 -t 15S http://127.0.0.1:8080/`):
  ```
  transactions:            225705
  availability:            100.00
  failed_transactions:          0
  ```
  100% availability (well above the required 99.5%), 0 failed transactions.
  Process RSS went from 3888 KB to 4004 KB over the run (~3% growth from
  allocator/connection-table churn, not an unbounded climb) — no indication of
  a leak under load, consistent with the valgrind result above. No lingering
  established connections found after the run.

## Not independently verified

Left unverified because they need either a real browser or a longer/more
adversarial session than this audit ran — call these out explicitly rather
than let their absence be read as a pass:

- **Browser-based checks** from the "Verificar com um navegador" section:
  network tab inspection of request/response headers, directory listing via
  browser, redirected URL behavior, and general "try anything you like"
  exploratory browsing.
- **Longer-duration siege runs** ("use siege indefinitely without restarting
  the server") — only a 15s/10-connection run was done here; a multi-minute
  run before the actual defense would give more confidence against slow leaks
  or fd exhaustion.
- **Multiple independent webserv processes started concurrently** with
  overlapping config ports — the code path for the single-process
  `EADDRINUSE` case was verified by reading (see compliant list below), but
  not exercised by literally starting two OS processes at once.

---

## What's confirmed compliant (verified, not assumed)

- Exactly one `poll()` in the main serving loop, multiplexing read and write
  together (see finding 3 for the one caveat).
- At most one `recv`/`send`/CGI-pipe `read`/`write` per client per poll
  wake-up — no draining loops.
- Every socket/pipe error path removes the client and frees resources.
- Return values of `recv`/`send`/pipe I/O explicitly check both `0` and `< 0`.
- `errno` is never used for control flow after `recv`/`send`; only for
  logging (confirmed by full-codebase grep).
- No fd — socket or CGI pipe — is ever read/written outside the `poll()`
  dispatch.
- `SIGPIPE` is ignored in the parent (`EventLoop.cpp:136`), so a write to an
  already-dead CGI child can't kill the server.
- GET/POST/DELETE all work end-to-end (live-tested), unknown methods return
  405/501 without crashing (live-tested with pipelined and raw-junk input).
- Status codes spot-checked against RFC 9110 semantics: 200/201/204/301/302/
  400/403/404/405/413/414/431/500/501/505 all used correctly; live-tested a
  dozen malformed-input cases (oversized headers, negative/overflowing
  Content-Length, bad chunk encoding, path traversal, malformed percent-
  encoding) — all handled with the correct status and no crash.
- Config supports: multiple ports and multiple listening sockets per
  interface, routed correctly by interface since finding 1's fix, custom
  error pages, `client_max_body_size` (enforced, 413), per-location root,
  per-location index, per-location `limit_except` (enforced, 405 + `Allow`
  header), Host-header virtual hosting on a shared port (deliberate,
  unit-tested design, not a crash).
- Only `SO_REUSEADDR` is set (not `SO_REUSEPORT`), so two independent webserv
  processes on the same interface:port fail one of them gracefully
  (`EADDRINUSE` caught, that listener skipped, warning logged) rather than
  producing undefined shared-port behavior.
- CGI: GET and POST both work; `chdir()` into the script's directory before
  `execve` with the interpreter path resolved to absolute beforehand (the
  recent `060dec7` fix); malformed/erroring/no-output CGI scripts are answered
  with 502/403/500 without crashing the server; three interpreters
  (`python3`/`perl`/`php-cgi`) configured via `cgi_pass` per extension,
  verified live; 5s inactivity timeout drives `poll()`'s own wait so hung
  scripts are killed without needing other traffic; every CGI teardown path
  reaps the child (no zombies).
- `make`/`make clean`/`make fclean`/`make re` all present and correct;
  verified real incremental rebuilds (second `make` = "Nothing to be done",
  header touch recompiles only dependents) via `-MMD -MP`.
