import sys
import argparse
import os

def parseFile(fname, s):
    f = open(fname)
    while True:
        chars = f.read(4)
        if len(chars) < 4:
            break;
        reply = f.read(int(chars))
        s.add(reply);


def main ():

    s=set();

    files = [f for f in os.listdir('.') if os.path.isfile(f) and f.endswith("_replyLog.log")]
    for f in files:
        parseFile(f,s);

    l=sorted(s);
    for f in l:
        print f


if __name__ == "__main__":
    sys.exit(main())