#!/usr/bin/env python3
"""
LoRa UART Receiver for Raspberry Pi
====================================

This script receives LoRa packets from Vision Master E213 via UART
and forwards them to the Node.js backend server via HTTP POST.

Hardware Setup:
- Vision Master E213 connected to Raspberry Pi via UART
- Default: /dev/ttyS0 (GPIO 14 TX, GPIO 15 RX)
- Baud rate: 115200

Packet Format from Vision Master E213:
- [Device ID (10 bytes)] [Frame Counter (2 bytes)] [Port (1 byte)] [Data (n bytes)]
- RSSI and SNR appended by Vision Master

Author: Health Monitor System
Date: 2025
"""

import serial
import requests
import json
import base64
import time
import sys
import signal
from datetime import datetime

# Configuration
UART_PORT = '/dev/ttyS0'  # Change to /dev/ttyAMA0 if using Raspberry Pi 3/4
UART_BAUDRATE = 115200
UART_TIMEOUT = 1

# Backend server URL (running in Docker on same Raspberry Pi)
SERVER_URL = 'http://localhost:5000/api/sensor-data'

# Global variables
ser = None
running = True
stats = {
    'packets_received': 0,
    'packets_sent': 0,
    'errors': 0,
    'last_packet_time': None
}

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n\n⚠️  Shutting down gracefully...")
    running = False
    if ser and ser.is_open:
        ser.close()
    print("✅ UART port closed")
    print_statistics()
    sys.exit(0)

def print_statistics():
    """Print reception statistics"""
    print("\n" + "="*60)
    print("📊 RECEPTION STATISTICS")
    print("="*60)
    print(f"Packets Received: {stats['packets_received']}")
    print(f"Packets Sent to Server: {stats['packets_sent']}")
    print(f"Errors: {stats['errors']}")
    if stats['last_packet_time']:
        print(f"Last Packet: {stats['last_packet_time']}")
    print("="*60 + "\n")

def parse_packet(data):
    """
    Parse LoRa packet
    
    Packet format:
    [Device ID (10 bytes)] [Frame Counter (2 bytes)] [Port (1 byte)] [Payload (n bytes)]
    
    Returns dict with parsed data or None if invalid
    """
    if len(data) < 13:  # Minimum packet size
        print(f"❌ Packet too short: {len(data)} bytes")
        return None
    
    try:
        # Extract header
        device_id = data[0:10].decode('utf-8').rstrip('\x00')
        frame_counter = int.from_bytes(data[10:12], byteorder='little')
        port = data[12]
        payload = data[13:]
        
        packet_type_names = {1: "Realtime", 2: "ECG", 3: "Fall Event"}
        packet_type_name = packet_type_names.get(port, "Unknown")
        
        return {
            'device_id': device_id,
            'frame_counter': frame_counter,
            'packet_type': port,
            'packet_type_name': packet_type_name,
            'payload': payload,
            'payload_length': len(payload)
        }
    except Exception as e:
        print(f"❌ Error parsing packet: {e}")
        return None

def send_to_server(packet_info, rssi=-100, snr=0):
    """
    Send packet data to backend server via HTTP POST
    
    Args:
        packet_info: Parsed packet dictionary
        rssi: Received Signal Strength Indicator (dBm)
        snr: Signal-to-Noise Ratio (dB)
    """
    try:
        # Encode payload as base64
        payload_base64 = base64.b64encode(packet_info['payload']).decode('utf-8')
        
        # Create JSON payload matching the server's expected format
        json_data = {
            'device_id': packet_info['device_id'],
            'packet_type': packet_info['packet_type'],
            'data': payload_base64,
            'timestamp': datetime.now().isoformat(),
            'frame_counter': packet_info['frame_counter'],
            'rssi': rssi
        }
        
        # Send POST request
        response = requests.post(
            SERVER_URL,
            json=json_data,
            headers={'Content-Type': 'application/json'},
            timeout=5
        )
        
        if response.status_code == 200:
            print(f"   ✅ Sent to server (HTTP {response.status_code})")
            stats['packets_sent'] += 1
            return True
        else:
            print(f"   ❌ Server error: HTTP {response.status_code}")
            print(f"      Response: {response.text}")
            stats['errors'] += 1
            return False
            
    except requests.exceptions.Timeout:
        print("   ❌ Server timeout")
        stats['errors'] += 1
        return False
    except requests.exceptions.ConnectionError:
        print("   ❌ Cannot connect to server")
        stats['errors'] += 1
        return False
    except Exception as e:
        print(f"   ❌ Error sending to server: {e}")
        stats['errors'] += 1
        return False

