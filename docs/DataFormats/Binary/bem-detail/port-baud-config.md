# Get / Set Port Baud Config ⚠️ DEPRECATED

> **⚠️ DEPRECATION WARNING**
> This command is **DEPRECATED** and should not be used in new applications.
> Use [Port Baudrate](port-baudrate.md) (BEM ID 0x17) instead, which provides:
> - Direct baudrate values instead of baud codes
> - Separate session and store baudrate control
> - Improved device compatibility

---

Configures or retrieves the baud rate configuration for a specific device port using legacy baud code values. This command was used in older Actisense devices to set communication speeds.

This command has been superseded by **BEMCMD_PortBaudrate (0x17)**, which uses actual baudrate values (e.g., 115200) instead of device-specific baud codes.

## Command Ids

| Type | BST ID | BEM Id |
| -------- | ------- | ------- |
| Command | A1H | 12H |
| Response | A0H | 12H |

## Replacement Command

**Use [Port Baudrate](port-baudrate.md) (0x17) for all new development.**

The Port Baudrate command provides superior functionality:
- **Direct values**: Set baudrates as numbers (115200, 230400, etc.) instead of device-specific codes
- **Session vs Store**: Control immediate (session) and persistent (store) baudrates independently
- **Clearer API**: More intuitive interface with explicit 32-bit baudrate values
- **Better compatibility**: Works across all Actisense device families

## Migration Guide

If you are currently using Port Baud Config (0x12), migrate to Port Baudrate (0x17):

### Old Code (Port Baud Config 0x12)
```
Send command 0x12 with baud code array
// Baud code meanings vary by device
// Example: Code 7 might mean 115200 on one device, something else on another
```

### New Code (Port Baudrate 0x17)
```
Send command 0x17 with:
- Port number: 1
- Session baudrate: 115200 (0x0001C200)
- Store baudrate: 115200 (0x0001C200) or 0xFFFFFFFF (no change)

Follow with Commit To EEPROM (0x01) if store baudrate was changed
```

See [Port Baudrate](port-baudrate.md) documentation for detailed examples.

## Legacy Information

This command used device-specific baud code arrays rather than actual baudrate values. The encoding varied between device families (NGT, NGW, W2K1, etc.), making cross-device compatibility difficult.

**Port Baud Config is no longer documented or supported in the SDK.**

For historical reference:
- Used array of baud code values (one per port)
- Baud codes were device-specific enumerations
- Required separate hardware baud command (0x16) for immediate changes
- Less flexible than modern Port Baudrate command

## Notes

- **Do Not Use**: This command should not be used in new applications
- **Existing Code**: Applications using this command should migrate to Port Baudrate (0x17)
- **Device Support**: Newer devices may not support this legacy command
- **No Decode Function**: The decode function for this command has been removed from current SDK versions
- **See Also**:
  - [Port Baudrate](port-baudrate.md) - Recommended replacement
  - [Hardware Baud](hardware-baud.md) - Also deprecated, use Port Baudrate (0x17)