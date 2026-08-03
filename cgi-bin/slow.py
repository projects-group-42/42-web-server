import sys
import time

time.sleep(2)
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("slow cgi done")
