import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

data = bytes(i % 0xFF for i in range(0, 10))
data = b'cadebaba'
s.sendto(data, ("10.10.10.2", 2000))