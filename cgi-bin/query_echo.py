import sys
import os

qs = os.environ.get("QUERY_STRING", "")
method = os.environ.get("REQUEST_METHOD", "GET")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(method + ":" + qs)
