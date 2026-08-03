import sys

data = sys.stdin.read()
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("echo:" + data)
