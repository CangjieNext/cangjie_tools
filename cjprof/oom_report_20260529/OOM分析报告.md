## OOM 分析报告

### 基本信息
- 应用: `com.meituan.banma.hmcrowdsource` v13.2.1.006
- 设备: nova 12 Pro, ADA-AL00 6.0.0.130(SP25C00E130R3P7)
- 进程存活时间: 81s (1分21秒)
- 崩溃时进程 RSS: 941383 KB (~919 MB)
- 设备总内存: 11922868 KB (~11.4 GB), 可用: 5021696 KB (~4.9 GB)
- 仓颉堆: 总量 512 MB, 已用 511.99 MB (99.998%)

### 直接原因

应用在 UI 渲染期间请求分配 24 B 内存时触发 OOM。此时仓颉堆已完全耗尽（active units 131070/131072 = 99.998%），仅剩 2 units（8 KB）可用空间。

### OOM 触发调用栈

```
ohos.component.InteractableView::onAreaChange::lambda.1()(cj_interactable_view.cj:1129)
  → ohos.component.CallbackCJAreaChangedEvent::invoke(Int32, CPointer<...>, CPointer<...>)(cj_lambda_invoker_impl.cj:64)
    → ohos.ffi.ohosFFICJCallbackInvoker(Int64, Int32, CPointer<...>, CPointer<...>)(ffi_callback.cj:156)
      → (native) OHOS::Ace::NG::FrameNode::TriggerOnAreaChangeCallback
        → OHOS::Ace::NG::PipelineContext::FlushVsync
```

崩溃发生在 ArkUI 的 VSync 刷新路径上，由 `InteractableView::onAreaChange` 的 FFI 回调触发。

### 堆内存分析

#### Shallow Size Top 对象

| 排名 | 类型 | 数量 | Shallow Size | 说明 |
|------|------|------|-------------|------|
| 1 | RawArray<HashMapEntry<String,AnyType>>[16] | 129,941 | 81.29 MB | JSON 反序列化后的 HashMap 内部数组 |
| 2 | cjjson:AnyType | 1,365,589 | 20.84 MB | JSON 值容器 |
| 3 | com_meituan_banma_monitor.BaseMonitor.report:Message | 442,623 | 20.26 MB | **监控上报消息对象** |
| 4 | RawArray<U8>[16] | 136,603 | 18.76 MB | 字节数组 |
| 5 | stdx.encoding.json:JsonString | 591,158 | 13.53 MB | JSON 字符串 |
| 6 | stdx.encoding.json:JsonInt | 719,944 | 10.99 MB | JSON 整数 |
| 7 | HashMap<String,AnyType> | 129,941 | 10.91 MB | JSON Map |
| 8 | com_meituan_banma_nativecore.monitor.bean:DaBaiBean | 129,650 | 7.91 MB | 监控埋点 Bean |
| 9 | RawArray<U8>[32] | 30,114 | 7.81 MB | 字节数组 |
| 10 | RawArray<Message>[128] | 3,468 | 6.83 MB | 消息队列内部数组 |

#### Retained Size Top 对象

| 排名 | 对象 | Shallow | Retained | 说明 |
|------|------|---------|----------|------|
| 1 | Block<Message> | 80 B | 71.15 MB | 并发队列块，持有大量消息 |
| 2 | AtomicOptionReference<Block<Message>> | 16 B | 58.16 MB | 队列块引用节点 |
| 3 | Block<Message> | 80 B | 50.89 MB | 并发队列块 |
| 4 | AtomicOptionReference<Block<Message>> | 16 B | 9.73 MB | 队列块引用节点 |
| 5 | Block<Message> | 80 B | 9.08 MB | 并发队列块 |
| 6 | [G]:ohos.ffi:FFIDataManager | 48 B | 5.90 MB | **全局根**：FFI 数据管理器 |
| 7 | AtomicOptionReference<Block<Message>> | 16 B | 3.48 MB | 队列块引用节点 |
| 8 | [G]:TaskIpcManager | 32 B | 2.18 MB | **全局根**：任务 IPC 管理器 |
| 9 | [G]:ohos.ark_interop:JSContext | 240 B | 2.02 MB | **全局根**：JS 上下文 |
| 10 | [G]:WaybillDetailRepository | 72 B | 1.01 MB | **全局根**：运单详情仓库 |

