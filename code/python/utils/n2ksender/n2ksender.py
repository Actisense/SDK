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
import configparser
import os
import json
from datetime import datetime
from typing import List, Optional, Dict, Any
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


class PGNEncoder:
    """Encodes field values into N2K message data using PGN definitions"""

    @staticmethod
    def encode_fields(fields: List[Dict[str, Any]], field_values: Dict[str, Any]) -> bytes:
        """
        Encode field values into binary data based on field definitions.

        Args:
            fields: List of field definitions from JSON (each with name, bitLength)
            field_values: Dictionary mapping field names to values

        Returns:
            Encoded binary data
        """
        # Calculate total bits needed
        total_bits = sum(field['bitLength'] for field in fields)
        total_bytes = (total_bits + 7) // 8  # Round up to nearest byte

        # Initialize data array
        data = bytearray(total_bytes)
        bit_offset = 0

        for field in fields:
            field_name = field['name']
            bit_length = field['bitLength']

            # Get field value (default to 0xFF... for reserved fields or if not provided)
            if field_name in field_values:
                value = field_values[field_name]
            elif 'Reserved' in field_name or 'NMEA Reserved' in field_name:
                value = (1 << bit_length) - 1  # All 1s for reserved
            else:
                value = 0  # Default to 0 if not provided

            # Pack value into data at bit_offset
            PGNEncoder._pack_bits(data, bit_offset, bit_length, value)
            bit_offset += bit_length

        return bytes(data)

    @staticmethod
    def _pack_bits(data: bytearray, bit_offset: int, bit_length: int, value: int):
        """Pack a value into the data array at the specified bit offset"""
        # Ensure value fits in bit_length
        max_value = (1 << bit_length) - 1
        value = value & max_value

        # Pack bits
        for i in range(bit_length):
            if value & (1 << i):
                byte_index = (bit_offset + i) // 8
                bit_index = (bit_offset + i) % 8
                data[byte_index] |= (1 << bit_index)


