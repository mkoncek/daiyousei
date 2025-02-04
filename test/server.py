#!/usr/bin/env python3

import socket
import sys 
import time
import os
import subprocess
from threading import Thread

import unittest

socket_name = "./target/conn"

def consume(connection):
	data = bytes()
	while True:
		try:
			data += connection.recv(512, socket.MSG_DONTWAIT)
		except BlockingIOError:
			return data

def setup():
	try:
		os.unlink(socket_name)
	except:
		pass
	server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
	server.bind((socket_name))
	server.listen(True)
	return server

def run_client():
	return subprocess.Popen(["./target/bin/daiyousei", "--unix-socket", socket_name],
		stdin = subprocess.PIPE,
		stdout = subprocess.PIPE,
		stderr = subprocess.PIPE,
	)

class Test_Simple(unittest.TestCase):
	def test_exit_immediately_zero(self):
		with setup() as server, run_client() as client, server.accept()[0] as conn:
			consume(conn)
			conn.send(b"l8:exitcodei0ee")
			self.assertEqual(0, client.wait())
	
	def test_exit_immediately_nonzero(self):
		with setup() as server, run_client() as client, server.accept()[0] as conn:
			consume(conn)
			conn.send(b"l8:exitcodei66ee")
			self.assertEqual(66, client.wait())
	
	def test_passing_stdin(self):
		with setup() as server, run_client() as client, server.accept()[0] as conn:
			consume(conn)
			client.stdin.write(b"some input")
			client.stdin.flush()
			data = conn.recv(512)
			self.assertEqual(b"5:stdin10:some input", data)
			conn.send(b"l8:exitcodei0ee")
			self.assertEqual(0, client.wait())
	
	def test_passing_stdout(self):
		with setup() as server, run_client() as client, server.accept()[0] as conn:
			consume(conn)
			conn.send(b"l6:stdout11:some output8:exitcodei0ee")
			stdout = client.stdout.read()
			self.assertEqual(b"some output", stdout)
			self.assertEqual(0, client.wait())
	
	def test_passing_stderr(self):
		with setup() as server, run_client() as client, server.accept()[0] as conn:
			consume(conn)
			conn.send(b"l6:stderr10:some error8:exitcodei0ee")
			stderr = client.stderr.read()
			self.assertEqual(b"some error", stderr)
			self.assertEqual(0, client.wait())

try:
	unittest.main()
finally:
	try:
		os.unlink(socket_name)
	except:
		pass
