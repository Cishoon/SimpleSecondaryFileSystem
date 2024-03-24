
---

# 用户使用说明

本项目使用CMake来构建和测试`FileSystem`，用户需要按照以下说明操作：

## 前置要求

- 确保已安装CMake（至少版本3.20）和合适的C++编译器。本项目使用C++17标准。
- 该项目针对GNU C++和Clang编译器进行了特定的优化设置，也支持MSVC编译器。对于其他编译器，会显示一条消息提示可能未能正确设置优化标志。
- Google Test框架已作为子目录集成，用于执行单元测试。
- 磁盘空间应不少于1G。
- 本项目测试环境为 Linux/macOS，未在Windows环境下测试。

## 构建项目

1. 打开命令行或终端。
2. 导航到项目根目录。
3. 创建一个构建目录并进入：

   ```sh
   mkdir build 
   cd build
   ```

4. 运行CMake来生成构建系统：

   ```sh
   cmake ..
   ```

5. 构建项目：

   ```sh
   make
   ```

这将会构建主项目`FileSystem`以及测试项目`Tests`。

## 运行应用

- 在构建目录中，运行生成的`FileSystem`可执行文件来启动程序，命令为

  ```sh
  ./FileSystem
  ```
- 成功启动后，会自动在当前文件夹创建一个`image.img`文件。
- 进入命令行界面，输入 `help` 可以查看所有支持命令的使用方法。

## 运行测试

- 为了运行所有测试，可以使用CTest，一个CMake的测试驱动程序。在构建目录中，运行命令`ctest`来执行所有配置的测试。
- 也可以直接运行`Tests`可执行文件来进行单元测试。这些测试验证了文件系统的不同组件是否按预期工作。

---