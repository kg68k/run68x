run68mac
========

Human68k CUI Emulator for MacOS

run68をMac上で動作するようにしたものです。CMakeに変更しています。

How to build
------------

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

How to use
----------

```
$ ./run68 mdx2mus.x bos14.mdx
```

SJISが文字化けする場合は iconv で

```
$ ./run68 mxc.x bos14_.mus | iconv -f SJIS

MML converter for mxdrv2  version 1.01 (c)1989 MFS soft, milk.
MDXタイトル : "Ｂlast Ｐower ！ 〜 from BOSCONIAN-X68"
PCMファイル : 'bos'
               ChA   ChB   ChC   ChD   ChE   ChF   ChG   ChH   ChP
Clock counts: 07872 07872 07872 07872 07872 07872 07872 07872 07872
Loop  counts: 07872 07872 07872 07872 07872 07872 07872 07872 07872
使用音色番号: 3 6 7 10 11 12 13 15
```
