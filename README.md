# fbshow
Open a framebuffer for a display device (e.g. small OLED or LCD screen on a Raspberry Pi) and write an image
or some text to it.

```
./configure
make
sudo make install
```

Note that the freetype2 and Magickwand stuff is handle by the Makefile, not by Gnu Configure.
Can't be bothered to figure out how they intend for this to be handled properly.

## Annoyances
At some point around 2022, the ssd1307fb driver (at least) seems to have stopped handling FB_DEFERRED_IO
correctly when using it with mmap() in userspace the way I used to do it, perhaps related to commit 5905585,
"fbdev: Put mmap for deferred I/O into drivers". Rather than investigating further, I opted to add
a single-byte write at the end to update the framebuffer. If someone knows what I should have done instead,
please let me know!