class MessageGenerator:
    """Generates N2K messages in BST format"""

    @staticmethod
    def generate_bst93(pgn: int, data_length: int, data: Optional[bytes] = None) -> bytes:
        """Generate BST 93 message (Gateway -> PC format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03

        priority = random.randint(0, 7)
        source = random.randint(0, 253)
        destination = 0xFF  # Broadcast
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF

        # Use provided data or generate random data
        if data is None:
            data = bytes([random.randint(0, 255) for _ in range(data_length)])
        else:
            data_length = len(data)
        
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
    def generate_bst94(pgn: int, data_length: int, data: Optional[bytes] = None) -> bytes:
        """Generate BST 94 message (PC -> Gateway format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03

        priority = random.randint(0, 7)
        destination = 0xFF  # Broadcast

        # Use provided data or generate random data
        if data is None:
            data = bytes([random.randint(0, 255) for _ in range(data_length)])
        else:
            data_length = len(data)
        
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
    def generate_bstd0(pgn: int, data_length: int, data: Optional[bytes] = None) -> bytes:
        """Generate BST D0 message (modern format) - returns unencoded BST frame"""
        # Extract PGN components
        pdus = pgn & 0xFF
        pduf = (pgn >> 8) & 0xFF
        dp = (pgn >> 16) & 0x03

        priority = random.randint(0, 7)
        source = random.randint(0, 253)
        destination = 0xFF  # Broadcast
        timestamp = int(time.time() * 1000) & 0xFFFFFFFF

        # Use provided data or generate random data
        if data is None:
            data = bytes([random.randint(0, 255) for _ in range(data_length)])
        else:
            data_length = len(data)
        
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
        self.root.geometry("800x800")

        # Settings file
        self.settings_file = "n2ksender.ini"

        # State variables
        self.serial_port: Optional[serial.Serial] = None
        self.is_sending = False
        self.send_thread: Optional[threading.Thread] = None

        # Available baud rates
        self.baud_rates = [4800, 9600, 19200, 38400, 57600, 115200, 230400]

        # Parametric PGN data
        self.pgn_definitions: Dict[int, Dict[str, Any]] = {}
        self.field_widgets: Dict[str, tk.Entry] = {}

        # Set up logging
        self.setup_logging()

        # Load PGN definitions from JSON
        self.load_pgn_definitions()

        self.create_widgets()
        self.load_settings()
        self.update_serial_ports()
        self.update_calculations()

        # Handle window close to save settings
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
    
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

    def load_pgn_definitions(self):
        """Load PGN definitions from n2k_pgns.json"""
        json_file = os.path.join(os.path.dirname(__file__), 'n2k_pgns.json')

        if not os.path.exists(json_file):
            print(f"Warning: {json_file} not found. Parametric PGN feature will be disabled.")
            return

        try:
            with open(json_file, 'r') as f:
                data = json.load(f)

            # Build dictionary of PGN definitions keyed by PGN number
            for pgn_def in data.get('pgns', []):
                pgn_num = pgn_def['pgn']
                self.pgn_definitions[pgn_num] = pgn_def

            print(f"Loaded {len(self.pgn_definitions)} PGN definitions from {json_file}")
        except Exception as e:
            print(f"Error loading PGN definitions: {e}")
    
    def load_settings(self):
        """Load settings from .ini file if it exists"""
        if not os.path.exists(self.settings_file):
            return
        
        try:
            config = configparser.ConfigParser()
            config.read(self.settings_file)
            
            # Load Serial Connection settings
            if config.has_section('SerialConnection'):
                if config.has_option('SerialConnection', 'port'):
                    self.port_var.set(config.get('SerialConnection', 'port'))
                if config.has_option('SerialConnection', 'baud_rate'):
                    self.baud_var.set(config.getint('SerialConnection', 'baud_rate'))
            
            # Load Bandwidth Control settings
            if config.has_section('BandwidthControl'):
                if config.has_option('BandwidthControl', 'bandwidth_percent'):
                    self.bandwidth_var.set(config.getint('BandwidthControl', 'bandwidth_percent'))
            
            # Load Message Configuration settings
            if config.has_section('MessageConfig'):
                if config.has_option('MessageConfig', 'message_type'):
                    self.msg_type_var.set(config.get('MessageConfig', 'message_type'))
                if config.has_option('MessageConfig', 'variable_length'):
                    self.variable_length_var.set(config.getboolean('MessageConfig', 'variable_length'))
                if config.has_option('MessageConfig', 'fixed_length'):
                    self.fixed_length_var.set(config.getint('MessageConfig', 'fixed_length'))
            
            # Load PGN list
            if config.has_section('PGNList'):
                if config.has_option('PGNList', 'pgns'):
                    pgn_text = config.get('PGNList', 'pgns')
                    self.pgn_text.delete('1.0', tk.END)
                    self.pgn_text.insert('1.0', pgn_text)

            # Load Parametric PGN settings
            if config.has_section('ParametricPGN') and self.pgn_definitions:
                if config.has_option('ParametricPGN', 'use_parametric'):
                    self.use_parametric_var.set(config.getboolean('ParametricPGN', 'use_parametric'))
                if config.has_option('ParametricPGN', 'selected_pgn'):
                    selected_pgn = config.getint('ParametricPGN', 'selected_pgn')
                    # Find and set the PGN in combo box
                    for idx, pgn in enumerate(sorted(self.pgn_definitions.keys())):
                        if pgn == selected_pgn:
                            self.parametric_pgn_combo.current(idx)
                            break
                # Trigger parametric mode setup if enabled
                if self.use_parametric_var.get():
                    self.toggle_parametric_mode()
                # Load field values
                if config.has_option('ParametricPGN', 'field_values'):
                    field_values_str = config.get('ParametricPGN', 'field_values')
                    try:
                        field_values = json.loads(field_values_str)
                        for field_name, value in field_values.items():
                            if field_name in self.field_widgets:
                                self.field_widgets[field_name].delete(0, tk.END)
                                self.field_widgets[field_name].insert(0, str(value))
                    except json.JSONDecodeError:
                        pass

            self.log_status(f"Settings loaded from {self.settings_file}")
        except Exception as e:
            self.log_status(f"Error loading settings: {e}")
    
    def on_setting_changed(self):
        """Called whenever any setting changes - triggers auto-save"""
        # Use after_cancel to debounce multiple rapid changes
        if hasattr(self, '_save_timer'):
            self.root.after_cancel(self._save_timer)
        self._save_timer = self.root.after(500, self.save_settings)
    
    def on_closing(self):
        """Handle window close event - save settings and exit"""
        if self.is_sending:
            self.stop_sending()
            # Give a brief moment for the send thread to stop
            self.root.after(100, self.on_closing)
            return
        
        self.save_settings()
        self.root.destroy()
    
    def save_settings(self):
        """Save current UI settings to .ini file"""
        try:
            config = configparser.ConfigParser()
            
            # Save Serial Connection settings
            config.add_section('SerialConnection')
            config.set('SerialConnection', 'port', self.port_var.get())
            config.set('SerialConnection', 'baud_rate', str(self.baud_var.get()))
            
            # Save Bandwidth Control settings
            config.add_section('BandwidthControl')
            config.set('BandwidthControl', 'bandwidth_percent', str(self.bandwidth_var.get()))
            
            # Save Message Configuration settings
            config.add_section('MessageConfig')
            config.set('MessageConfig', 'message_type', self.msg_type_var.get())
            config.set('MessageConfig', 'variable_length', str(self.variable_length_var.get()))
            config.set('MessageConfig', 'fixed_length', str(self.fixed_length_var.get()))
            
            # Save PGN list
            config.add_section('PGNList')
            pgn_text = self.pgn_text.get('1.0', tk.END).strip()
            config.set('PGNList', 'pgns', pgn_text)

            # Save Parametric PGN settings
            if self.pgn_definitions:
                config.add_section('ParametricPGN')
                config.set('ParametricPGN', 'use_parametric', str(self.use_parametric_var.get()))

                # Save selected PGN
                selection = self.parametric_pgn_combo.get()
                if selection:
                    try:
                        pgn_num = int(selection.split(' - ')[0])
                        config.set('ParametricPGN', 'selected_pgn', str(pgn_num))
                    except (ValueError, IndexError):
                        pass

                # Save field values
                field_values = {}
                for field_name, widget in self.field_widgets.items():
                    try:
                        value = int(widget.get())
                        field_values[field_name] = value
                    except ValueError:
                        field_values[field_name] = 0

                config.set('ParametricPGN', 'field_values', json.dumps(field_values))

            # Write to file
            with open(self.settings_file, 'w') as f:
                config.write(f)

            self.log_status(f"Settings saved to {self.settings_file}")
        except Exception as e:
            self.log_status(f"Error saving settings: {e}")
        
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
        self.baud_var.trace_add('write', lambda *args: self.on_setting_changed())
        baud_combo = ttk.Combobox(conn_frame, textvariable=self.baud_var, values=self.baud_rates, 
                                   width=10, state='readonly')
        baud_combo.grid(row=0, column=4, padx=5)
        baud_combo.bind('<<ComboboxSelected>>', lambda e: self.update_calculations())
        
        # Bandwidth Section
        bw_frame = ttk.LabelFrame(main_frame, text="Bandwidth Control", padding="5")
        bw_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(bw_frame, text="Bandwidth %:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.bandwidth_var = tk.IntVar(value=50)
        self.bandwidth_var.trace_add('write', lambda *args: self.on_setting_changed())
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
        self.msg_type_var.trace_add('write', lambda *args: self.on_setting_changed())
        msg_type_combo = ttk.Combobox(msg_frame, textvariable=self.msg_type_var, 
                                       values=["BST 93", "BST 94", "BST D0"], 
                                       width=15, state='readonly')
        msg_type_combo.grid(row=0, column=1, padx=5)
        
        # Message Length Configuration
        ttk.Label(msg_frame, text="Variable Length:").grid(row=1, column=0, sticky=tk.W, padx=5)
        self.variable_length_var = tk.BooleanVar(value=True)
        self.variable_length_var.trace_add('write', lambda *args: self.on_setting_changed())
        var_check = ttk.Checkbutton(msg_frame, variable=self.variable_length_var, 
                                     command=self.toggle_length_control)
        var_check.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        ttk.Label(msg_frame, text="Fixed Length:").grid(row=1, column=2, sticky=tk.W, padx=5)
        self.fixed_length_var = tk.IntVar(value=8)
        self.fixed_length_var.trace_add('write', lambda *args: self.on_setting_changed())
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
        
        self.pgn_text = scrolledtext.ScrolledText(pgn_frame, width=60, height=6)
        self.pgn_text.grid(row=0, column=0, padx=5, pady=5)
        self.pgn_text.insert('1.0', "60928, 129025, 129029, 130312, 130316, 128267")
        self.pgn_text.bind('<KeyRelease>', lambda e: self.on_setting_changed())

        # Parametric PGN Section (only show if PGN definitions are loaded)
        if self.pgn_definitions:
            param_frame = ttk.LabelFrame(main_frame, text="Parametric PGN Simulation (Limited PGNs)",
                                          padding="5")
            param_frame.grid(row=4, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)

            # Enable parametric mode checkbox
            self.use_parametric_var = tk.BooleanVar(value=False)
            self.use_parametric_var.trace_add('write', lambda *args: self.on_setting_changed())
            ttk.Checkbutton(param_frame, text="Use Parametric PGN (overrides random PGN list)",
                             variable=self.use_parametric_var,
                             command=self.toggle_parametric_mode).grid(row=0, column=0, columnspan=2,
                                                                         sticky=tk.W, padx=5, pady=5)

            # PGN selector
            ttk.Label(param_frame, text="Select PGN:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
            self.parametric_pgn_var = tk.IntVar()
            pgn_list = sorted(self.pgn_definitions.keys())
            pgn_display = [f"{pgn} - {self.pgn_definitions[pgn]['name']}" for pgn in pgn_list]
            self.parametric_pgn_combo = ttk.Combobox(param_frame, values=pgn_display,
                                                      width=40, state='disabled')
            self.parametric_pgn_combo.grid(row=1, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
            self.parametric_pgn_combo.bind('<<ComboboxSelected>>', self.on_parametric_pgn_selected)
            if pgn_list:
                self.parametric_pgn_combo.current(0)

            # Frame for dynamic field inputs
            self.param_fields_frame = ttk.Frame(param_frame)
            self.param_fields_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S),
                                          padx=5, pady=5)

            # Configure param_frame grid weights
            param_frame.columnconfigure(1, weight=1)
            param_frame.rowconfigure(2, weight=1)
        else:
            # No parametric mode available
            self.use_parametric_var = tk.BooleanVar(value=False)

        # Control Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=5, column=0, columnspan=2, pady=10)
        
        self.send_btn = ttk.Button(btn_frame, text="Send Messages", command=self.start_sending)
        self.send_btn.grid(row=0, column=0, padx=5)
        
        self.stop_btn = ttk.Button(btn_frame, text="Stop", command=self.stop_sending, state='disabled')
        self.stop_btn.grid(row=0, column=1, padx=5)
        
        self.save_btn = ttk.Button(btn_frame, text="Save Settings", command=self.save_settings)
        self.save_btn.grid(row=0, column=2, padx=5)
        
        # Status Display
        status_frame = ttk.LabelFrame(main_frame, text="Status", padding="5")
        status_frame.grid(row=6, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)

        self.status_text = scrolledtext.ScrolledText(status_frame, width=60, height=8, state='disabled')
        self.status_text.grid(row=0, column=0, padx=5, pady=5)

        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(3, weight=1)  # PGN list
        main_frame.rowconfigure(4, weight=1)  # Parametric section
        main_frame.rowconfigure(6, weight=1)  # Status
        
    def toggle_length_control(self):
        """Enable/disable fixed length control based on variable length checkbox"""
        if self.variable_length_var.get():
            self.fixed_length_spin.config(state='disabled')
        else:
            self.fixed_length_spin.config(state='normal')
        self.update_calculations()

    def toggle_parametric_mode(self):
        """Enable/disable parametric mode controls"""
        if not self.pgn_definitions:
            return

        if self.use_parametric_var.get():
            self.parametric_pgn_combo.config(state='readonly')
            self.on_parametric_pgn_selected(None)  # Trigger field creation
        else:
            self.parametric_pgn_combo.config(state='disabled')
            # Clear field widgets
            for widget in self.param_fields_frame.winfo_children():
                widget.destroy()
            self.field_widgets.clear()

    def on_parametric_pgn_selected(self, event):
        """Handle PGN selection - create input fields for the selected PGN"""
        if not self.pgn_definitions:
            return

        # Get selected PGN number from combo box
        selection = self.parametric_pgn_combo.get()
        if not selection:
            return

        try:
            pgn_num = int(selection.split(' - ')[0])
        except (ValueError, IndexError):
            return

        # Clear existing field widgets
        for widget in self.param_fields_frame.winfo_children():
            widget.destroy()
        self.field_widgets.clear()

        # Get PGN definition
        pgn_def = self.pgn_definitions.get(pgn_num)
        if not pgn_def:
            return

        # Create input fields for each field in the PGN
        fields = pgn_def.get('fields', [])

        # Create a scrollable canvas for fields
        canvas = tk.Canvas(self.param_fields_frame, height=150)
        scrollbar = ttk.Scrollbar(self.param_fields_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)

        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        row = 0
        for field in fields:
            field_name = field['name']
            bit_length = field['bitLength']

            # Skip reserved fields
            if 'Reserved' in field_name or 'NMEA Reserved' in field_name:
                continue

            # Create label
            ttk.Label(scrollable_frame, text=f"{field_name}:").grid(row=row, column=0,
                                                                      sticky=tk.W, padx=5, pady=2)

            # Create entry widget
            entry = ttk.Entry(scrollable_frame, width=20)
            entry.grid(row=row, column=1, padx=5, pady=2, sticky=(tk.W, tk.E))
            entry.insert(0, "0")  # Default value
            entry.bind('<KeyRelease>', lambda e: self.on_setting_changed())

            # Add bit length info
            ttk.Label(scrollable_frame, text=f"({bit_length} bits)").grid(row=row, column=2,
                                                                            sticky=tk.W, padx=5, pady=2)

            self.field_widgets[field_name] = entry
            row += 1

        scrollable_frame.columnconfigure(1, weight=1)

        # Trigger settings save
        self.on_setting_changed()
        
    def update_serial_ports(self):
        """Update the list of available serial ports"""
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        self.port_combo['values'] = port_list
        
        # Preserve current selection if it still exists, otherwise select first
        current_port = self.port_var.get()
        if current_port in port_list:
            self.port_combo.set(current_port)
        elif port_list:
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

        # Check if using parametric mode
        use_parametric = self.use_parametric_var.get() and self.pgn_definitions

        if use_parametric:
            # Get selected parametric PGN
            selection = self.parametric_pgn_combo.get()
            if not selection:
                self.log_status("Error: No parametric PGN selected")
                return
            try:
                pgn_num = int(selection.split(' - ')[0])
                pgns = [pgn_num]
            except (ValueError, IndexError):
                self.log_status("Error: Invalid parametric PGN selection")
                return
        else:
            # Use random PGN list
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
        self.send_thread = threading.Thread(target=self.send_messages, args=(pgns, use_parametric), daemon=True)
        self.send_thread.start()
        
    def stop_sending(self):
        """Stop sending messages"""
        self.is_sending = False
        self.send_btn.config(state='normal')
        self.stop_btn.config(state='disabled')
        self.log_status("Stopping transmission...")
        
    def send_messages(self, pgns: List[int], use_parametric: bool = False):
        """Send messages continuously (runs in separate thread)"""
        msg_type = self.msg_type_var.get()
        bytes_per_sec = int(self.bytes_per_sec_var.get())

        if bytes_per_sec == 0:
            self.log_status("Error: Bandwidth is 0%, cannot send messages")
            self.is_sending = False
            self.root.after(0, lambda: self.send_btn.config(state='normal'))
            self.root.after(0, lambda: self.stop_btn.config(state='disabled'))
            return

        mode_str = "Parametric" if use_parametric else "Random"
        self.log_status(f"Starting transmission: {len(pgns)} PGNs, Type: {msg_type}, Mode: {mode_str}")

        message_count = 0
        start_time = time.time()

        try:
            while self.is_sending and self.serial_port and self.serial_port.is_open:
                for pgn in pgns:
                    if not self.is_sending:
                        break

                    # Generate message data
                    message_data = None

                    if use_parametric and pgn in self.pgn_definitions:
                        # Get field values from UI
                        field_values = {}
                        for field_name, widget in self.field_widgets.items():
                            try:
                                field_values[field_name] = int(widget.get())
                            except ValueError:
                                field_values[field_name] = 0

                        # Log field values for first message (debugging)
                        if message_count == 0:
                            field_str = ', '.join(f"{k}={v}" for k, v in field_values.items())
                            self.root.after(0, lambda: self.log_status(f"Parametric fields: {field_str}"))

                        # Encode fields into binary data
                        pgn_def = self.pgn_definitions[pgn]
                        fields = pgn_def.get('fields', [])
                        message_data = PGNEncoder.encode_fields(fields, field_values)
                        data_length = len(message_data)

                        # Log encoded data for first message (debugging)
                        if message_count == 0:
                            hex_data = ' '.join(f'{b:02X}' for b in message_data)
                            self.root.after(0, lambda: self.log_status(f"Encoded data: {hex_data}"))
                    else:
                        # Random data mode
                        if self.variable_length_var.get():
                            data_length = random.randint(5, 100)
                        else:
                            data_length = self.fixed_length_var.get()

                    # Generate message based on type
                    if msg_type == "BST 93":
                        bst_message = MessageGenerator.generate_bst93(pgn, data_length, message_data)
                    elif msg_type == "BST 94":
                        bst_message = MessageGenerator.generate_bst94(pgn, data_length, message_data)
                    else:  # BST D0
                        bst_message = MessageGenerator.generate_bstd0(pgn, data_length, message_data)
                    
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
