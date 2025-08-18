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

TEST SUITE
==========
in the data directory there are some files with which to test the software, to perform the test
```
$ ./testing
```
In addition to the files present in data, some temporary files will be created and destroyed, including some files filled with random values just to test the speed of the software.<br>
Running all the tests may take some time.
