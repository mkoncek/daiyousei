#!/usr/bin/env python3

import socket
import sys 
import time
import os

socket_name = "./target/conn"
try:
	os.unlink(socket_name)
except:
	pass
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind((socket_name))
server.listen(1)
conn, addr = server.accept()

while True:
	data = conn.recv(1024)
	if data:
		print(data)
	else:
		print("done")
		break
