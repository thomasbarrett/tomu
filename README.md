## Build
```
git clone https://github.com/thomasbarrett/kvm.git
cd kvm
make
```

## Run
Currently, this project only supports direct kernel boot.
```
./bin/example <bzImage> <initramfs>
```

## Existing Device Support
- serial: the `serial_t` device emulates a 16550A UART device.
```
    epoll <──────── eventfd <─────╮
                                  │
               ╭──────────────╮   │
     read <─── │ │ │ fifo │ │ │ <─┴─ out
(nonblocking)  ╰──────────────╯
               ╭──────────────╮
    write ───> │ │ │ fifo │ │ │ ───> in
(nonblocking)  ╰──────────────╯
```

## In Progress
- phb: the `phb_t` device emulates a generic PCI host bridge.

```
~ # lspci -nn
00:00.0 Host bridge [0600]: Intel Corporation Device [8086:0d57]
```

## Future Device Support
- virtio-blk
- virtio-net
- vfio

## Documentation
* https://www.kernel.org/doc/Documentation/x86/boot.txt
* https://docs.kernel.org/admin-guide/sysrq.html

## Dependencies
- libaio:  0.3