def read_lora_packets():
    """
    Main loop to read LoRa packets from UART
    
    Expected format from Vision Master E213:
    - Line-based protocol with RSSI/SNR info
    - Packet data in hexadecimal or binary format
    """
    global ser, running
    
    print("\n" + "="*60)
    print("🎧 Listening for LoRa packets...")
    print("="*60)
    print(f"UART Port: {UART_PORT}")
    print(f"Baud Rate: {UART_BAUDRATE}")
    print(f"Server: {SERVER_URL}")
    print("="*60 + "\n")
    
    buffer = bytearray()
    
    while running:
        try:
            # Read available data
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                buffer.extend(data)
                
                # Process complete packets
                # Simple framing: packets start with device ID prefix
                while len(buffer) >= 13:
                    # Try to find packet start
                    # Device IDs typically start with "ESP32-"
                    if buffer[:5] == b'ESP32':
                        # Determine packet length based on type
                        if len(buffer) >= 13:
                            port = buffer[12]
                            
                            # Expected payload sizes
                            payload_sizes = {1: 10, 2: 65, 3: 45}
                            expected_size = 13 + payload_sizes.get(port, 0)
                            
                            if len(buffer) >= expected_size:
                                # Extract packet
                                packet_data = bytes(buffer[:expected_size])
                                buffer = buffer[expected_size:]
                                
                                # Parse and process packet
                                packet_info = parse_packet(packet_data)
                                if packet_info:
                                    stats['packets_received'] += 1
                                    stats['last_packet_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                                    
                                    print(f"\n📦 Packet #{stats['packets_received']}")
                                    print(f"   Device: {packet_info['device_id']}")
                                    print(f"   Type: {packet_info['packet_type_name']} (Port {packet_info['packet_type']})")
                                    print(f"   Frame: {packet_info['frame_counter']}")
                                    print(f"   Size: {packet_info['payload_length']} bytes")
                                    
                                    # Send to server (RSSI/SNR would come from Vision Master)
                                    send_to_server(packet_info)
                            else:
                                # Wait for more data
                                break
                    else:
                        # Remove invalid byte
                        buffer = buffer[1:]
                
                # Prevent buffer overflow
                if len(buffer) > 256:
                    print("⚠️  Buffer overflow, clearing...")
                    buffer = bytearray()
            
            else:
                # No data available, short sleep
                time.sleep(0.01)
                
        except serial.SerialException as e:
            print(f"❌ UART error: {e}")
            stats['errors'] += 1
            time.sleep(1)
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"❌ Unexpected error: {e}")
            stats['errors'] += 1
            time.sleep(1)

def main():
    """Main function"""
    global ser
    
    # Set up signal handler for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    print("\n" + "="*60)
    print("🚀 LoRa UART Receiver Starting")
    print("="*60)
    
    # Open UART port
    try:
        ser = serial.Serial(
            port=UART_PORT,
            baudrate=UART_BAUDRATE,
            timeout=UART_TIMEOUT,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
        
        print(f"✅ UART port opened: {UART_PORT} @ {UART_BAUDRATE} baud")
        
        # Clear any existing data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
    except serial.SerialException as e:
        print(f"❌ Failed to open UART port: {e}")
        print("\nTroubleshooting:")
        print("1. Check if port exists: ls -l /dev/tty*")
        print("2. Check permissions: sudo usermod -a -G dialout $USER")
        print("3. Enable UART in raspi-config")
        print("4. Reboot after changes")
        sys.exit(1)
    
    # Test server connection
    try:
        print(f"\n🔗 Testing server connection...")
        response = requests.get(SERVER_URL.replace('/api/sensor-data', '/health'), timeout=3)
        if response.status_code == 200:
            print(f"✅ Server is reachable")
        else:
            print(f"⚠️  Server responded with HTTP {response.status_code}")
    except:
        print(f"❌ Cannot reach server at {SERVER_URL}")
        print("   Make sure Docker containers are running: docker ps")
    
    # Start reading packets
    read_lora_packets()
    
    # Cleanup
    if ser and ser.is_open:
        ser.close()
    print_statistics()

if __name__ == '__main__':
    main()
