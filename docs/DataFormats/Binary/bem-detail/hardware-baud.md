# Get / Set Hardware Baud ⚠️ DEPRECATED

> **⚠️ DEPRECATION WARNING**
> This command is **DEPRECATED** and should not be used in new applications.
> Use [Port Baudrate](port-baudrate.md) (BEM ID 0x17) instead, which provides:
> - Unified control of both session (hardware) and store (EEPROM) baudrates
> - Direct baudrate values instead of baud codes
> - Single command for complete baudrate management

---

Configures or retrieves the hardware-level baud rate settings for device interfaces using legacy baud code values. This command was used to set immediate (session) communication speeds without affecting stored (EEPROM) values.

This command has been superseded by **BEMCMD_PortBaudrate (0x17)**, which manages both session and store baudrates in a single, more flexible interface.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 16H |
| Response | A0H | 16H |

## Replacement Command

**Use [Port Baudrate](port-baudrate.md) (0x17) for all new development.**

The Port Baudrate command provides superior functionality:
- **Session Baudrate**: Replaces Hardware Baud (0x16) - immediate hardware changes
- **Store Baudrate**: Replaces Port Baud Config (0x12) - persistent EEPROM values
- **Single Command**: One command controls both session and store, eliminating the need for two separate commands
- **Direct Values**: Uses actual baudrate values (115200, 230400, etc.) instead of baud codes

## Migration Guide

If you are currently using Hardware Baud (0x16), migrate to Port Baudrate (0x17):

### Old Workflow (Two Commands Required)
```
1. Set Port Baud Config (0x12) - Configure EEPROM baudrate
2. Commit To EEPROM (0x01) - Save to EEPROM
3. Set Hardware Baud (0x16) - Apply immediately to hardware
```

### New Workflow (Single Command)
```
1. Set Port Baudrate (0x17) with both session and store values
2. Commit To EEPROM (0x01) - Only if store baudrate was changed
```

**Example - Set Immediate Baudrate Only**:
```
Port Baudrate (0x17):
- Port: 1
- Session baudrate: 230400 (immediate hardware change)
- Store baudrate: 0xFFFFFFFF (do not change EEPROM)
```

**Example - Set Both Immediate and Persistent**:
```
Port Baudrate (0x17):
- Port: 1
- Session baudrate: 115200 (immediate hardware change)
- Store baudrate: 115200 (will persist after reboot)

Then send: Commit To EEPROM (0x01)
```

See [Port Baudrate](port-baudrate.md) documentation for detailed examples.

## Legacy Information

This command provided hardware-level (immediate, non-persistent) baudrate control using device-specific baud code arrays. Changes took effect immediately but were lost on device reset unless Port Baud Config (0x12) was also used.

**Hardware Baud is no longer documented or supported in the SDK.**

For historical reference:
- Changed hardware baudrate immediately (session/RAM only)
- Did not modify EEPROM - changes were temporary
- Used array of baud code values (one per port)
- Required Port Baud Config (0x12) for persistent changes
- Baud codes varied between device families

## Comparison: Old vs New

| Feature | Hardware Baud (0x16) | Port Baudrate (0x17) |
|---------|---------------------|---------------------|
| Immediate change | ✓ | ✓ (session baudrate) |
| Persistent change | ✗ (need 0x12) | ✓ (store baudrate) |
| Value format | Baud codes | Actual baudrates |
| Single command | ✗ | ✓ |
| Device-independent | ✗ | ✓ |

## Notes

- **Do Not Use**: This command should not be used in new applications
- **Existing Code**: Applications using this command should migrate to Port Baudrate (0x17)
- **Device Support**: Newer devices may not support this legacy command
- **No Decode Function**: The decode function for this command has been removed from current SDK versions
- **Connection Warning**: Changing session baudrates interrupts the active connection. Applications must close, wait, and reconnect at the new baudrate
- **See Also**:
  - [Port Baudrate](port-baudrate.md) - Recommended replacement for both session and store baudrate control
  - [Port Baud Config](port-baud-config.md) - Also deprecated, use Port Baudrate (0x17)
  - [Commit To EEPROM](commit-to-eeprom.md) - Required after changing store baudrate