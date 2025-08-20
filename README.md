huffman v0.0.0
==================

Description:
============
very simple huffman implelemtation and bitdiet, bitdiet is multithreading version of huffman

Build:
======
```bash
$ meson setup build
$ cd build
$ ninja
```

Usage:
======
compress only with huffman not multithreading:
```bash
$ cat ../data/divina_commedia.txt | ./huffman -c > dc.hff
```
decompress:
```bash
$ cat ./dc.hff | ./huffman -d > dc.txt
```
compress with multi blocks huffman multithreading(bitdiet):
```bash
$ cat ../data/divina_commedia.txt | ./huffman -bc > dc.bdet
```
decompress:
```bash
$ cat ./dc.bdet | ./huffman -bd > dc.txt
```
can decompress classic huffman with bitdiet enabled but not use umtithreading
```
$ cat ./dc.hff | ./huffman -bd > dc.txt
```

TEST SUITE
==========
in the data directory there are some files with which to test the software, to perform the test
```
$ ./testing
```
testing bitdiet
```
$ ./testing -b
```
In addition to the files present in data, some temporary files will be created and destroyed, including some files filled with random values just to test the speed of the software.<br>
Running all the tests may take some time.
