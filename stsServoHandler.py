import socket
from   select import select
import struct
import time
import pickle

# servo ports (arduino ports)
stsMetryPort = 1040
stsCmdPort   = 1041

# comm handler ports
recvCmdPort = 9105
sendCmdPort = 9106

# servo op codes
stopMovingOp   = 0x0801
startMovingOp  = 0x0802
doRotationsOp  = 0x0803
gotoPositionOP = 0x0804
setSpeedOp     = 0x0805

# station op code
servoMetry      = b'servoMetry'

isSim = False 
#isSim = True 

'''
messgae structure: [opCode, servoId, data0, data1, data2, data3]
where servoId is a place holder for multiple servos
example, start moving 4 rounds to the positive direction, speed 100, :
    [0x02, -1, 4, 100, 0, 0,0,0]
'''

if isSim:
    targetIp = ""
    stationIp   = "127.0.0.1"
else:
    targetIp = "10.10.0.31"
    stationIp   = "127.0.0.1"

servoSock = socket.socket(socket.AF_INET, # Internet
        socket.SOCK_DGRAM) # UDP
servoSock.bind(("", stsMetryPort))

commSock = socket.socket(socket.AF_INET, # Internet
        socket.SOCK_DGRAM) # UDP
commSock.bind(("", recvCmdPort))



if __name__ == '__main__':
    lastCmdMsgSentTic = time.time()
    lastMetryTic      = time.time()

    servoStatus = {}

    metryCnt = 0.0
    metryTic = time.time()


    while True:
        socks = select([servoSock, commSock], [], [], 0.001)[0]

        if len(socks) > 0:
            for s in socks:
                data, addr = s.recvfrom(64*1024)
                
                #if addr[0] == targetIp or socks[0].getsockname()[1] == stsMetryPort:
                if s.getsockname()[1] == stsMetryPort:
                    if len(data) == 18:
                        metryCnt += 1
                        data = struct.unpack('h'*9, data)
                        if data[7] > 0:
                            servoStatus['pos'] = data[0] #/11.375
                            servoStatus['speed'] = data[1]
                            servoStatus['load'] = data[2]
                            servoStatus['volt'] = data[3]/10
                            servoStatus['temper'] = data[4]
                            servoStatus['move'] = data[5]
                            servoStatus['current'] = data[6]/100
                            servoStatus['id'] = data[7]
                            servoStatus['rots'] = data[8]

                            msg = pickle.dumps([servoMetry, servoStatus])
                            commSock.sendto( msg, (stationIp, sendCmdPort) )
                            #print('-->', servoStatus)
                            lastMetryTic = time.time()
                elif s.getsockname()[1] ==  recvCmdPort:
                #elif addr[0] == stationIp or socks[0].getsockname()[1] == recvCmdPort::
                    print('bla', targetIp, stsCmdPort)
                    data = pickle.loads(data)
                    msg = struct.pack('h'*6, *data)
                    servoSock.sendto(msg, (targetIp, stsCmdPort))
                    lastCmdMsgSentTic = time.time()
                else:
                    print('--->>', addr[0], addr[1], s.getsockname())


        if time.time() - lastMetryTic > 3:
            servoStatus = {}


        if time.time() - metryTic > 3:
            mps = metryCnt/(time.time() - metryTic)
            print('-->', servoStatus)
            print('>> metry message rate: %0.2f'%mps)
            metryTic = time.time()
            metryCnt = 0.0


