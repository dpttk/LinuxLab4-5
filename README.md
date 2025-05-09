## Installation

```bash
make
sudo insmod int_stack.ko
chmod +x kernel_stack
```

Optional module parameters:
- `default_capacity=N`: Set initial stack capacity (default: 16)
- `enable_auto_resize=1`: Enable auto-resizing when stack is full (default: 0)

Example:
```bash
sudo insmod int_stack.ko default_capacity=32 enable_auto_resize=0
```

## Usage

```
./kernel_stack set-size <size>  # Configure the maximum stack capacity
./kernel_stack push <value>     # Push an integer onto the stack
./kernel_stack pop              # Pop and display the top stack element
./kernel_stack unwind           # Pop and display all stack elements
```
