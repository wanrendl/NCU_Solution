# Project_Backend-API Linux 构建说明

## 1. 环境要求
- Ubuntu 22.04+（或其他支持 CMake 的 Linux 发行版）
- GCC 11+ 或 Clang 14+
- CMake 3.16+
- OpenSSL 开发库
- jsoncpp 开发库（带 pkg-config）

## 2. 安装依赖（Ubuntu）
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libssl-dev libjsoncpp-dev
```

## 3. 构建
在 `Project_Backend-API` 目录下执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 4. 运行
将前端资源文件（如 `index.html`、`tailwindcss.min.js`、`font-awesome.min.css`、`icon.png`）放在可执行文件当前工作目录可访问的位置，然后运行：

```bash
./build/Project_Backend-API
```

默认监听端口：`8080`

页面入口：
- `http://<服务器IP>:8080/reservation`

## 5. 常见问题
- 找不到 `jsoncpp`
    - 确认安装了 `libjsoncpp-dev` 和 `pkg-config`
- OpenSSL 链接失败
    - 确认安装了 `libssl-dev`
- 端口占用
    - 修改启动端口或释放 `8080`
