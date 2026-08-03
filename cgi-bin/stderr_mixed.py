import sys

sys.stderr.write("log: starting\n")
sys.stderr.write("log: processing\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("X-Debug: on\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("result from stderr_mixed.py")
sys.stderr.write("log: done\n")
