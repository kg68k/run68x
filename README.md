run68mac
========

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
```

Xcodeでビルドする場合(デバッグが楽)
```
$ mkdir build
$ cd build
$ cmake -G Xcode ..
$ open run68.xcodeproj
```

How to use
----------

```
$ ./run68 mdx2mus.x bos14.mdx
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
$ ./run68  HAS.X |& iconv -f sjis
X68k High-speed Assembler v3.09 Copyright 1990-94 by Y.Nakamura
使用法：as ［スイッチ］ ファイル名
	-t path		テンポラリパス指定
	-o name		オブジェクトファイル名
	-i path		インクルードパス指定
	-p [file]	リストファイル作成
	-n		最適化の禁止
	-w [n]		ワーニングレベルの指定(0〜4)
	-u		未定義シンボルを外部参照にする
	-d		すべてのシンボルを外部定義にする
	-8		シンボルの識別長を８バイトにする
	-s symbol[=num]	シンボルの定義
	-x [file]	シンボルの出力
	-f [f,m,w,p]	リストファイルのフォーマット指定
	-l		起動時にタイトルを表示する
	-e		外部参照オフセットのデフォルトをロングワードにする
	-b		ロングワードのPC間接を絶対ロングにする
	-g		SCD用デバッグ情報の出力
	-c		HAS v2.x互換の最適化を行う
	-m 680x0	アセンブル対象CPUの指定

	環境変数 HAS の内容がコマンドラインの最後に追加されます
```
