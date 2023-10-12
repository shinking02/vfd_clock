# vfd_clock
ESP32とVFD管を使用した時計です。
![C304AA01-70E9-4F03-94E3-FAF61CE729B6_1_105_c](https://user-images.githubusercontent.com/72262790/231120577-a863b351-ff1c-4b84-8210-0ea8d6c84aed.jpeg)
![VFDesp32](https://github.com/shinking02/vfd_clock/assets/72262790/8d550fb8-8991-4242-aa77-a5c361f89517)

## ディレクトリ構成

```
.
├── README.md
├── gerber 
│   └── 基盤のデータです
├── include
│   └── 
├── lib
│   └── ライブラリはここに追加してください
├── platformio.ini
└── src
    └── main.cpp
```
## 開発
VSCodeとPlatformIOを使用しています<br>
SSIDなどは `lib/Secrets_example.h` を参考にSecrets.hを作成し、記述してください。
### ダウンロード
```shell
git clone https://github.com/shinking02/vfd_clock.git
```
