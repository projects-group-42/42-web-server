import sys

sys.stdout.write("Status: 404 Not Found\r\n")
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("<h1>CGI 404</h1>")
