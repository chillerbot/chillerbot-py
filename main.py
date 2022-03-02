#!/usr/bin/env python3

import socket

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8303

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sock.sendto(b"foo", (SERVER_IP, SERVER_PORT))

