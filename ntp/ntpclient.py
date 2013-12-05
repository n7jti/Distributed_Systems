import time
import socket
import select
import csv
from collections import deque

def filter(theta, rtt, lastEight = deque()):
    lastEight.append([theta, rtt])
    while len(lastEight) > 8:
        lastEight.popleft()
    return sorted(lastEight, key=lambda foo: foo[1])[0][0]

sent = deque();

#open our two log files
successLog = open('success.csv', 'wb')
failureLog = open('failure.csv', 'wb')

csvSuccessLog = csv.writer(successLog)
csvSuccessLog.writerow(["seq", "t3", "t2", "t1", "t0", "rtt", "theta", "smoothed theta","t'"])

csvFailureLog = csv.writer(failureLog)
csvFailureLog.writerow(["Dropped Sequence Numbers"])

FUTUREPROOF_IP = socket.gethostbyname("futureproof.cs.washington.edu")
print "FUTUREPROOF_IP:", FUTUREPROOF_IP

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((socket.gethostname(), 0))

# run for 1 minute
startTime = time.time()
endTime = startTime + 60 * 60 * 12
sequenceNumber = 1

while time.time() < endTime:
    # Send a packet
    lastOutPacket = time.time();
    outData = "%d %.6f" % (sequenceNumber , lastOutPacket)
    print "sending data:", outData
    sock.sendto(outData, (FUTUREPROOF_IP, 5555))
    sent.append(sequenceNumber)
    sequenceNumber = sequenceNumber + 1

    # wait for data to come back
    while lastOutPacket + 10 > time.time():
        #find the dead packets. they are the one with sequence numbers more than 2 minutes (or 12 indexes) ago
        while len(sent) > 0 and sent[0] < (sequenceNumber - 12):
            dropped = sent.popleft()
            print "dropped sequence:", dropped
            csvFailureLog.writerow([dropped])

        readList, writeList, errList = select.select([sock],[],[], lastOutPacket + 10 - time.time())
        if readList:
            recvTime = time.time()
            inData, addr = sock.recvfrom(1024)
            print "received message:", inData, "%.6f" % recvTime

            seq, t3, t2, t1 = inData.split();
            seq = int(seq)
            t3 = float(t3)
            t2 = float(t2)
            t1 = float(t1)
            t0 = recvTime

            sent.remove(seq)

            rtt = (t2-t3) + (t0 - t1)
            theta = ((t2-t3) - (t0 - t1)) / 2.0
            
            print "seq:", seq, "t3:", t3, "t2:", t2, "t1:", t1, "t0:", t0
            print "rtt:", rtt, "theta:", theta 

            smooththeta = filter(theta,rtt)
            tprime= t0 + smooththeta
            print "Smoothed Theta:", smooththeta , "t':", tprime
            print ""

            csvSuccessLog.writerow([seq, t3, t2, t1, t0, rtt, theta, smooththeta, tprime])

while len(sent) > 0:
    dropped = sent.popleft()
    print "droppedSequence:", dropped
    csvFailureLog.writerow(dropped)



