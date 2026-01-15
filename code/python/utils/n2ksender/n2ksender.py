#!/usr/bin/env python3
"""
N2K Sender Utility - Version 1.0
Generate and transmit NMEA 2000 encoded messages via serial connection for SDK testing.
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import serial.tools.list_ports
import threading
import time
import random
import logging
from datetime import datetime
from typing import List, Optional
from dataclasses import dataclass


# Protocol constants
DLE = 0x10
STX = 0x02
ETX = 0x03

# Message types
MSG_TYPE_BST93 = 0x93
MSG_TYPE_BST94 = 0x94
MSG_TYPE_BSTD0 = 0xD0


@dataclass
class N2KMessage:
    """NMEA 2000 message structure"""
    pgn: int
    priority: int
    source: int
    destination: int
    data: bytes


class BDTPEncoder:
    """Encodes messages using BDTP protocol with DLE expansion"""
    
    @staticmethod
    def encode(data: bytes) -> bytes:
        """Encode data with BDTP framing (DLE STX ... DLE ETX)"""
        encoded = bytearray([DLE, STX])
        
        # Escape DLE characters in data
        for byte in data:
            encoded.append(byte)
            if byte == DLE:
                encoded.append(DLE)  # DLE stuffing
        
        encoded.extend([DLE, ETX])
        return bytes(encoded)
    
    @staticmethod
    def calculate_checksum(data: bytes) -> int:
        """Calculate BST checksum (sum of all bytes should equal 0)"""
        checksum = 0x00
        for byte in data:
            checksum = (checksum - byte) & 0xFF
        return checksum


class MessageGenerator:
    """Generates N2K messages in BST format"""
    
    @staticmethod
    def generate_bst93(pgn: int, data_length: int) -> bytes:
        """Generate BST 93 message (Gateway -> PC format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03
        
        priority = random.randint(0, 7)
        source = random.randint(0, 253)
        destination = 0xFF  # Broadcast
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF
        
        # Generate random data
        data = bytes([random.randint(0, 255) for _ in range(data_length)])
        
        # Build BST 93 message (without checksum first to calculate length)
        payload_length = 11 + data_length  # 11 header bytes + data (checksum byte is not part of length)
        
        message = bytearray([
            MSG_TYPE_BST93,
            payload_length,  # Length (excludes ID and length bytes, (checksum byte is not part of length))
            priority & 0x07,
            pdus,
            pduf,
            dp & 0x03,
            destination,
            source,
            timestamp & 0xFF,
            (timestamp >> 8) & 0xFF,
            (timestamp >> 16) & 0xFF,
            (timestamp >> 24) & 0xFF,
            data_length
        ])
        message.extend(data)
        
        # Add checksum
        checksum = BDTPEncoder.calculate_checksum(message)
        message.append(checksum)
        
        return bytes(message)
    
    @staticmethod
    def generate_bst94(pgn: int, data_length: int) -> bytes:
        """Generate BST 94 message (PC -> Gateway format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03
        
        priority = random.randint(0, 7)
        destination = 0xFF  # Broadcast
        
        # Generate random data
        data = bytes([random.randint(0, 255) for _ in range(data_length)])
        
        # Build BST 94 message (without checksum first to calculate length)
        payload_length = 6 + data_length  # 6 header bytes (priority, pdus, pduf, dp, destination, data_length) + data
        
        message = bytearray([
            MSG_TYPE_BST94,
            payload_length,  # Length (excludes ID and length bytes, checksum byte is not part of length)
            priority & 0x07,
            pdus,
            pduf,
            dp & 0x03,
            destination,
            data_length
        ])
        message.extend(data)
        
        # Add checksum
        checksum = BDTPEncoder.calculate_checksum(message)
        message.append(checksum)
        
        return bytes(message)
    
    @staticmethod
    def generate_bstd0(pgn: int, data_length: int) -> bytes:
        """Generate BST D0 message (modern format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03
        
        priority = random.randint(0, 7)
        source = random.randint(0, 253)
        destination = 0xFF  # Broadcast
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF
        
        # Generate random data
        data = bytes([random.randint(0, 255) for _ in range(data_length)])
        
        # BST Type 2 (D0) length includes the full 13-byte header (ID + L0 + L1 + 10 data header bytes)
        # Length = 13 header bytes + message data (checksum is not included)
        payload_length = 13 + len(data)
        
        # DPP field: Data Page and Priority
        dpp = (dp & 0x03) | ((priority & 0x07) << 2)
        
        # Control field: Message type (0=single packet), direction (0=received)
        control = 0x00  # Single packet, received, external source
        
        # Build BST D0 message
        message = bytearray([
            MSG_TYPE_BSTD0,
            payload_length & 0xFF,  # Length low byte
            (payload_length >> 8) & 0xFF,  # Length high byte
            destination,
            source,
            pdus,
            pduf,
            dpp,
            control,
            timestamp & 0xFF,
            (timestamp >> 8) & 0xFF,
            (timestamp >> 16) & 0xFF,
            (timestamp >> 24) & 0xFF,
        ])
        message.extend(data)
        
        # Add checksum
        checksum = BDTPEncoder.calculate_checksum(message)
        message.append(checksum)
        
        return bytes(message)


class N2KSenderGUI:
    """Main GUI application"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("N2K Sender Utility v1.0")
        self.root.geometry("700x600")
        
        # State variables
        self.serial_port: Optional[serial.Serial] = None
        self.is_sending = False
        self.send_thread: Optional[threading.Thread] = None
        
        # Available baud rates
        self.baud_rates = [4800, 9600, 19200, 38400, 57600, 115200, 230400]
        
        # Set up logging
        self.setup_logging()
        
        self.create_widgets()
        self.update_serial_ports()
        self.update_calculations()
    
    def setup_logging(self):
        """Set up file logging for sent messages"""
        # Create logger
        self.logger = logging.getLogger('n2ksender')
        self.logger.setLevel(logging.INFO)
        
        # Create file handler
        log_file = 'n2ksender.log'
        file_handler = logging.FileHandler(log_file, mode='a')
        file_handler.setLevel(logging.INFO)
        
        # Create formatter - raw output only
        formatter = logging.Formatter('%(message)s')
        file_handler.setFormatter(formatter)
        
        # Add handler to logger
        if not self.logger.handlers:
            self.logger.addHandler(file_handler)
        
    def create_widgets(self):
        """Create all GUI widgets"""
        # Main container
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Serial Connection Section
        conn_frame = ttk.LabelFrame(main_frame, text="Serial Connection", padding="5")
        conn_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(conn_frame, text="Port:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=15, state='readonly')
        self.port_combo.grid(row=0, column=1, padx=5)
        
        ttk.Button(conn_frame, text="Refresh", command=self.update_serial_ports).grid(row=0, column=2, padx=5)
        
        ttk.Label(conn_frame, text="Baud Rate:").grid(row=0, column=3, sticky=tk.W, padx=5)
        self.baud_var = tk.IntVar(value=115200)
        baud_combo = ttk.Combobox(conn_frame, textvariable=self.baud_var, values=self.baud_rates, 
                                   width=10, state='readonly')
        baud_combo.grid(row=0, column=4, padx=5)
        baud_combo.bind('<<ComboboxSelected>>', lambda e: self.update_calculations())
        
        # Bandwidth Section
        bw_frame = ttk.LabelFrame(main_frame, text="Bandwidth Control", padding="5")
        bw_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(bw_frame, text="Bandwidth %:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.bandwidth_var = tk.IntVar(value=50)
        bandwidth_spin = ttk.Spinbox(bw_frame, from_=0, to=100, textvariable=self.bandwidth_var, 
                                      width=10, command=self.update_calculations)
        bandwidth_spin.grid(row=0, column=1, padx=5)
        bandwidth_spin.bind('<KeyRelease>', lambda e: self.update_calculations())
        
        ttk.Label(bw_frame, text="Bytes/sec:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.bytes_per_sec_var = tk.StringVar(value="0")
        ttk.Label(bw_frame, textvariable=self.bytes_per_sec_var, width=10).grid(row=0, column=3, padx=5)
        
        ttk.Label(bw_frame, text="Frames/sec:").grid(row=0, column=4, sticky=tk.W, padx=5)
        self.frames_per_sec_var = tk.StringVar(value="0")
        ttk.Label(bw_frame, textvariable=self.frames_per_sec_var, width=10).grid(row=0, column=5, padx=5)
        
        # Message Configuration Section
        msg_frame = ttk.LabelFrame(main_frame, text="Message Configuration", padding="5")
        msg_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(msg_frame, text="Message Type:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.msg_type_var = tk.StringVar(value="BST 93")
        msg_type_combo = ttk.Combobox(msg_frame, textvariable=self.msg_type_var, 
                                       values=["BST 93", "BST 94", "BST D0"], 
                                       width=15, state='readonly')
        msg_type_combo.grid(row=0, column=1, padx=5)
        
        # Message Length Configuration
        ttk.Label(msg_frame, text="Variable Length:").grid(row=1, column=0, sticky=tk.W, padx=5)
        self.variable_length_var = tk.BooleanVar(value=True)
        var_check = ttk.Checkbutton(msg_frame, variable=self.variable_length_var, 
                                     command=self.toggle_length_control)
        var_check.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        ttk.Label(msg_frame, text="Fixed Length:").grid(row=1, column=2, sticky=tk.W, padx=5)
        self.fixed_length_var = tk.IntVar(value=8)
        self.fixed_length_spin = ttk.Spinbox(msg_frame, from_=5, to=100, 
                                              textvariable=self.fixed_length_var, 
                                              width=10, state='disabled',
                                              command=self.update_calculations)
        self.fixed_length_spin.grid(row=1, column=3, padx=5)
        self.fixed_length_spin.bind('<KeyRelease>', lambda e: self.update_calculations())
        
        # PGN List Section
        pgn_frame = ttk.LabelFrame(main_frame, text="PGN Numbers (comma or newline separated)", 
                                   padding="5")
        pgn_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.pgn_text = scrolledtext.ScrolledText(pgn_frame, width=60, height=8)
        self.pgn_text.grid(row=0, column=0, padx=5, pady=5)
        self.pgn_text.insert('1.0', "60928, 129025, 129029, 130312, 130316, 128267")
        
        # Control Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=4, column=0, columnspan=2, pady=10)
        
        self.send_btn = ttk.Button(btn_frame, text="Send Messages", command=self.start_sending)
        self.send_btn.grid(row=0, column=0, padx=5)
        
        self.stop_btn = ttk.Button(btn_frame, text="Stop", command=self.stop_sending, state='disabled')
        self.stop_btn.grid(row=0, column=1, padx=5)
        
        # Status Display
        status_frame = ttk.LabelFrame(main_frame, text="Status", padding="5")
        status_frame.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, width=60, height=10, state='disabled')
        self.status_text.grid(row=0, column=0, padx=5, pady=5)
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(3, weight=1)
        main_frame.rowconfigure(5, weight=1)
        
    def toggle_length_control(self):
        """Enable/disable fixed length control based on variable length checkbox"""
        if self.variable_length_var.get():
            self.fixed_length_spin.config(state='disabled')
        else:
            self.fixed_length_spin.config(state='normal')
        self.update_calculations()
        
    def update_serial_ports(self):
        """Update the list of available serial ports"""
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        self.port_combo['values'] = port_list
        if port_list:
            self.port_combo.current(0)
        self.log_status(f"Found {len(port_list)} serial port(s)")
        
    def update_calculations(self):
        """Update bandwidth and throughput calculations"""
        try:
            baud = self.baud_var.get()
            bandwidth_pct = self.bandwidth_var.get()
            
            # Calculate bytes per second (1 byte = 10 bits with start/stop bits)
            bytes_per_sec = int((baud / 10) * (bandwidth_pct / 100))
            self.bytes_per_sec_var.set(str(bytes_per_sec))
            
            # Estimate frames per second
            if bytes_per_sec > 0:
                # Estimate average frame size based on message type and length
                if self.variable_length_var.get():
                    avg_data_len = 25  # Average for variable length
                else:
                    avg_data_len = self.fixed_length_var.get()
                
                # Estimate total frame size with protocol overhead
                msg_type = self.msg_type_var.get()
                if msg_type == "BST 93":
                    frame_size = 14 + avg_data_len + 1  # Header + data + checksum
                elif msg_type == "BST 94":
                    frame_size = 8 + avg_data_len + 1  # Header + data + checksum
                else:  # BST D0
                    frame_size = 14 + avg_data_len + 1  # Header + data + checksum
                
                # Add BDTP overhead (4 bytes + DLE escaping ~10% average)
                frame_size = int(frame_size * 1.1 + 4)
                
                frames_per_sec = bytes_per_sec / frame_size
                self.frames_per_sec_var.set(f"{frames_per_sec:.1f}")
            else:
                self.frames_per_sec_var.set("0")
        except Exception as e:
            self.log_status(f"Calculation error: {e}")
    
    def parse_pgns(self) -> List[int]:
        """Parse PGN list from text input"""
        pgn_text = self.pgn_text.get('1.0', tk.END)
        pgns = []
        
        # Split by comma or newline
        parts = pgn_text.replace(',', '\n').split('\n')
        
        for part in parts:
            part = part.strip()
            if part:
                try:
                    pgn = int(part)
                    if 0 <= pgn <= 0x3FFFF:  # Valid PGN range
                        pgns.append(pgn)
                    else:
                        self.log_status(f"Warning: PGN {pgn} out of range (0-262143)")
                except ValueError:
                    self.log_status(f"Warning: Invalid PGN '{part}' - skipping")
        
        return pgns
    
    def log_status(self, message: str):
        """Add message to status log"""
        self.status_text.config(state='normal')
        self.status_text.insert(tk.END, f"{time.strftime('%H:%M:%S')} - {message}\n")
        self.status_text.see(tk.END)
        self.status_text.config(state='disabled')
        
    def start_sending(self):
        """Start sending messages"""
        if self.is_sending:
            return
            
        # Validate inputs
        if not self.port_var.get():
            self.log_status("Error: No serial port selected")
            return
            
        pgns = self.parse_pgns()
        if not pgns:
            self.log_status("Error: No valid PGNs entered")
            return
        
        # Open serial port
        try:
            self.serial_port = serial.Serial(
                port=self.port_var.get(),
                baudrate=self.baud_var.get(),
                timeout=1
            )
            self.log_status(f"Opened {self.port_var.get()} at {self.baud_var.get()} baud")
        except Exception as e:
            self.log_status(f"Error opening serial port: {e}")
            return
        
        # Update UI state
        self.is_sending = True
        self.send_btn.config(state='disabled')
        self.stop_btn.config(state='normal')
        
        # Start sending thread
        self.send_thread = threading.Thread(target=self.send_messages, args=(pgns,), daemon=True)
        self.send_thread.start()
        
    def stop_sending(self):
        """Stop sending messages"""
        self.is_sending = False
        self.send_btn.config(state='normal')
        self.stop_btn.config(state='disabled')
        self.log_status("Stopping transmission...")
        
    def send_messages(self, pgns: List[int]):
        """Send messages continuously (runs in separate thread)"""
        msg_type = self.msg_type_var.get()
        bytes_per_sec = int(self.bytes_per_sec_var.get())
        
        if bytes_per_sec == 0:
            self.log_status("Error: Bandwidth is 0%, cannot send messages")
            self.is_sending = False
            self.root.after(0, lambda: self.send_btn.config(state='normal'))
            self.root.after(0, lambda: self.stop_btn.config(state='disabled'))
            return
        
        self.log_status(f"Starting transmission: {len(pgns)} PGNs, Type: {msg_type}")
        
        message_count = 0
        start_time = time.time()
        
        try:
            while self.is_sending and self.serial_port and self.serial_port.is_open:
                for pgn in pgns:
                    if not self.is_sending:
                        break
                    
                    # Determine message length
                    if self.variable_length_var.get():
                        data_length = random.randint(5, 100)
                    else:
                        data_length = self.fixed_length_var.get()
                    
                    # Generate message based on type
                    if msg_type == "BST 93":
                        bst_message = MessageGenerator.generate_bst93(pgn, data_length)
                    elif msg_type == "BST 94":
                        bst_message = MessageGenerator.generate_bst94(pgn, data_length)
                    else:  # BST D0
                        bst_message = MessageGenerator.generate_bstd0(pgn, data_length)
                    
                    # Encode with BDTP
                    encoded_message = BDTPEncoder.encode(bst_message)
                    
                    # Send to serial port
                    self.serial_port.write(encoded_message)
                    
                    # Log sent bytes (raw hex for protocol analysis)
                    hex_bytes = ' '.join(f'{b:02X}' for b in encoded_message)
                    self.logger.info(hex_bytes)
                    
                    message_count += 1
                    
                    # Rate limiting based on bandwidth
                    bytes_sent = len(encoded_message)
                    delay = bytes_sent / bytes_per_sec if bytes_per_sec > 0 else 0
                    time.sleep(delay)
                    
                    # Log every 100 messages
                    if message_count % 100 == 0:
                        elapsed = time.time() - start_time
                        rate = message_count / elapsed if elapsed > 0 else 0
                        self.root.after(0, lambda: self.log_status(
                            f"Sent {message_count} messages ({rate:.1f} msg/sec)"))
        
        except Exception as e:
            self.root.after(0, lambda: self.log_status(f"Error during transmission: {e}"))
        
        finally:
            # Clean up
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
                self.log_status("Serial port closed")
            
            elapsed = time.time() - start_time
            rate = message_count / elapsed if elapsed > 0 else 0
            self.root.after(0, lambda: self.log_status(
                f"Transmission complete: {message_count} messages in {elapsed:.1f}s ({rate:.1f} msg/sec)"))
            
            self.is_sending = False
            self.root.after(0, lambda: self.send_btn.config(state='normal'))
            self.root.after(0, lambda: self.stop_btn.config(state='disabled'))


def main():
    """Main entry point"""
    root = tk.Tk()
    app = N2KSenderGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
