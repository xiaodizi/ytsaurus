## 从源码构建 YTsaurus

#### 构建要求

我们使用Ubuntu 18.04和20.04测试了YTsaurus的构建版本。其他Linux发行版也可以使用，但可能需要额外的工作。目前只支持x86_64 Linux。

下面是在构建YTsaurus之前需要安装的包的列表。`How to Build`部分包含获取这些包的分步说明。

- cmake 3.22+
- clang-14
- lld-14
- lldb-14
- conan 1.57.0
- git 2.20+
- python 3.8+
- pip3
- ninja 1.10+
- m4
- libidn11-dev
- protoc
- unzip

如何构建

1. 添加依赖库

   注意:大多数Linux发行版都需要以下存储库。如果你的GNU/Linux发行版的默认仓库中已经包含了所有需要的包，那么你可以跳过这一步。
   有关更多信息，请阅读您的分发文档和
   https://apt.llvm.org as well as https://apt.kitware.com/

   ```
   curl -s https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add
   curl -s https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
   echo "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-14 main" | sudo tee /etc/apt/sources.list.d/llvm.list >/dev/null
   echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
   sudo add-apt-repository ppa:ubuntu-toolchain-r/test

   sudo apt-get update
   ```
2. 安装 python (only for Ubuntu 18.04, skip this step for 20.04).

   ```
   sudo add-apt-repository ppa:deadsnakes/ppa
   sudo apt-get update
   sudo apt-get install -y python3.11 python3.11-dev python3.11-distutils python3.11-venv
   sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.11 1
   ```
3. 安装依赖关系.

   ```
   sudo apt-get install -y python3-pip ninja-build libidn11-dev m4 clang-14 lld-14 cmake unzip
   sudo python3 -m pip install PyYAML==6.0 conan==1.57.0 dacite
   ```
4. 安装 protoc.

   ```
   curl -sL -o protoc.zip https://github.com/protocolbuffers/protobuf/releases/download/v3.20.1/protoc-3.20.1-linux-x86_64.zip
   sudo unzip protoc.zip -d /usr/local
   rm protoc.zip
   ```
5. 创建工作目录. 确保电脑有80Gb的可用空间.我们还建议将该目录放在SSD上以减少构建时间.

   ```
   mkdir ~/ytsaurus && cd ~/ytsaurus
   mkdir build
   ```
6. 克隆YTsaurus仓库.

   ```
   git clone https://github.com/ytsaurus/ytsaurus.git
   ```
7. 构建 YTsaurus.

   运行cmake以生成构建配置:

   ```
   cd build
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../ytsaurus/clang.toolchain ../ytsaurus
   ```
   要构建，只需运行:

   ```
   ninja
   ```
   如果你需要构建具体的目标，你可以运行:

   ```
   ninja <target>
   ```
   YTsaurus服务器二进制文件可以在:

   ```
   yt/yt/server/all/ytserver-all
   ```
