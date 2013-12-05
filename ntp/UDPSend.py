import socket

UDP_IP = "127.0.0.1"
UDP_PORT = 5555
MESSAGE = "Hello UDP!"

print "UDP Target IP:", UDP_PORT
print "UDP target port:", UDP_PORT
print "message:",MESSAGE

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.sendto(MESSAGE, (UDP_IP, UDP_PORT));
