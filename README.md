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
- serial
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

## Future Device Support
- virtio-blk
- vfio

## Documentation
* https://www.kernel.org/doc/Documentation/x86/boot.txt
* https://docs.kernel.org/admin-guide/sysrq.html
