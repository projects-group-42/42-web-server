*Este projeto foi criado como parte da 42 por jucoelho, dajesus-, galves-a.*

# webserv

## Descrição

`webserv` é um servidor HTTP/1.1 escrito do zero em **C++98**, sem bibliotecas externas, inspirado no modelo de configuração do nginx.

O servidor roda em um **único processo e uma única thread**, sobre um laço de eventos baseado em `poll()`. Sockets de escuta, sockets de clientes e os pipes dos processos CGI são multiplexados no mesmo laço, com I/O não-bloqueante, de modo que uma conexão lenta não bloqueia as demais.

O comportamento é definido inteiramente por um arquivo de configuração, que descreve um ou mais blocos `server` e, dentro deles, blocos `location` com regras por rota.

Recursos implementados:

- **HTTP/1.1** com `keep-alive`, requisições em `Content-Length` ou `Transfer-Encoding: chunked`.
- **Métodos** `GET`, `POST` e `DELETE`, restringíveis por rota com `limit_except` (responde `405` com o cabeçalho `Allow`).
- **Arquivos estáticos** com resolução de `root`/`index` e detecção de MIME type pela extensão.
- **Autoindex** — listagem de diretório quando `autoindex on` e não há arquivo de índice.
- **Upload de arquivos** via `POST` `multipart/form-data`, gravados no diretório indicado por `upload_store`.
- **DELETE** de arquivos servidos pelo servidor.
- **CGI** (`.py`, `.pl`, `.php`) sob `/cgi-bin/`, com `fork`/`execve`, corpo entregue por pipe e **timeout de 5 s** (responde `504` e mata o processo).
- **Servidores virtuais** — vários blocos `server` na mesma porta, escolhidos pelo cabeçalho `Host`; o primeiro bloco é o padrão.
- **Múltiplas portas** em um mesmo arquivo de configuração.
- **Redirecionamentos** com `return <código> <destino>`.
- **Páginas de erro customizadas** por `error_page`, com fallback interno.
- **Limite de corpo** por `client_max_body_size` (responde `413`).
- **Robustez** — entrada malformada responde `400` sem derrubar o processo; `SIGPIPE` é ignorado.

## Instruções

### Requisitos

- `make`
- Um compilador C++ compatível com `-std=c++98` (`c++` / `clang++` / `g++`)
- Opcional, apenas para os scripts CGI de exemplo: `python3`, `perl`, `php-cgi`

O projeto usa apenas a biblioteca padrão do C++ e chamadas POSIX. A compilação e os testes desta versão foram verificados em macOS (Darwin/arm64).

### Compilação

```sh
make
```

Gera o binário `webserv` na raiz do repositório. Flags usadas: `-std=c++98 -Wall -Wextra -Werror`.

Outros alvos:

| Alvo | O que faz |
|---|---|
| `make` / `make all` | Compila o servidor |
| `make clean` | Remove os objetos (`obj/`) |
| `make fclean` | Remove objetos, o binário e os binários de teste |
| `make re` | `fclean` + `all` |
| `make test` | Compila e roda toda a suíte de testes unitários |
| `make val` | Roda o servidor sob `valgrind --leak-check=full` |

### Execução

```sh
./webserv conf/default.conf
```

O servidor passa a escutar em `127.0.0.1:8080`. Encerre com `Ctrl+C`.

Sem argumento, o arquivo padrão é `conf/simple.conf`, que escuta na porta `8088`:

```sh
./webserv
```

Antes de testar a rota de upload de `conf/default.conf`, crie o diretório de destino:

```sh
mkdir -p www/uploads
```

### Arquivos de configuração disponíveis

| Arquivo | Porta | Para que serve |
|---|---|---|
| `conf/default.conf` | 8080 | Configuração completa: estáticos, autoindex, upload, CGI, redirect, páginas de erro |
| `conf/simple.conf` | 8088 | Configuração mínima (padrão quando nenhum argumento é passado) |
| `conf/multiplename.conf` | 8088 | Dois servidores virtuais na mesma porta (`portfolio.com` e `site-da-escola.com`) |
| `conf/inheritance.conf` | 8085 | Herança de `root` do bloco `server` para os `location` |
| `conf/location_root.conf` | 8082 | `root` sobrescrito dentro de um `location` |

### Diretivas suportadas

No bloco `server`: `listen`, `server_name`, `root`, `index`, `autoindex`, `client_max_body_size`, `error_page`.

No bloco `location`: `root`, `index`, `autoindex`, `client_max_body_size`, `limit_except`, `upload_store`, `cgi_pass`, `return`.

```nginx
server {
    listen localhost:8080;
    error_page 404 /errors/404.html;
    client_max_body_size 10M;

    location / {
        root www/;
        index index.html;
        limit_except GET POST DELETE;
        autoindex off;
    }
}
```

### Verificando com o navegador

Com `./webserv conf/default.conf` rodando, abra <http://localhost:8080/>.

### Verificando com curl

