# MSIME-UI

遵循[组织级约定](https://github.com/metasequoiaime/.github/blob/main/AGENTS.md)。

本仓是通用 Win32/Direct2D/DirectWrite 控件与渲染库。CandidateList 可以承载输入法候选，也可以承载其他列表；通过数据和回调接入业务，不读取 Server 配置、IPC、引擎、词库或全局输入状态。原生输入法窗口/皮肤适配器位于 MSIME-Server。CI 的 `scripts/check-boundary.py` 检查已知反向依赖，控件行为仍由本仓原生测试验证。
