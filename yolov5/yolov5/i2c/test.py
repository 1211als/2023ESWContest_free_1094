#! /usr/bin/python3
import smbus as sm
import time

address = 0x40
time.sleep(1)
bus = sm.SMBus(1)
time.sleep(1)

def writeNumber(value):
    bus.write_byte(address, value)
    return -1

def readNumber():
    number = bus.read_byte(address)
    return number

def send_data_via_i2c(data):
    bytes_data = data.encode('utf-8')
    
    if len(bytes_data) > 12:
        raise ValueError("Data length exceeds 12 bytes")

    bus.write_i2c_block_data(address, 0x00, list(bytes_data))

while True:
    data = "111111111111"
    send_data_via_i2c(data)
    print(data)
    time.sleep(0.1)

    
