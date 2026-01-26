# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in Sumi firmware, please report it by opening a private security advisory on GitHub rather than opening a public issue.

We will acknowledge your report within 48 hours and provide a detailed response within 7 days.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.4.x   | ✅ Yes    |
| 1.3.x   | ⚠️ Critical fixes only |
| < 1.3   | ❌ No     |

## Security Considerations

Sumi runs a local WiFi access point during setup. The default AP password (`sumisetup`) is intentionally simple for ease of use. Users should:

1. Complete setup quickly
2. Avoid storing sensitive data on the device
3. Use the device on trusted networks only

## Known Limitations

- The setup portal runs over HTTP (not HTTPS) on the local network
- WiFi credentials are stored in NVS (non-volatile storage) on the device
- No encryption for SD card contents
