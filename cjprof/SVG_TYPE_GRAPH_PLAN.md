# 仓颉堆内存快照类型关系图 SVG 生成方案

## 1. 需求确认

| 需求项 | 确认结论 |
|--------|---------|
| 触发方式 | `cjprof heap -i xxx.data --dump-report` 时自动生成 |
| 输出位置 | 与输入 `.data` 文件同目录，文件名 `xxx.svg` |
| 节点含义 | 每个节点代表一个类型（class name），标识：类型名、实例数量、shallow size |
| 边含义 | A→B 表示 A 类型的对象引用了 B 类型的对象；边上标识：引用 B 的实例总数、被引用 B 实例的 shallow size 总和 |
| 绘制范围 | **全量绘制**，不过滤，无论堆多大都要画出全图 |
| 数组类型 | `RawArray<T>` 和普通类型一样作为独立节点展示 |
| 节点形状 | 矩形（`shape=box`），文本标识数值，宽度自适应以容纳全名 |
| Retained size | **暂不展示** |
| 根对象标识 | 用不同颜色区分：全局根[G]、本地根[L]、未知根[U]、普通对象 |
| SVG 角色 | `--dump-report` 的新增附件，不影响现有 report 输出 |

---

## 2. 技术方案概述

### 2.1 架构选择

采用 **"C++ 生成 DOT + 后台调用 Graphviz 转 SVG"** 的两阶段方案：

1. **C++ 侧（cjprof）**：遍历堆对象数据，按类型聚合节点和边，生成 `.dot` 中间文件
2. **Graphviz 侧（dot）**：执行 `dot -Tsvg input.dot -o output.svg`，输出最终 SVG

**理由**：
- C++ 侧算法复杂度 O(N+E)，可高效处理 600 万+ 对象
- 复用系统已安装的 Graphviz，无需引入第三方图形库
- DOT 作为中间格式，便于调试和手动优化

### 2.2 代码位置

新增独立模块，职责分离：

```
include/Reporter/SvgTypeGraphGenerator.h
src/Reporter/SvgTypeGraphGenerator.cpp
```

在 `HeapAnalyzer::StartReportServer()` 的数据构建阶段完成后调用，于 HTTP 服务启动前生成 DOT 文件，并通过后台线程调用 Graphviz 生成 SVG，避免阻塞服务就绪时间。

---

## 3. 数据模型

### 3.1 聚合节点（按类型聚合）

```cpp
struct TypeNode {
    std::string className;      // 类型名（如 "com.meituan.banma.monitor.BaseMonitor.report:Message"）
    uint64_t instanceCount = 0; // 该类型的实例总数
    uint64_t totalShallowSize = 0; // 该类型所有实例的 shallow size 总和
    bool hasGlobalRoot = false; // 是否有实例是全局根 [G]
    bool hasLocalRoot = false;  // 是否有实例是本地根 [L]
    bool hasUnknownRoot = false;// 是否有实例是未知根 [U]
};
```

### 3.2 聚合边（按类型对聚合）

```cpp
struct TypeEdge {
    uint64_t refCount = 0;       // 源类型实例引用目标类型实例的总次数
    uint64_t refShallowSize = 0; // 被引用的目标类型实例的 shallow size 总和
};
```

### 3.3 存储结构

```cpp
// 节点映射：className -> TypeNode
std::unordered_map<std::string, TypeNode> typeNodes;

// 边映射：srcClassName -> dstClassName -> TypeEdge
std::unordered_map<std::string, std::unordered_map<std::string, TypeEdge>> typeEdges;

// 对象 ID -> 类型名 快速索引（用于边解析时查找目标对象类型）
std::unordered_map<uint64_t, std::string> objectIdToName;

// 对象 ID -> shallow size 快速索引
std::unordered_map<uint64_t, uint64_t> objectIdToSize;
```

---

## 4. 算法设计

### 4.1 算法流程

```
输入: heapObjects (vector<HeapObject>), gcRoots (vector<GcRoot>), outputSvgPath
输出: xxx.svg 文件

阶段 1: 构建对象 ID 索引（O(N)）
  for each obj in heapObjects:
    objectIdToName[obj.object_id] = obj.name
    objectIdToSize[obj.object_id] = obj.size

阶段 2: 聚合类型节点（O(N)）
  for each obj in heapObjects:
    node = typeNodes[obj.name]
    node.instanceCount++
    node.totalShallowSize += obj.size

阶段 3: 标记根对象（O(R + N)，R 为根对象数）
  建立 rootId -> RootType 映射
  for each obj in heapObjects:
    if obj.object_id 在根映射中:
      更新 typeNodes[obj.name] 的根标记

阶段 4: 聚合类型边（O(E)，E 为所有对象引用总数）
  for each obj in heapObjects:
    srcName = obj.name
    for each refId in obj.refs:
      dstName = objectIdToName[refId]
      if dstName 不存在: continue
      dstSize = objectIdToSize[refId]
      edge = typeEdges[srcName][dstName]
      edge.refCount++
      edge.refShallowSize += dstSize

阶段 5: 生成 DOT 文件（O(V + E)，V 为类型数）
  写入 DOT 语法，节点/边标签按规范格式化

阶段 6: 调用 Graphviz 生成 SVG（后台线程）
  dot -Tsvg xxx.dot -o xxx.svg
```

