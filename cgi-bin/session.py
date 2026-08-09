#!/usr/bin/env python3
# Mostra a sessao que o servidor mantem para este cliente. O id vem do cookie
# SESSIONID que o servidor manda; o contador e do proprio servidor, entregue ao
# script pelo ambiente. Recarregue a pagina para ver o contador subir, e abra
# uma janela anonima para ver uma sessao nova comecar do zero.
import os
from html import escape

session = escape(os.environ.get("SESSION_ID", "<sem sessao>"))
visits = escape(os.environ.get("SESSION_VISITS", "0"))
cookie = escape(os.environ.get("HTTP_COOKIE", "<o navegador ainda nao devolveu o cookie>"))

print("Content-Type: text/html; charset=utf-8")
print()
print("<html><head><title>Sessao</title></head><body>")
print("<h1>Sessao</h1>")
print("<p>Id da sessao: <code>%s</code></p>" % session)
print("<p>Requisicoes nesta sessao: <strong>%s</strong></p>" % visits)
print("<p>Cookie recebido: <code>%s</code></p>" % cookie)
print("<p><a href=\"/cgi-bin/session.py\">Recarregar</a></p>")
print("</body></html>")
