import sys

data = sys.stdin.read()
sys.stdout.write("Status: 201 Created\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("X-Cgi-Test: ok\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("cgi-body:" + data)
