import sys

data = sys.stdin.read()
content_len = str(len(data))
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: " + content_len + "\r\n")
sys.stdout.write("X-Body-Size: " + content_len + "\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(data)
