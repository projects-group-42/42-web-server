*Este projeto foi criado como parte do currículo da 42 por jucoelho, dajesus-, galves-a.*

# webserv

## Descrição

`webserv` é um servidor HTTP/1.1 escrito do zero em C++98, sem nenhuma biblioteca externa. Ele lê
um arquivo de configuração no estilo do bloco `server` do NGINX e, a partir dele, sobe os sockets,
resolve qual bloco atende cada requisição e responde.

O servidor é **single-threaded e não bloqueante**: uma única chamada de `poll()` no laço principal
cuida de tudo — sockets de escuta, leitura e escrita dos clientes e os pipes dos processos CGI. Não
há uma segunda thread nem uma segunda multiplexação; nenhuma requisição pode travar o servidor para
as outras.

O que ele faz:

- **Métodos** `GET`, `POST` e `DELETE`, com a lista de métodos aceitos configurável por rota.
- **Conteúdo estático** com arquivo padrão para diretório e listagem automática opcional.
- **Upload de arquivos**, tanto com corpo cru quanto `multipart/form-data`, para um diretório
  declarado na configuração.
- **CGI** escolhido pela extensão do arquivo (Python e Perl acompanham o repositório; qualquer
  interpretador pode ser ligado a uma extensão). Requisições em partes (`Transfer-Encoding:
  chunked`) são desagrupadas antes de chegar ao script, e a saída do script é aceita até o EOF
  quando ele não informa `Content-Length`.
- **Páginas de erro** próprias por código de status, com uma página embutida quando nenhuma é
  configurada.
- **Redirecionamento HTTP** por rota.
- **Limite de tamanho do corpo** da requisição, por servidor ou por rota.
- **Várias portas** em um mesmo processo e **hosts virtuais** pelo cabeçalho `Host`.
- **Conexões persistentes** (keep-alive) e requisições em fila (pipelining).

## Instruções

Requisitos: um compilador C++ com suporte a C++98 (`c++`/`g++`/`clang++`) e `make`. Python 3 e
Perl são opcionais, usados apenas pelos exemplos de CGI.

```sh
git clone <url-do-repositorio>
cd 42-web-server
make
```

O binário `webserv` é gerado na raiz do projeto.

```sh
./webserv conf/default.conf   # sobe em localhost:8080
./webserv                     # sem argumento, usa conf/simple.conf
```

O servidor precisa ser iniciado a partir da raiz do repositório, porque os caminhos das
configurações que acompanham o projeto são relativos a ela.

Regras do `Makefile`: `all` (padrão), `clean`, `fclean`, `re` e `test` (compila e roda a suíte de
testes unitários).

Depois de subir, dá para conferir com o navegador em `http://localhost:8080/` ou pelo terminal:

```sh
curl -i http://localhost:8080/                       # página inicial
curl -i http://localhost:8080/assets/                # listagem de diretório
curl -i http://localhost:8080/cgi-bin/echo.py        # CGI em Python
curl -i -X POST --data-binary @arquivo.txt \
     http://localhost:8080/uploads/arquivo.txt       # upload
curl -i http://localhost:8080/uploads/arquivo.txt    # recupera o que subiu
curl -i -X DELETE http://localhost:8080/uploads/arquivo.txt
```

Configurações de exemplo em `conf/`: `default.conf` (completa), `simple.conf` (mínima),
`multiplename.conf` (hosts virtuais), `inheritance.conf` e `location_root.conf`.

## Recursos

Referências usadas e em que parte do projeto:

- **RFC 9110 (HTTP Semantics)** e **RFC 9112 (HTTP/1.1)** — semântica dos métodos, códigos de
  status, `Allow`, `Content-Length` e enquadramento das mensagens. Base do analisador de requisição
  (`src/http/RequestParser.cpp`) e da montagem da resposta (`src/http/ResponseBuilder.cpp`).
- **RFC 3875 (The Common Gateway Interface, Version 1.1)** — variáveis de ambiente e formato da
  saída do CGI. Base de `src/cgi/CgiHandler.cpp`.
- **RFC 7578 (Returning Values from Forms: multipart/form-data)** — formato usado no upload por
  formulário (`src/http/MultipartParser.cpp`).
- **Documentação do NGINX**, seções `server`, `location`, `root`, `index`, `autoindex`,
  `error_page`, `client_max_body_size`, `limit_except` e `return` — modelo da gramática do arquivo
  de configuração e referência de comportamento para comparar respostas
  (`src/config/`, `src/server/Router.cpp`).
- **Beej's Guide to Network Programming** — sockets, `bind`/`listen`/`accept` e uso de descritores
  não bloqueantes (`src/network/Socket.cpp`).
- **`man` de `poll`, `fcntl`, `fork`, `execve`, `pipe`, `waitpid`, `stat`, `opendir`** — todas as
  chamadas de sistema usadas, principalmente no laço de eventos (`src/server/EventLoop.cpp`) e no
  CGI (`src/cgi/`).
- **`docs/webserv-guia-de-estudo.md`** — guia de estudo escrito pelo próprio grupo durante o
  projeto, usado para alinhar o entendimento de multiplexação de I/O e do fluxo de uma requisição.

### Uso de IA

Ferramentas de IA (Claude) foram usadas como apoio, sempre com o resultado revisado, testado e
discutido pelo grupo antes de entrar no repositório:

- **Auditoria de conformidade** contra o subject e a régua de avaliação, incluindo a execução dos
  testers oficiais e a redação dos relatórios `AUDIT-2026-08-08.md` e
  `SUBJECT-COMPLIANCE-FINDINGS.md`. Foi o uso mais extenso: levantar o que faltava e o que já
  passava, com evidência reproduzível para cada item.
- **Revisão de trechos específicos** de tratamento de erro de I/O — verificação do valor de retorno
  de `read`/`recv`/`write`/`send` no laço de eventos (`src/server/EventLoop.cpp`,
  `src/network/Connection.cpp`, `src/cgi/CgiProcess.cpp`).
- **Substituição de funções não autorizadas** pelo subject por equivalentes permitidos
  (`realpath` → verificação de contenção com `stat` em `src/utils/Utils.cpp`, `inet_pton` →
  leitura do literal IPv4 em `src/network/Socket.cpp`).
- **Discussão de casos de teste** para o analisador de requisição e para o upload — quais entradas
  malformadas exercitar e qual status cada uma deveria devolver.

A arquitetura do servidor, o modelo de laço de eventos, o analisador de configuração e a
implementação do CGI foram escritos e decididos pelo grupo.