### 4.2 复杂度分析

| 指标 | 复杂度 | 说明 |
|------|--------|------|
| 时间 | O(N + E) | N=对象数，E=引用边总数。对 660 万对象约需 5~15 秒 |
| 空间 | O(V + E_type) | V=类型数（~7000），E_type=类型对边数（远小于对象级边数） |
| DOT 文件大小 | 5~20 MB | 7000 节点 + 数万边 |
| Graphviz 耗时 | 1~5 min | 7000 节点全图布局，与机器性能相关 |

---

## 5. DOT / SVG 生成规范

### 5.1 根对象颜色映射

| 根类型 | DOT fillcolor | 说明 |
|--------|--------------|------|
| 全局根 [G] | `#ff6b6b` | 红色，只要有任意实例为全局根即标记 |
| 本地根 [L] | `#ffa502` | 橙色 |
| 未知根 [U] | `#eccc68` | 黄色 |
| 普通对象 | `#74b9ff` | 蓝色（默认） |

优先级：全局根 > 本地根 > 未知根 > 普通对象。一个类型同时满足多个条件时，按优先级取色。

### 5.2 节点标签格式

```dot
"typename" [label="typename\ninstances: 442623\nshallow: 20.26 MB", fillcolor="#74b9ff"];
```

- 类型名始终显示全名（DOT 自动计算节点宽度）
- 换行符 `\n` 分隔三项信息
- `fontsize=10` 保证可读性

### 5.3 边标签格式

```dot
"A" -> "B" [label="count: 50\nsize: 800 B"];
```

- 标识引用总数和 shallow size 总和
- 方向：从引用者指向被引用者（与对象引用方向一致）

### 5.4 图级属性

```dot
digraph TypeGraph {
  rankdir=TB;                    // 从上到下布局
  node [shape=box, style=filled, fontsize=10, fontname="Arial"];
  edge [fontsize=9, fontname="Arial"];
  // 节点和边...
}
```

### 5.5 自环与循环引用

- 自环边（A→A）正常绘制，DOT 会自动处理为自环弧线
- 循环引用（A→B→C→A）正常绘制，不特殊处理

### 5.6 DOT 特殊字符转义

类型名可能包含 `"`、`<`、`>`、`\` 等字符，需按 DOT 规则转义：
- `"` → `\"`
- `\` → `\\`
- 换行符等控制字符需剔除或替换为 `_`

---

## 6. 代码改动点

### 6.1 新增文件

| 文件 | 说明 |
|------|------|
| `include/Reporter/SvgTypeGraphGenerator.h` | 生成器类头文件 |
| `src/Reporter/SvgTypeGraphGenerator.cpp` | 生成器实现 |

### 6.2 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/Analyzer/HeapAnalyzer.cpp` | 在 `StartReportServer()` 数据构建完成后，调用 `SvgTypeGraphGenerator::Generate()` |
| `src/Reporter/CMakeLists.txt`（如存在） | 将新增 cpp 加入编译目标 |
| `src/CMakeLists.txt` | 同上（如使用根目录 CMakeLists） |

### 6.3 调用时序

在 `HeapAnalyzer::StartReportServer()` 中：

```cpp
// 现有代码：数据构建完成
auto t_after_build = std::chrono::steady_clock::now();

// 【新增】生成类型关系图 SVG
// 在 move 数据到 shared_ptr 之前，先基于本地 heapObjects/gcRoots 生成 DOT
std::string svgOutputPath = GetSvgOutputPath(m_filePath);
std::string dotTempPath = svgOutputPath + ".dot";

auto t_svg0 = std::chrono::steady_clock::now();
SvgTypeGraphGenerator::GenerateDot(heapObjects, gcRoots, dotTempPath);
auto t_svg1 = std::chrono::steady_clock::now();
auto dotMs = std::chrono::duration_cast<std::chrono::milliseconds>(t_svg1 - t_svg0).count();
LOG_INFO("[perf] TypeGraph DOT generation: {} ms", dotMs);

// 后台线程调用 Graphviz，避免阻塞 HTTP 服务启动
std::thread([dotTempPath, svgOutputPath]() {
    auto t0 = std::chrono::steady_clock::now();
    bool ok = SvgTypeGraphGenerator::RenderSvg(dotTempPath, svgOutputPath);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (ok) {
        LOG_INFO("[perf] TypeGraph SVG rendering: {} ms", elapsed);
        // 可选：生成成功后删除临时 DOT 文件
        std::remove(dotTempPath.c_str());
    } else {
        LOG_WARN("TypeGraph SVG generation failed");
    }
}).detach();

// 现有代码：Move data to shared_ptr...
```

