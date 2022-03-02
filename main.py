#!/usr/bin/env python3

import socket

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8303

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

class CNetPacketConstruct():
    def __init__(self):
        self.token = 0
        self.response_token = 0
        self.flags = 0
        self.ack = 0
        self.num_chunks = 0
        self.data_size = 0
        self.chunk_data = b"foo"

def SendPacket(packet: CNetPacketConstruct):
    buf = packet.chunk_data
    sock.sendto(buf, (SERVER_IP, SERVER_PORT))


pck = CNetPacketConstruct()
SendPacket(pck)