```sh
# página estática
curl -i http://localhost:8080/

# listagem de diretório (autoindex on)
curl -i http://localhost:8080/assets/

# redirecionamento 302
curl -i http://localhost:8080/redirect-target

# página de erro customizada
curl -i http://localhost:8080/rota-inexistente

# método não permitido -> 405 + Allow
curl -i -X PUT http://localhost:8080/

# corpo acima de client_max_body_size -> 413
head -c 12000000 /dev/zero | tr '\0' 'a' > /tmp/big.txt
curl -i -F "file=@/tmp/big.txt" http://localhost:8080/uploads

# CGI: GET, query string e POST
curl -i http://localhost:8080/cgi-bin/echo.py
curl -i "http://localhost:8080/cgi-bin/query_echo.py?a=1"
curl -i -X POST --data-binary 'ola' http://localhost:8080/cgi-bin/post_echo.py

# upload, download e remoção
mkdir -p www/uploads
echo 'conteudo' > /tmp/up.txt
curl -i -F "file=@/tmp/up.txt" http://localhost:8080/uploads   # 201
curl -i http://localhost:8080/uploads/up.txt                   # 200
curl -i -X DELETE http://localhost:8080/uploads/up.txt         # 204
```

Servidores virtuais, com `./webserv conf/multiplename.conf`:

```sh
curl -H 'Host: portfolio.com'        http://localhost:8088/
curl -H 'Host: site-da-escola.com'   http://localhost:8088/
```

### Testes automatizados

```sh
make test
```

Compila e executa as suítes de `tests/`, que cobrem o lexer e o parser da configuração, o `ServerConfig`, o carregamento do arquivo, o parser de requisições, o roteador, o autoindex, as páginas de erro, os MIME types, a seleção de host virtual, o parser multipart, o fluxo de upload e o CGI (handler, processo e suíte de integração).

### Estrutura do repositório

```
src/       implementação (config, network, server, http, handlers, cgi, utils)
include/   headers, espelhando a estrutura de src/
conf/      arquivos de configuração de exemplo
www/       raiz dos sites servidos, incluindo www/errors/
cgi-bin/   scripts CGI de exemplo (.py e .php)
tests/     testes unitários e de integração
docs/      guia de conceitos do projeto
```

## Recursos

Referências consultadas durante o desenvolvimento, e a parte do projeto em que cada uma foi usada:

| Recurso | Onde foi usado |
|---|---|
| [RFC 9112 — HTTP/1.1 Message Syntax and Routing](https://www.rfc-editor.org/rfc/rfc9112) | Formato da requisição e da resposta, `Content-Length`, `Transfer-Encoding: chunked` e `keep-alive` — `src/http/RequestParser.cpp` e `src/http/ResponseBuilder.cpp` |
| [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110) | Semântica dos métodos e escolha dos códigos de status (`201`, `204`, `302`, `400`, `405` + `Allow`, `413`, `504`) — `src/http/HttpResponse.cpp` e `src/server/Router.cpp` |
| [RFC 7578 — Returning Values from Forms: multipart/form-data](https://www.rfc-editor.org/rfc/rfc7578) | Delimitadores de `boundary` e cabeçalhos de cada parte no upload — `src/http/MultipartParser.cpp` |
| [RFC 3875 — The Common Gateway Interface (CGI) Version 1.1](https://www.rfc-editor.org/rfc/rfc3875) | Variáveis de ambiente (`REQUEST_METHOD`, `QUERY_STRING`, `PATH_INFO`, `CONTENT_LENGTH`) e leitura da resposta do script — `src/cgi/` |
| [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) | Criação e configuração dos sockets: `socket`, `setsockopt(SO_REUSEADDR)`, `bind`, `listen`, `accept` — `src/network/Socket.cpp` |
| Man pages `poll(2)`, `fcntl(2)`, `socket(2)`, `send(2)`, `recv(2)` | Laço de eventos não-bloqueante e tratamento de `POLLIN`/`POLLOUT`/`POLLHUP` — `src/server/EventLoop.cpp` e `src/network/Connection.cpp` |
| Man pages `fork(2)`, `execve(2)`, `pipe(2)`, `waitpid(2)`, `kill(2)` | Execução do interpretador CGI, pipes de entrada/saída e timeout do processo — `src/cgi/CgiProcess.cpp` e `src/cgi/CgiPipes.cpp` |
| [Documentação do nginx — `http` core module](https://nginx.org/en/docs/http/ngx_http_core_module.html) | Sintaxe e semântica das diretivas `server`, `location`, `listen`, `server_name`, `root`, `index`, `autoindex`, `error_page`, `client_max_body_size`, `limit_except`, `return` — `src/config/` |
| [MDN — HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP) | Referência rápida de cabeçalhos e tipos MIME — `src/http/MimeType.cpp` |
| [IANA Media Types](https://www.iana.org/assignments/media-types/media-types.xhtml) | Tabela de extensão para `Content-Type` — `src/http/MimeType.cpp` |
| `docs/webserv-guia-de-estudo.md` | Guia interno do grupo: divisão dos módulos, convenções de código e de commits, e fluxo de git |