### 6.4 SvgTypeGraphGenerator 接口设计

```cpp
// include/Reporter/SvgTypeGraphGenerator.h
#pragma once
#include <string>
#include <vector>
#include "Analyzer/Types.h"

class SvgTypeGraphGenerator {
public:
    // 基于堆对象和 GC 根数据生成 DOT 文件
    static void GenerateDot(
        const std::vector<HeapObject>& heapObjects,
        const std::vector<GcRoot>& gcRoots,
        const std::string& outputDotPath);

    // 调用系统 Graphviz 将 DOT 转为 SVG
    // 返回 true 表示成功
    static bool RenderSvg(
        const std::string& dotPath,
        const std::string& outputSvgPath);

    // 根据输入 .data 文件路径推导 .svg 输出路径
    static std::string GetSvgOutputPath(const std::string& dataFilePath);

private:
    static std::string EscapeDotString(const std::string& str);
    static std::string FormatByteSize(uint64_t bytes);
};
```

---

## 7. Graphviz 调用策略

### 7.1 跨平台路径查找

`RenderSvg()` 内部按以下优先级查找 `dot` 可执行文件：

1. 环境变量 `PATH` 中的 `dot`（`which dot` / `where dot`）
2. Windows 常见安装路径：`C:\Program Files\Graphviz\bin\dot.exe`
3. Linux/macOS 常见路径：`/usr/bin/dot`、`/usr/local/bin/dot`

### 7.2 失败处理

- 如果找不到 `dot`：记录 `LOG_WARN("Graphviz 'dot' not found, skip SVG generation")`，不报错退出
- 如果 `dot` 执行失败（非零退出码）：记录警告，保留 DOT 文件供用户手动转换
- 超时保护：对 `dot` 进程设置 10 分钟超时，超时则 kill 进程并记录错误

---

## 8. 性能评估

### 8.1 C++ 侧性能（DOT 生成）

以 `unknown.data`（660 万对象，~7000 类型）为基准：

| 阶段 | 预估耗时 | 说明 |
|------|---------|------|
| 构建 ID 索引 | ~200 ms | unordered_map 插入 660 万次 |
| 聚合类型节点 | ~200 ms | 遍历 + 累加 |
| 标记根对象 | ~100 ms | hashset 查找 |
| 聚合类型边 | ~2~5 s | 取决于平均引用数，假设 3000~6000 万次 ref 遍历 |
| 写 DOT 文件 | ~500 ms | 7000 节点 + 数万边 |
| **C++ 合计** | **~3~6 s** | 可接受 |

### 8.2 Graphviz 侧性能（SVG 渲染）

| 指标 | 预估值 | 说明 |
|------|--------|------|
| DOT 文件大小 | 5~15 MB | |
| SVG 输出大小 | 10~30 MB | 含全名标签的矢量图 |
| `dot` 布局耗时 | 1~5 min | 7000 节点层次布局，机器依赖 |
| 内存峰值 | 2~8 GB | Graphviz 内部数据结构 |

---

## 9. 风险与应对

| 风险 | 影响 | 应对方案 |
|------|------|---------|
| **Graphviz 处理 7000+ 节点过慢或 OOM** | SVG 无法生成 | 1. 后台线程执行，不阻塞主流程；2. 增加 10min 超时保护；3. 超时后保留 DOT 文件，提示用户手动执行 `dot -Tsvg` |
| **类型名过长导致节点极度膨胀** | SVG 横向尺寸过大，浏览器卡顿 | 已在需求中确认"始终显示全名"，按需求实现。如实际体验过差，后续迭代可增加"截断模式"开关 |
| **边数爆炸** | DOT 文件过大，Graphviz 无法处理 | 实际堆数据中引用关系是稀疏的，类型对边数通常 < 10 万。如确实过大，可在后续迭代增加"按引用计数过滤低权重边"的选项 |
| **跨平台 dot 路径不一致** | Windows/Linux 都找不到 dot | 实现多路径 fallback（见第 7 节），找不到时优雅降级 |
| **缓存加载路径（cacheLoaded=true）跳过数据构建** | SVG 无法生成 | 在 `cacheLoaded=true` 分支后同样调用 `SvgTypeGraphGenerator`，此时 `heapObjects` 和 `gcRoots` 已从缓存恢复，数据完整 |

---

## 10. 待办 Checklist

- [ ] 新建 `SvgTypeGraphGenerator.h/.cpp`
- [ ] 实现 `GenerateDot()`：类型聚合 + DOT 写入
- [ ] 实现 `RenderSvg()`：跨平台查找 dot 并执行
- [ ] 修改 `HeapAnalyzer::StartReportServer()`，插入调用点
- [ ] 更新 CMakeLists.txt 加入新源文件
- [ ] 在 Windows 环境测试 `unknown.data` 生成结果
- [ ] 验证 SVG 在浏览器中的可读性
