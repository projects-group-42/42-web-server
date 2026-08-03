import sys

body = "x" * 65536
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: " + str(len(body)) + "\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)
