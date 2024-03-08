# run68x
Human68k CUI Emulator

run68の改造版です。  
無保証につき各自の責任で使用して下さい。


## 仕様

### コマンドラインオプション

* `-himem=<mb>` ... ハイメモリ容量指定(16, 32, 64, 128, 256, 384, 512, 768)
* `-f` ... ファンクションコールトレース
* `-tr <adr>` ... MPU命令トラップ
* `-d` ... 簡易デバッガ起動
* `-read-file-utf8` ... ファイル読み込み時にUTF-8からシフトJISに変換


### run68.ini

* `[all]` セクション ... 各種設定
  * `iothrough` ... 現在機能しません。
* `[environment]` セクション ... 環境変数の設定
  * `変数名=値`


### ハイメモリ

`-himem=<mb>`オプションを指定すると、MPUの命令セットは68000のままですが
アドレスバスのビット数が擬似的に拡張されハイメモリが使用可能になります。
実機とは互換性がありません。

実行ファイルにリンクされているXCライブラリにパッチをあてる機能はないため、
あらかじめ各自で実行ファイルを書き換えておく必要があります。

メインメモリのメモリ管理にハイメモリが連結され、以下のDOSコールが追加されます。
* `DOS _MALLOC3` ($ff60, $ff90)
* `DOS _SETBLOCK2` ($ff61, $ff91)
* `DOS _MALLOC4` ($ff62, $ff91)

`DOS _EXEC`でファイルタイプの指定ができなくなります。

開始アドレスは060turboと同じ`$10000000`です。  
アドレスのビット29、28が1の場合(`(address & 0x30000000) != 0`)にハイメモリとして扱われます。
容量を超える部分は(メインメモリが見えるのではなく)バスエラーになります。


### ファイル読み込み時のエンコーディング変換

実験的な機能です。

`-read-file-utf8`オプションを指定すると、読み込みモードで開いたファイルから内容を
読み込むときに、エンコーディングをUTF-8からシフトJISに変換します。

すべてのファイル読み込みに適用されるため、UTF-8以外で記述されたファイルや
バイナリファイルでは正しく内容を読み取ることができなくなります。

読み書きモードで開いたファイルや、新規作成したファイルは変換されません。
また、ファイルの書き込み時には変換されません。

現在の実装では、変換に失敗した場合は変換せずに続行します。


## Build
Windows (Visual Studio 2022、x64)、WSL上のUbuntu 22.04でのみ確認しています。  
なにか問題があれば報告してください。

### Visual Studio
フォルダを開いてビルドしてください。  
DebugビルドではなくReleaseビルドにすると動作が速くなります。

### その他の環境
```
$ cmake -B build
$ cmake --build build
```


## Origins
* https://github.com/rururutan/run68
* https://github.com/GOROman/run68mac
* https://github.com/yunkya2/run68mac
* https://github.com/jg1uaa/run68mac
* https://github.com/yosshin4004/run68mac
* https://github.com/iwadon/run68mac/tree/fix-debugger-eof


## License
GNU GENERAL PUBLIC LICENSE Version 2 or later.


## Author
TcbnErik / https://github.com/kg68k/run68x
