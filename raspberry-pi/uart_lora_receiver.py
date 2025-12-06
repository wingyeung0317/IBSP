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
SERVER_URL = 'http://192.168.1.137:5000/api/sensor-data'

# Global variables
ser = None
running = True
stats = {
    'packets_received': 0,
    'packets_sent': 0,
    'errors': 0,
    'last_packet_time': None,
    'time_syncs_sent': 0
}

def send_time_sync():
    """Send current time to Vision Master E213 (sent with every packet)"""
    global ser, stats
    
    if not ser or not ser.is_open:
        return
    
    try:
        now = datetime.now()
        # Time sync format: [0xFF][0xFE][YY][YY][MM][DD][HH][MM][SS][0xFD]
        time_packet = bytearray([
            0xFF, 0xFE,
            (now.year >> 8) & 0xFF, now.year & 0xFF,
            now.month, now.day,
            now.hour, now.minute, now.second,
            0xFD
        ])
        
        ser.write(time_packet)
        stats['time_syncs_sent'] += 1
        print(f"⏰ Time sync sent: {now.strftime('%Y-%m-%d %H:%M:%S')}")
        
    except Exception as e:
        print(f"❌ Error sending time sync: {e}")

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
        # Device ID may not be valid UTF-8, try to decode, fallback to hex
        try:
            # Decode and remove all null bytes (not just trailing ones)
            device_id = data[0:10].decode('utf-8', errors='ignore').replace('\x00', '').strip()
            if not device_id:  # If empty after removing nulls, use hex
                device_id = data[0:10].hex()
        except UnicodeDecodeError:
            # If not valid UTF-8, use hex representation
            device_id = data[0:10].hex()
            print(f"⚠️  Device ID not UTF-8, using hex: {device_id}")
        
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
        print(f"   Raw data (first 20 bytes): {data[:20].hex()}")
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
    consecutive_failures = 0
    MAX_CONSECUTIVE_FAILURES = 10
    
    while running:
        try:
            # Check for incoming data
            if ser and ser.is_open and ser.in_waiting > 0:
                first_byte = ser.read(1)
                
                if first_byte == b'\xAA':
                    # Start of LoRa packet frame
                    # Format: [0xAA][LEN][RSSI][SNR][DATA...][0x55]
                    
                    # Read header bytes with short timeout
                    header_data = ser.read(3) if ser.in_waiting >= 3 else b''
                    
                    # If not enough bytes yet, wait briefly
                    if len(header_data) < 3:
                        time.sleep(0.05)
                        header_data += ser.read(3 - len(header_data))
                    
                    if len(header_data) == 3:
                        length = header_data[0]
                        rssi_encoded = header_data[1]
                        snr_encoded = header_data[2]
                        
                        # Validate length (reasonable packet size)
                        if length < 13 or length > 255:
                            print(f"⚠️  Invalid packet length: {length}")
                            consecutive_failures += 1
                            continue
                        
                        # Wait for complete packet data + end marker
                        bytes_needed = length + 1
                        packet_data = b''
                        timeout_counter = 0
                        
                        while len(packet_data) < bytes_needed and timeout_counter < 300:
                            available = ser.in_waiting
                            if available > 0:
                                chunk = ser.read(min(available, bytes_needed - len(packet_data)))
                                packet_data += chunk
                            else:
                                time.sleep(0.01)
                                timeout_counter += 1
                        
                        if len(packet_data) == bytes_needed:
                            # Split data and end marker
                            data_bytes = packet_data[:-1]
                            end_marker = packet_data[-1:]
                            
                            if end_marker == b'\x55' and len(data_bytes) == length:
                                # Decode RSSI/SNR
                                rssi = rssi_encoded - 150
                                snr = snr_encoded - 20
                                
                                # Reset failure counter on success
                                consecutive_failures = 0
                                
                                # Parse packet
                                packet_info = parse_packet(data_bytes)
                                if packet_info:
                                    stats['packets_received'] += 1
                                    stats['last_packet_time'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                                    
                                    print(f"\n📦 Packet #{stats['packets_received']}")
                                    print(f"   Device: {packet_info['device_id']}")
                                    print(f"   Type: {packet_info['packet_type_name']} (Port {packet_info['packet_type']})")
                                    print(f"   Frame: {packet_info['frame_counter']}")
                                    print(f"   RSSI: {rssi} dBm, SNR: {snr} dB")
                                    print(f"   Size: {packet_info['payload_length']} bytes")
                                    
                                    # Show payload hex for debugging
                                    if packet_info['packet_type'] == 3:
                                        print(f"   🚨 FALL EVENT DETECTED!")
                                        print(f"   Payload (hex): {packet_info['payload'].hex()}")
                                    
                                    # Add RSSI/SNR to packet info
                                    packet_info['rssi'] = rssi
                                    
                                    # Send to server
                                    send_to_server(packet_info)
                                    
                                    # Send time sync after each packet
                                    send_time_sync()
                            else:
                                print(f"⚠️  Invalid packet frame (end={end_marker.hex() if end_marker else 'empty'})")
                                consecutive_failures += 1
                        else:
                            print(f"⚠️  Timeout waiting for packet data (got {len(packet_data)}/{bytes_needed})")
                            consecutive_failures += 1
                    else:
                        print(f"⚠️  Could not read header (got {len(header_data)}/3 bytes)")
                        consecutive_failures += 1
                        
                elif first_byte == b'\x55':
                    # Orphaned end marker - we're out of sync
                    # This is normal on startup, ignore silently unless it persists
                    pass
                else:
                    # Unknown byte - could be noise or we're mid-packet
                    pass
                
                # If too many failures, clear buffer to resync
                if consecutive_failures >= MAX_CONSECUTIVE_FAILURES:
                    print("⚠️  Too many packet failures, resynchronizing...")
                    ser.reset_input_buffer()
                    consecutive_failures = 0
                    time.sleep(0.5)
            
            time.sleep(0.005)  # Reduced from 0.01 for faster response
            
        except serial.SerialException as e:
            print(f"❌ UART error: {e}")
            stats['errors'] += 1
            print("   🔄 Attempting to reconnect...")
            try:
                if ser and ser.is_open:
                    ser.close()
                ser = None
                time.sleep(2)
                ser = serial.Serial(
                    port=UART_PORT,
                    baudrate=UART_BAUDRATE,
                    timeout=UART_TIMEOUT,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE
                )
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                print("   ✅ Reconnected successfully")
            except Exception as reconnect_error:
                print(f"   ❌ Reconnection failed: {reconnect_error}")
                time.sleep(5)
        except KeyboardInterrupt:
            break
        except OSError as e:
            if e.errno == 5:  # Input/output error
                print(f"❌ I/O Error: {e}")
                print("   This usually means the UART device disconnected or has hardware issues")
                print("   🔄 Attempting to reconnect...")
                stats['errors'] += 1
                try:
                    if ser and ser.is_open:
                        ser.close()
                    ser = None
                    time.sleep(2)
                    ser = serial.Serial(
                        port=UART_PORT,
                        baudrate=UART_BAUDRATE,
                        timeout=UART_TIMEOUT,
                        bytesize=serial.EIGHTBITS,
                        parity=serial.PARITY_NONE,
                        stopbits=serial.STOPBITS_ONE
                    )
                    ser.reset_input_buffer()
                    ser.reset_output_buffer()
                    print("   ✅ Reconnected successfully")
                except Exception as reconnect_error:
                    print(f"   ❌ Reconnection failed: {reconnect_error}")
                    print("   Please check:")
                    print("   1. UART cable connections")
                    print("   2. Power to Vision Master E213")
                    print("   3. UART is enabled: sudo raspi-config")
                    time.sleep(5)
            else:
                print(f"❌ OS Error: {e}")
                stats['errors'] += 1
                time.sleep(1)
        except Exception as e:
            print(f"❌ Unexpected error: {e}")
            import traceback
            traceback.print_exc()
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
