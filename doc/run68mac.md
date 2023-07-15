run68mac
========

[![CMake](https://github.com/GOROman/run68mac/actions/workflows/cmake.yml/badge.svg)](https://github.com/GOROman/run68mac/actions/workflows/cmake.yml)

Human68k CUI Emulator

Human68k上の実行ファイル(*.x, *.r)をCUIで動かすツールrun68。run68を Mac 上で動作するようにしたものです。
（Mac以外の環境でも使えるようになりました。）

How to build
------------

CMakeを使用してビルドするように変更しています。あらかじめ、brew install cmake などでインストールしておいてください。漢字コードを変換するライブラリには libiconv を使用しています。
```
$ brew install cmake
$ brew install libiconv
```

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
$ run68 [実行ファイル(*.x *.r)] [引数]
```


起動例
```
$ run68 mdx2mus.x bos14.mdx
```

標準出力(stdout)および標準エラー出力(strerr)は、Shift-JISは自動的にUTF-8に変換します。
```
$ ./run68 mxc.x bos14_.mus

MML converter for mxdrv2  version 1.01 (c)1989 MFS soft, milk.
MDXタイトル : "Ｂlast Ｐower ！ 〜 from BOSCONIAN-X68"
PCMファイル : 'bos'
               ChA   ChB   ChC   ChD   ChE   ChF   ChG   ChH   ChP
Clock counts: 07872 07872 07872 07872 07872 07872 07872 07872 07872
Loop  counts: 07872 07872 07872 07872 07872 07872 07872 07872 07872
使用音色番号: 3 6 7 10 11 12 13 15
```

```
$ ./run68 HAS.X
X68k High-speed Assembler v3.09 Copyright 1990-94 by Y.Nakamura
使用法：as ［スイッチ］ ファイル名
	-t path		テンポラリパス指定
	-o name		オブジェクトファイル名
	-i path		インクルードパス指定
	-p [file]	リストファイル作成
	-n		最適化の禁止
(...省略)
```

TIPS
----

- あたかもX68000のコマンドがMacのターミナル上で動いているように見せる方法

1. /usr/local/x68/bin 以下にX68000形式の実行ファイルを置いておく
````
$ ls /usr/local/x68/bin
CV.x   HAS.X  hlk.x  note.x
````

2. 同名のシェルスクリプトをパスが通っている場所に配置(/usr/local/binなど)

/usr/local/bin/has.x
```sh
#!/bin/sh
run68 /usr/local/x68/bin/has.x $@
```

3. 実行属性(+x)を与える
````
$ chmod +x /usr/local/bin/has.x
````

4. X68000のコマンドが動作可能
````
$ has.x
X68k High-speed Assembler v3.09 Copyright 1990-94 by Y.Nakamura
使用法：as ［スイッチ］ ファイル名
	-t path		テンポラリパス指定
	-o name		オブジェクトファイル名
	-i path		インクルードパス指定
...
````