#### 支配树可视化

已生成支配树可视化文件 `report/dominator_tree.svg`（7.94 MB），可通过浏览器直接打开查看完整的对象持有关系图。

### 根因分析

1. **泄漏对象是什么？**
   - `com_meituan.banma.monitor.BaseMonitor.report:Message` 实例高达 **442,623 个**，仅 shallow size 就占用 20.26 MB。
   - 伴随大量 JSON 序列化对象（JsonString 591K+、JsonInt 719K+、AnyType 1.36M+）和 `DaBaiBean` 129,650 个，说明监控埋点数据在序列化后堆积在内存中。

2. **谁持有它们？**
   - Retained size 分析显示，最大的持有者是 `std.collection.concurrent:Block<com_meituan.banma_monitor.BaseMonitor.report:Message>` 及其 `AtomicOptionReference` 引用节点。
   - 前 5 名 retained size 合计超过 **202 MB**，几乎全部被监控消息队列的内部块占据。

3. **为什么不能被 GC 回收？**
   - 这些 `Message` 对象被并发队列的 `Block` 节点直接引用，而队列本身是一个活跃的数据结构（虽然 Block 节点不是全局根，但被上层队列头/尾指针持续引用）。
   - 此外，全局根 `[G]:FFIDataManager` 持有 5.90 MB，`[G]:WaybillDetailRepository` 持有 1.01 MB，这些全局根对象及其引用的对象永远不会被 GC 回收。

4. **泄漏的业务场景是什么？**
   - 应用在启动后仅 **81 秒** 内就产生了 44.2 万条监控消息，平均每秒约 **5,460 条**。
   - 这种极高的生产速率表明监控埋点存在过度上报（可能是在循环、高频回调或页面切换中重复上报）。
   - 同时，消息队列的消费端似乎没有跟上生产速度，或者队列存在逻辑缺陷（如消费线程阻塞、异常后未恢复），导致消息无限堆积。
   - OOM 最终发生在 UI 渲染的 `onAreaChange` 回调中，说明 UI 线程在尝试分配微小内存（24 B）时因堆耗尽而崩溃。

### 建议修复方向

1. **限制监控消息队列大小并增加丢弃策略**
   - 为 `BaseMonitor` 的消息队列设置上限（如 1 万条），超过上限时丢弃旧消息或新消息，避免无限制堆积。
   - 检查队列的消费线程是否正常运行，是否有死锁、阻塞或异常退出导致消费停滞。

2. **降低监控埋点上报频率**
   - 81 秒内 44 万条消息显然过高。检查 `onAreaChange`、`Page switch` 等高频事件的埋点逻辑，增加采样率或合并上报（批量上报而非逐条入队）。
   - 特别关注 `BaseAlertPage` 相关的页面切换历史（日志中显示多次快速切换），可能在页面切换时触发了大量重复埋点。

3. **检查 FFI 回调生命周期管理**
   - 崩溃栈涉及 `InteractableView::onAreaChange` FFI 回调，且全局根 `FFIDataManager` 持有 5.90 MB。
   - 检查 UI 组件销毁时是否正确注销了 `onAreaChange` 等 FFI 回调，避免回调闭包和关联数据在 `FFIDataManager` 中持续累积。

### 附件说明

| 文件 | 说明 |
|------|------|
| `OOM分析报告.md` | 本分析报告 |
| `cjheap.dot` | 支配树 DOT 原始数据 |
| `dominator_tree.svg` | 支配树可视化图，用浏览器打开 |
