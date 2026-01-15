#!/usr/bin/env python3
"""
Simple test to verify n2ksender settings save/load functionality
"""

import tkinter as tk
from tkinter import ttk
import os
import sys

# Import the GUI class
from n2ksender import N2KSenderGUI

def test_settings():
    """Test that settings are saved and loaded correctly"""
    
    # Remove any existing ini file
    if os.path.exists("n2ksender.ini"):
        os.remove("n2ksender.ini")
        print("✓ Removed old settings file")
    
    # Create window and app
    root = tk.Tk()
    app = N2KSenderGUI(root)
    
    # Verify no ini file exists yet
    assert not os.path.exists("n2ksender.ini"), "INI file should not exist before first save"
    print("✓ No ini file at startup (as expected)")
    
    # Change some settings
    test_port = "COM5"
    test_baud = 57600
    test_bandwidth = 75
    test_pgn = "60928, 129025"
    
    app.port_var.set(test_port)
    app.baud_var.set(test_baud)
    app.bandwidth_var.set(test_bandwidth)
    app.pgn_text.delete('1.0', tk.END)
    app.pgn_text.insert('1.0', test_pgn)
    
    print(f"✓ Set test values: port={test_port}, baud={test_baud}, bandwidth={test_bandwidth}%")
    
    # Manually trigger save
    app.save_settings()
    root.update()  # Process pending events
    
    # Verify ini file was created
    assert os.path.exists("n2ksender.ini"), "INI file should be created after save"
    print("✓ Settings file created")
    
    # Check file content
    with open("n2ksender.ini", "r") as f:
        content = f.read()
        assert test_port in content, "Port should be in ini file"
        assert str(test_baud) in content, "Baud rate should be in ini file"
        assert str(test_bandwidth) in content, "Bandwidth should be in ini file"
        assert test_pgn in content, "PGN list should be in ini file"
    print("✓ All settings correctly written to file")
    
    # Close and reopen
    root.destroy()
    
    # Create new window
    root2 = tk.Tk()
    app2 = N2KSenderGUI(root2)
    
    # Verify settings were loaded
    assert app2.port_var.get() == test_port, f"Port not loaded: {app2.port_var.get()}"
    assert app2.baud_var.get() == test_baud, f"Baud not loaded: {app2.baud_var.get()}"
    assert app2.bandwidth_var.get() == test_bandwidth, f"Bandwidth not loaded: {app2.bandwidth_var.get()}"
    
    pgn_content = app2.pgn_text.get('1.0', tk.END).strip()
    assert test_pgn in pgn_content, f"PGN not loaded correctly: {pgn_content}"
    
    print("✓ All settings successfully reloaded")
    
    root2.destroy()
    
    # Clean up
    if os.path.exists("n2ksender.ini"):
        os.remove("n2ksender.ini")
    
    print("\n✅ All tests passed!")

if __name__ == "__main__":
    test_settings()
