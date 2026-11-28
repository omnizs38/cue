import struct
d=open('/tmp/slowhd.state','rb').read()
last,key,end,ops,waited=struct.unpack('dqqqd',d[:40])
print(f"disk: {ops} operations, {waited:.0f} ms of disk time")
