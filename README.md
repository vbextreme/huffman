huffman v0.0.0
==================

Description:
============
very simple huffman implelemtation

Build:
======
```bash
$ meson setup build
$ cd build
$ ninja
```

Usage:
======
compress:
```bash
$ cat ../data/divina_commedia.txt | ./huffman -c > dc.hff
```
decompress:
```bash
$ cat ./dc.hff | ./huffman -d > dc.txt
```


