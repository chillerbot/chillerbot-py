#!/usr/bin/env python3

import ctypes

lib = ctypes.cdll.LoadLibrary('./libtwnetwork.so')
class NETADDR(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("ip", ctypes.c_char * 16),
        ("port", ctypes.c_short),
        ("reserved", ctypes.c_short)
    ]

class CNetPacketConstruct(ctypes.Structure):
    _fields_ = [
        ("token", ctypes.c_uint),
        ("response_token", ctypes.c_uint),
        ("flags", ctypes.c_int),
        ("ack", ctypes.c_int),
        ("num_chunks", ctypes.c_int),
        ("data_size", ctypes.c_int),
        ("chunk_data", ctypes.c_ubyte * 1391)
    ]

addr_str = ctypes.create_string_buffer(b"127.0.0.1")
lib.Connect(ctypes.pointer(addr_str), 8303)

lib.SendSample()

while True:
    lib.PumpNetwork()
