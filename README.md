# Distributed Message Routing and File Distribution Platform

一个面向演示与课程/项目落地的分布式消息路由与文件分发平台单仓库。

## 项目概览
- `gateway`：TCP 接入、协议解析、鉴权、命令分发、短期路由缓存
- `scheduler`：节点注册、心跳、负载评分、一致性哈希选路
- `message-service`：在线投递、离线补发、未读计数、`message_id` 幂等
- `file-service`：分块上传、热点 chunk 缓存、断点续传、分块下载
- `admin-api`：为演示后台聚合集群、消息、文件、路由状态
- `web/admin-dashboard`：React + Vite 可视化控制台

当前版本以“最小可运行演示闭环”为目标：
- C++ 后端服务骨架已可编译
- `demo_client` 可跑通离线消息补发和文件续传流程
- 后台页面可独立启动并查看演示面板
- Docker Compose 提供 Redis / MySQL / 前端容器化启动示例

## 技术栈
- C++20
- CMake
- MinGW UCRT64 `g++ 14.1.0`
- React + TypeScript + Vite
- Docker Compose
- Proto3

## 目录结构
```text
.
|-- services/              # Gateway / Scheduler / Message / File / Admin API
|-- libs/                  # 共享模型、协议、路由、存储、观测基础设施
|-- clients/demo-client/   # 演示客户端
|-- web/admin-dashboard/   # 管理后台
|-- infra/                 # Compose / MySQL / Redis 配置
|-- tests/                 # 单元测试与集成测试
|-- scripts/               # 配置、启动、停止演示脚本
|-- docs/                  # 架构说明
|-- proto/                 # 内部 RPC 接口定义草案
```

## 本地运行

### 方式一：一键启动
在 Windows PowerShell 中执行：

```powershell
cd E:\APP\Codex\Project\01
.\start_demo.cmd
```

它会自动：
- 使用固定 MinGW 工具链配置项目
- 编译 C++ 目标
- 启动前端开发服务器
- 打开浏览器访问 `http://localhost:5173`
- 运行 `demo_client.exe` 输出完整演示流程

如果还要顺手拉起 Redis / MySQL：

```powershell
.\start_demo.cmd -WithDocker
```

停止前端与 Docker 服务：

```powershell
.\stop_demo.cmd
```

### 方式二：手动构建
```powershell
& "C:\Program Files\CMake\bin\cmake.exe" --preset mingw-debug
& "C:\Program Files\CMake\bin\cmake.exe" --build build\mingw-debug --parallel
& ".\build\mingw-debug\demo_client.exe"
```

运行测试：

```powershell
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build\mingw-debug --output-on-failure
```

启动前端：

```powershell
cd .\web\admin-dashboard
npm install
npm run dev
```

## 当前已验证内容
- CMake 使用绝对路径配置成功
- MinGW UCRT64 `g++ 14.1.0` 编译成功
- `ctest` 3/3 通过
- `demo_client` 成功跑通消息离线补发与文件续传流程
- 前端 `npm run build` 成功

## 后续可扩展方向
- 将内存版存储替换为真实 Redis / MySQL 适配层
- 补齐真实 gRPC 服务间调用
- 将后台接到实时数据流而不是静态演示数据
- 把 C++ 服务纳入 Compose 统一启动

## 仓库建议名
推荐使用：`distributed-message-routing-platform`

