#!/usr/bin/env python3
"""
UART Test Utility for Raspberry Pi
===================================

This script helps test UART communication with Vision Master E213.
"""

import serial
import time
import sys

UART_PORT = '/dev/ttyS0'
UART_BAUDRATE = 115200

def test_uart_loopback():
    """Test UART loopback (TX connected to RX)"""
    print("\n=== UART Loopback Test ===")
    print("Connect TX (Pin 8) to RX (Pin 10) for this test\n")
    
    try:
        ser = serial.Serial(UART_PORT, UART_BAUDRATE, timeout=2)
        print(f"✅ Opened {UART_PORT} at {UART_BAUDRATE} baud")
        
        test_message = b"Hello UART!"
        print(f"Sending: {test_message}")
        ser.write(test_message)
        ser.flush()
        
        time.sleep(0.1)
        
        if ser.in_waiting > 0:
            received = ser.read(ser.in_waiting)
            print(f"Received: {received}")
            
            if received == test_message:
                print("✅ Loopback test PASSED")
            else:
                print("❌ Loopback test FAILED - data mismatch")
        else:
            print("❌ Loopback test FAILED - no data received")
            print("   Check if TX is connected to RX")
        
        ser.close()
        return True
        
    except serial.SerialException as e:
        print(f"❌ UART error: {e}")
        return False

def test_uart_receive():
    """Test receiving data from Vision Master"""
    print("\n=== UART Receive Test ===")
    print("Listening for data from Vision Master E213...")
    print("Press Ctrl+C to stop\n")
    
    try:
        ser = serial.Serial(UART_PORT, UART_BAUDRATE, timeout=1)
        print(f"✅ Opened {UART_PORT} at {UART_BAUDRATE} baud")
        print("Waiting for data...\n")
        
        start_time = time.time()
        bytes_received = 0
        
        while True:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                bytes_received += len(data)
                
                print(f"[{time.time() - start_time:.2f}s] Received {len(data)} bytes:")
                print(f"  Hex: {data.hex()}")
                print(f"  ASCII: {data}")
                print()
            
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print(f"\n\nStopped. Received {bytes_received} bytes total")
        ser.close()
    except serial.SerialException as e:
        print(f"❌ UART error: {e}")
        return False

def check_uart_availability():
    """Check if UART port is available"""
    print("\n=== Checking UART Availability ===")
    
    import os
    
    ports = ['/dev/ttyS0', '/dev/ttyAMA0', '/dev/serial0']
    
    for port in ports:
        if os.path.exists(port):
            print(f"✅ Found: {port}")
            
            # Try to open
            try:
                ser = serial.Serial(port, UART_BAUDRATE, timeout=1)
                print(f"   Can open: Yes")
                ser.close()
            except:
                print(f"   Can open: No (check permissions)")
        else:
            print(f"❌ Not found: {port}")

def main():
    print("="*60)
    print("Raspberry Pi UART Test Utility")
    print("="*60)
    
    if len(sys.argv) < 2:
        print("\nUsage:")
        print("  python3 test_uart.py check      - Check UART availability")
        print("  python3 test_uart.py loopback   - Test loopback (TX→RX)")
        print("  python3 test_uart.py receive    - Receive data from Vision Master")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == 'check':
        check_uart_availability()
    elif command == 'loopback':
        test_uart_loopback()
    elif command == 'receive':
        test_uart_receive()
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)

if __name__ == '__main__':
    main()
