run68mac
========

[![CMake](https://github.com/GOROman/run68mac/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/GOROman/run68mac/actions/workflows/cmake.yml)

Human68k CUI Emulator for MacOS

run68をMac上で動作するようにしたものです。CMakeを使用してビルドするように変更しています。
あらかじめ、brew install cmake などでインストールしておいてください。

How to build
------------

```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```
make install で /usr/local/bin/ 等にインストールされます。

デバッグのためにXcodeでビルドする場合
```
$ mkdir build
$ cd build
$ cmake -G Xcode ..
$ open run68.xcodeproj
```

How to use
----------

```
$ run68 起動したいX68000実行ファイル(*.x *.r) 引数
```



起動例
```
$ run68 mdx2mus.x bos14.mdx
```

SJISが文字化けする場合は iconv を使用
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

標準エラー出力も含める場合は |& でリダイレクト
```
$ ./run68 HAS.X |& iconv -f sjis
X68k High-speed Assembler v3.09 Copyright 1990-94 by Y.Nakamura
使用法：as ［スイッチ］ ファイル名
	-t path		テンポラリパス指定
	-o name		オブジェクトファイル名
	-i path		インクルードパス指定
	-p [file]	リストファイル作成
	-n		最適化の禁止
(...省略)
```
