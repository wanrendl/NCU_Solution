# Copilot Instructions

## General Guidelines
- Use tab characters for indentation (not spaces) in this codebase.

## 项目指南
- 日志格式标准使用 `[date][level][threadnumber] info`，并且 `HttpServer` 构造时注入 `Logger` 实例，`main` 中定义 `Logger`。
- 主线程名称固定为 `Thread-0`，其他线程名称使用统一位数格式（如 `Thread-01`、`Thread-02` 等），每个线程有独立名称，格式 `Thread-<number>`（如 `Thread-1`、`Thread-13`）。
- `ReservationManager` 的工作线程 `processOther` 和 `processReservation` 使用线程名称 `Thread-Other` 和 `Thread-Reservation`。
- 在进行文件写入时，需要在日志中输出写入的文件名和写入内容。
- 服务端日志仅记录请求的 body 部分，不记录完整原始请求包。
- 在待支付接口 `/api/pending/delete` 和 `/api/pending/pay` 的请求体中，`uniqueid` 应直接发送为字符串，不使用数组包装。