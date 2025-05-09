## Installation

```bash
cd lab5
make
sudo insmod int_stack.ko
chmod +x kernel_stack
```

Optional module parameters:
- `default_capacity=N`: Set initial stack capacity (default: 16)
- `enable_auto_resize=1`: Enable auto-resizing when stack is full (default: 0)
- `usb_vid=0xXXXX`: USB Vendor ID in hex format (default: 0x1234)
- `usb_pid=0xXXXX`: USB Product ID in hex format (default: 0x5678)

Example:
```bash
# For a Logitech device with VID=046d and PID=c52b
sudo insmod int_stack.ko usb_vid=0x046d usb_pid=0xc52b default_capacity=32
```

To find your USB device's IDs, use the `lsusb` command:
```
$ lsusb
Bus 001 Device 007: ID 046d:c52b Logitech, Inc. Unifying Receiver
```

If the usb-storage module claims your device, unload it first:
```bash
sudo rmmod usb_storage
```

## Usage

The device `/dev/int_stack` will only appear when the configured USB device is inserted.

```
./kernel_stack set-size <size>  # Configure the maximum stack capacity
./kernel_stack push <value>     # Push an integer onto the stack
./kernel_stack pop              # Pop and display the top stack element
./kernel_stack unwind           # Pop and display all stack elements
```

When the USB key is removed, the device will disappear but the stack contents will be preserved.
