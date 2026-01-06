# Embedded Operating Systems

## Homework 1
請在 RPi 上撰寫一支外送系統。讓顧客可以順利點餐，並如願拿到熱騰騰的外送餐點。

## Homework 2
本次作業將延續作業一的內容，將原來單機版本的外送系統利用 socket server 修改成可連線版本。

## Homework 3
本次作業將延續作業二的內容，利用process或thread將伺服器改成多人連線版本，並且要能正確處理race condition的狀況。


## Lab 2
1. 移除不必要的功能, 縮小RPi OS 的 image size (應說明: 目的、移除哪些功能、如何移除、前後image size與功能的比較)

2. 在網路上下載可以評測 kernel 的套件, 比較原本與更改後的 kernel (應說明: 下載哪一套件、改善哪方面的效能與評測的結果)

3. 使 kernel 支援 real-time 功能, 可參照講義的作法, 依照自己 kernel 的版本來 patch (應說明: 欲下載哪一版本的補丁、如何下載與下載的結果)

### Lab2 hint:

由於版本差異，建議保留一開始從pi imager下載的原始版kernel，以及用bcm2711_defconfig的未自行更動版本

pi imager下載的原始版: 可打開wifi、連接螢幕，但不能做lab3的內容

bcm2711_defconfig下載的未更動版: 可能無法打開wifi、連接螢幕，但可做lab3的內容

如要功能皆齊全，請至https://www.raspberrypi.com/software/operating-systems/找到Raspberry Pi OS (Legacy, 64-bit)，下載完後用pi imager選擇use custom重新安裝至sd卡

在步驟5: Build with Configs時，建議可在make後加上 " -j4 "，-j<n>代表用n個核心一起跑，取決於你虛擬機設定，加速編譯

作業第三題patch則是要自行從步驟3: Get the Kernel Sources中更改branch的版本，找到在

           https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/ 中能夠對應的patch，建議從3.0~4.19範圍中尋找

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig 的功能為生出.config檔，仍需進行步驟5~9才能複製Kernel進SD卡，使raspberry pi 正常運作

## Lab 3
請在 Raspberry pi 上撰寫 " 學號跑馬燈 " 。
修改 Lab3 講義提供的 driver 程式，可以在 Raspberry pi 上透過 writer 將數字寫到 driver 當中，將
數字從 driver 中讀出來，並學會運用 GPIO 腳位顯示於七段顯示器。

## Lab 4
請在Raspberry Pi 上撰寫一支名字跑馬燈。在 Raspberry Pi 上透過 writer 程式將英文字母寫到
driver 當中，透過 reader 程式將該字從 driver 中讀出來，最後透過 socket 傳遞給 VM 上頭的
seg.py 程式，其會把該字用十六段顯示器 (GUI) 呈現出來。
## Lab 5
請在 VM 上撰寫一隻Socket程式。在 VM 上會先將 server 程式執行起來，等待 client 程式來做連線。當連線建立之後，server 會將列車的 ASCII art，透過 socket 傳給 client 端，於是在 client 端 terminal 上就會看見一台列車從右到左急駛而過。

## Lab 6
請在 VM 上撰寫一隻 socket 程式。多個客戶端同時連線進來對同一個帳戶進行存款與提款的動作，而伺服器端要能正確處理同時存取所導致的 race condition 問題，才不至於造成客戶金錢上的損失。

## Lab 7
請在 VM 上撰寫終極密碼遊戲。該遊戲由兩份程式組成，第一份程式利用 timer，每格一秒做猜數字的動作，用 signal 通知第二份程式，其會將被猜的數字讀進來，並給出猜中了、太大、太小這三種回應。兩隻程式透過 shared memory 做資料傳出的動作。

## Final Project
