"""
This file takes the V=3 reference file as input, then output the V=2 and V=1 ones
"""

import numpy as np
import struct
from struct import Struct
import sys

def convert_file(filename):
    V3data = []
    with open(filename, 'rb') as f:
        record_struct = Struct('<ff')
        chunks = iter(lambda: f.read(record_struct.size), b'')
        it = (record_struct.unpack(chunk) for chunk in chunks)
        for (I, Q) in it:
            V3data.append((I, Q))
    SLOT_SIZE = 320
    SINGLE_REF_LENGTH = SLOT_SIZE * 8
    with open(filename + ".V3", 'wb') as f:
        for i in range(len(V3data)):
            (I, Q) = V3data[i]
            f.write(struct.pack("<ff", I, Q))
    with open(filename + ".V2", 'wb') as f:
        for i in range(len(V3data)):
            if (i / SLOT_SIZE) % 8 < 4:
                (I, Q) = V3data[i]
                f.write(struct.pack("<ff", I, Q))
    with open(filename + ".V1", 'wb') as f:
        for i in range(len(V3data)):
            if (i / SLOT_SIZE) % 8 < 2:
                (I, Q) = V3data[i]
                f.write(struct.pack("<ff", I, Q))
    print("reference file has %d LCD's ref" % (len(V3data) / SINGLE_REF_LENGTH))

if __name__ == "__main__":
    assert len(sys.argv) == 2, "please input the V=3 reference file"
    convert_file(sys.argv[1])
