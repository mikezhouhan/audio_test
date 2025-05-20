# WASAPI 音频捕获工具

这是一个基于Windows Audio Session API (WASAPI)的简单命令行音频捕获工具。它允许用户捕获系统音频并将其保存为WAV文件。

## 功能

- 捕获系统播放的音频（扬声器输出）
- 用户指定捕获时长
- 保存为标准WAV文件格式
- 显示捕获过程的进度

## 构建方法

### 使用CMake

```
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 使用Visual Studio

- 打开项目文件夹
- 打开Developer Command Prompt for VS
- 执行上述CMake命令
- 或者直接在Visual Studio中打开CMakeLists.txt

## 使用方法

1. 运行编译好的程序
2. 输入你想要捕获的秒数
3. 系统将开始捕获音频
4. 捕获完成后，WAV文件将保存在程序的当前目录

## 技术实现

本程序使用WASAPI的环回(Loopback)模式捕获系统音频，无需额外的音频硬件设备.
