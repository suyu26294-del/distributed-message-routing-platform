# 分布式消息路由与文件分发平台

## 模块总览
- `gateway`：TCP 协议入口、鉴权、命令分发、短期路由缓存。
- `scheduler`：服务节点注册、负载采样、一致性哈希选路。
- `message-service`：在线投递、离线存储、未读计数、幂等去重。
- `file-service`：上传会话、chunk 元数据、热点缓存、断点续传。
- `admin-api`：聚合状态并输出 JSON 给演示后台。

## 当前实现形态
- 后端先以内存版存储闭环验证主流程。
- 构建使用 `CMake + MinGW Makefiles + g++ 14.1.0`。
- Redis、MySQL、Proto、前端后台和 Compose 已提供最小示例与扩展入口。

## 演示流程
1. `demo-client` 让 Alice 登录并向离线的 Bob 发送消息。
2. Bob 登录后拉取离线消息，未读计数清零。
3. Alice 初始化上传，先传 chunk 0，再查询续传点，补传缺失块并完成上传。
4. `admin-api` 输出集群、路由、消息、文件统计 JSON。

## 一键启动
- 直接运行仓库根目录下的 `start_demo.cmd`
- 或执行 `powershell -ExecutionPolicy Bypass -File .\scripts\start_demo.ps1`
- 如需同时拉起 Redis/MySQL：`.\start_demo.cmd -WithDocker`
- 停止前端与 Docker 服务：`.\stop_demo.cmd`

## 目录约定
- `services/`：服务实现与各自入口。
- `libs/`：共享模型、协议、路由、存储、观测基础设施。
- `tests/`：单元与集成测试。
- `web/admin-dashboard/`：可视化后台。
- `infra/compose/`：本地依赖服务编排。
