# Memory space mappings

Limine already gives a pretty usable memory layout, so we'll keep that as a base.
Note that this means we cannot reclaim 'Bootloader Reclaimable' ram as there might be page maps there, and keeping them there is much easier then relocating elsewhere. The total reclaimable space is only ~1MB anyway so is not a huge loss.

## Example limine map (qemu)
```
(1) 0000000000001000-0000000100000000 00000000fffff000 -rw
(2) 000000fd00000000-0000010000000000 0000000300000000 -rw
(3) ffff800000000000-ffff800100000000 0000000100000000 -rw
(4) ffff80fd00000000-ffff810000000000 0000000300000000 -rw
(5) ffffffffc0000000-ffffffffc0004000 0000000000004000 -r-
(6) ffffffffc0004000-ffffffffc000a000 0000000000006000 -rw
```

We only need higher-half entries, lower half can be removed to clear up for user space. (1) and (2) are cleared. We keep:

```
ffff 8000 0000 0000 -> direct map
ffff 80fd 0000 0000 -> framebuffer
ffff ffff c000 0000 -> kernel code/static data
```

We need a kernel heap, so pick an unused higher-half PML4 entry address, e.g.

```
ffff ce80 0000 0000 -> kernel heap
```
