## HLE工具开发指南

## 开源项目介绍

`hle`是一个仓颉调用ArkTS互操作代码模板自动生成工具。
该工具的输入是ArkTS接口声明文件，例如后缀.d.ts或者.d.ets结尾的文件，输出为包含BUILD.gn文件和src文件夹。src文件夹中包含的cj文件中存放生成的互操作代码。工具也会输出包含ArkTS文件的所有信息的json文件。

## 安装和使用指导：

1. 下载并解压最新仓颉包，配置仓颉环境

```sh
cd path_to_cangjie
source path_to_cangjie/envsetup.sh;
```

新安装可以参考：
    [安装仓颉工具链](https://cangjie-lang.cn/docs?url=%2F0.53.13%2Fuser_manual%2Fsource_zh_cn%2Ffirst_understanding%2Finstall_Community.html)

本项目也依赖 `stdx`库， 请自行下载[stdx](https://gitcode.com/Cangjie/cangjie-stdx/releases)二进制包，并将库路径： `/path/to/linux_x86_64_llvm/static/stdx`配置到环境变量 `CANGJIE_STDX_PATH`，建议设置为静态库路径，使得编译出来的 `hle` 可以独立运行; 若配置为动态库路径，则在运行编译出来的 `hle` 之前，还需要将 `stdx` 动态库路径配置到系统动态库路径中.

2. 本工具执行依赖nodejs

版本建议在v18.14.1及以上, 低版本可能存在ArkTS语法无法解析情况，建议使用新版本node。

[如何安装 Node.js](https://dev.nodejs.cn/learn/how-to-install-nodejs/)

3. 依赖

需要安装typescript依赖，里面包含typescript编译器，可以执行下列代码安装依赖

`cd src/dtsparser`
`npm install`

4. 克隆本项目hle分支，在本地构建后生成二进制文件main使用。

```sh
git clone /path/to/hle/repos
cd hle
```

二进制文件构建方式：

```sh
cd ${WORKDIR}/cangjie-tools/hyperlangExtension/build
python3 build.py                  (for Linux/Windows/macOS)
python3 build.py --cross-windows  (for Linux-to-Windows)
```

二进制文件位于 "target/release/bin"文件夹中（build构建的二进制为main）。在获取二进制文件后，直接使用命令行执行：

```sh
main -i 输入文件路径 -o 输出文件夹 -j ArkTS文件分析器
```

例如：

```sh
main  -i  /path/to/test.d.ts  -o  out  –j  /path/to/analysis.js --module-name=ohos.hilog
```

在Windows环境，文件目录当前不支持符号“\\”，仅支持使用“/”。
```sh
main -i  /path/to/test.d.ts -o out -j /path/to/analysis.js --module-name=ohos.hilog
```
