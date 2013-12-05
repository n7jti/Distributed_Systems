import time
import socket

UDP_IP = "127.0.0.1"
UDP_PORT = 5555

sockRecv = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sockRecv.bind((UDP_IP, UDP_PORT))

sockSend = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

while True:
    inData, addr = sockRecv.recvfrom(1024)
    recvTime = time.time();
    print "received message:", inData

    outData = inData + " %.3f %.3f" % (recvTime, time.time())
    sockSend.sendto(outData, addr);
    print "set message:", outData , addr
    


