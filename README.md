# run68x
Human68k CUI Emulator

run68の改造版です。  
無保証につき各自の責任で使用して下さい。


## 仕様

### run68.ini

* `[all]` セクション ... 各種設定
  * `trapemulate`
  * `pc98`
  * `iothrough`
* `[environment]` セクション ... 環境変数の設定
  * `変数名=値`



## Build
Windows (Visual Studio 2022、x64)、WSL上のUbuntu 20.04でのみ確認しています。  
なにか問題があれば報告してください。

### Visual Studio
フォルダを開いてビルドしてください。

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
