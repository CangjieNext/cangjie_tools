# 提取字段与提取入参表达式案例

本文依据《仓颉编程语言规范》0.53.18 的“表达式”章节，记录提取字段（Introduce Field）和提取入参（Introduce Parameter）当前支持的表达式类型及转换结果。

- 仓颉规范：<https://cangjie-lang.cn/docs?url=%2F0.53.18%2FSpec%2Fsource_zh_cn%2FChapter_04_Expressions%28zh%29.html>
- 竞品参考：[IntelliJ IDEA Extract field](https://www.jetbrains.com/help/idea/extract-field.html)
- 竞品参考：[IntelliJ IDEA Extract parameter](https://www.jetbrains.com/help/idea/extract-parameter.html)

IntelliJ IDEA 的转换原则是：提取字段时声明新字段并用所选表达式初始化，在原位置使用字段；
不能直接在字段声明处求值时，在原方法中增加字段赋值。提取入参时用新参数替换所选表达式，
并将原表达式传入所有调用点。仓颉实现沿用这一原则，并同步修改跨包调用点。

以下案例统一使用字段名或参数名 `extracted`。

## 支持类型概览

| 表达式类型 | 主要 ASTKind | 提取字段 | 提取入参 |
| --- | --- | --- | --- |
| 字面量 | `LIT_CONST_EXPR` | 支持 | 支持 |
| 变量名 | `REF_EXPR` | 支持 | 支持 |
| 函数名 | `REF_EXPR` | 支持 | 支持 |
| 泛型函数名 | `REF_EXPR` / `MEMBER_ACCESS` | 支持 | 支持 |
| 条件表达式 | `IF_EXPR` | 支持 | 支持 |
| 模式匹配表达式 | `MATCH_EXPR` | 支持 | 支持 |
| 数值类型转换 | `TYPE_CONV_EXPR` | 支持 | 支持 |
| this 表达式 | `REF_EXPR(isThis)` | 支持 | 支持 |
| 括号表达式 | `PAREN_EXPR` | 支持 | 支持 |
| 成员访问 | `MEMBER_ACCESS` | 支持 | 支持 |
| 函数调用 | `CALL_EXPR` | 支持 | 支持 |
| 索引访问 | `SUBSCRIPT_EXPR` | 支持 | 支持 |
| 问号操作符 | `OPTIONAL_EXPR` / `OPTIONAL_CHAIN_EXPR` | 支持 | 支持 |
| 一元表达式 | `UNARY_EXPR` | 支持 | 支持 |
| 算术表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 关系表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 类型判断 | `IS_EXPR` | 支持 | 支持 |
| 类型转换 | `AS_EXPR` | 支持 | 支持 |
| 位运算表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 区间表达式 | `RANGE_EXPR` | 支持 | 支持 |
| 逻辑表达式 | `BINARY_EXPR` | 支持 | 支持 |
| coalescing 表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 管道表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 组合表达式 | `BINARY_EXPR` | 支持 | 支持 |
| 复合赋值表达式 | `ASSIGN_EXPR` | - | 条件支持 |
| 指针构造表达式 | `POINTER_EXPR` | 支持 | 支持 |
| 数组字面量/构造 | `ARRAY_LIT` / `ARRAY_EXPR` | 支持 | 支持 |
| 元组字面量 | `TUPLE_LIT` | 支持 | 支持 |

## 提取字段案例

### 1. 字面量

选中内容：`42`。

提取前：

```cangjie
func value(): Int64 {
    return 42
}
main() {
    println(value())
}
```

提取后：

```cangjie
private var extracted: Int64 = 42
func value(): Int64 {
    return extracted
}
main() {
    println(value())
}
```

### 2. 变量名

选中内容：`source`。

提取前：

```cangjie
let source: Int64 = 42

func value(): Int64 {
    return source
}
main() {
    println(value())
}
```

提取后：

```cangjie
let source: Int64 = 42

private var extracted: Int64 = 0
func value(): Int64 {
    extracted = source
    return extracted
}
main() {
    println(value())
}
```

### 3. 函数名

选中内容：`double`。

提取前：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func transform(): (Int64) -> Int64 {
    return double
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

private var extracted: (Int64) -> Int64 = double
func transform(): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform()(2))
}
```

### 4. 泛型函数名

选中内容：`identity<Int64>`。

提取前：

```cangjie
func identity<T>(value: T): T {
    return value
}

func transform(): (Int64) -> Int64 {
    return identity<Int64>
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func identity<T>(value: T): T {
    return value
}

private var extracted: (Int64) -> Int64 = identity<Int64>
func transform(): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform()(2))
}
```

### 5. 条件表达式

选中内容：`整个 if 表达式`。

提取前：

```cangjie
func choose(enabled: Bool): Int64 {
    return if (enabled) {
        1
    } else {
        2
    }
}
main() {
    println(choose(true))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func choose(enabled: Bool): Int64 {
    extracted = if (enabled) {
        1
    } else {
        2
    }
    return extracted
}
main() {
    println(choose(true))
}
```

### 6. 模式匹配表达式

选中内容：`整个 match 表达式`。

提取前：

```cangjie
func describe(value: Int64): String {
    return match (value) {
        case 0 => "zero"
        case _ => "other"
    }
}
main() {
    println(describe(0))
}
```

提取后：

```cangjie
private var extracted: String = ""
func describe(value: Int64): String {
    extracted = match (value) {
        case 0 => "zero"
        case _ => "other"
    }
    return extracted
}
main() {
    println(describe(0))
}
```

### 7. 数值类型转换

选中内容：`Int64(value)`。

提取前：

```cangjie
func convert(value: Int32): Int64 {
    return Int64(value)
}
main() {
    println(convert(2))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func convert(value: Int32): Int64 {
    extracted = Int64(value)
    return extracted
}
main() {
    println(convert(2))
}
```

### 8. this 表达式

选中内容：`this`。

提取前：

```cangjie
class Box {

func self(): Box {
    return this
}
}
main() {
    let box = Box()
    println(box.self())
}

class Box {
```

提取后：

```cangjie
class Box {

private var extracted: Box = this
    func self(): Box {
        return extracted
    }
}
main() {
    let box = Box()
    println(box.self())
}

class Box {
```

### 9. 括号表达式

选中内容：`(a + b)`。

提取前：

```cangjie
func calculate(a: Int64, b: Int64): Int64 {
    return (a + b) * 2
}
main() {
    println(calculate(1, 2))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func calculate(a: Int64, b: Int64): Int64 {
    extracted = (a + b)
    return extracted * 2
}
main() {
    println(calculate(1, 2))
}
```

### 10. 成员访问

选中内容：`this.count`。

提取前：

```cangjie
class Counter {
    public var count: Int64 = 1

func current(): Int64 {
    return this.count
}
}
main() {
    let counter = Counter()
    println(counter.current())
}

class Counter {
    public var count: Int64 = 1
    private var extracted: Int64 = 0
```

提取后：

```cangjie
class Counter {
    public var count: Int64 = 1

func current(): Int64 {
        this.extracted = this.count
        return this.extracted
    }
}
main() {
    let counter = Counter()
    println(counter.current())
}

class Counter {
    public var count: Int64 = 1
```

### 11. 函数调用

选中内容：`source()`。

提取前：

```cangjie
func source(): Int64 {
    return 42
}

func value(): Int64 {
    return source()
}
main() {
    println(value())
}
```

提取后：

```cangjie
func source(): Int64 {
    return 42
}

private var extracted: Int64 = 0
func value(): Int64 {
    extracted = source()
    return extracted
}
main() {
    println(value())
}
```

### 12. 索引访问

选中内容：`整个 values[0]`。

提取前：

```cangjie
let values = [10, 20]

func first(): Int64 {
    return values[0]
}
main() {
    println(first())
}
```

提取后：

```cangjie
let values = [10, 20]

private var extracted: Int64 = 0
func first(): Int64 {
    extracted = values[0]
    return extracted
}
main() {
    println(first())
}
```

### 13. 问号操作符

选中内容：`box?.value`。

提取前：

```cangjie
class Box {
    public let value: Int64 = 1
}
let box: ?Box = Some(Box())

func optionalValue(): ?Int64 {
    return box?.value
}
main() {
    println(optionalValue())
}
```

提取后：

```cangjie
class Box {
    public let value: Int64 = 1
}
let box: ?Box = Some(Box())

private var extracted: ?Int64 = box?.value
func optionalValue(): ?Int64 {
    return extracted
}
main() {
    println(optionalValue())
}
```

### 14. 一元表达式

选中内容：`-value`。

提取前：

```cangjie
func negate(value: Int64): Int64 {
    return -value
}
main() {
    println(negate(2))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func negate(value: Int64): Int64 {
    extracted = -value
    return extracted
}
main() {
    println(negate(2))
}
```

### 15. 算术表达式

选中内容：`a + b`。

提取前：

```cangjie
func add(a: Int64, b: Int64): Int64 {
    return a + b
}
main() {
    println(add(1, 2))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func add(a: Int64, b: Int64): Int64 {
    extracted = a + b
    return extracted
}
main() {
    println(add(1, 2))
}
```

### 16. 关系表达式

选中内容：`a < b`。

提取前：

```cangjie
func less(a: Int64, b: Int64): Bool {
    return a < b
}
main() {
    println(less(1, 2))
}
```

提取后：

```cangjie
private var extracted: Bool = false
func less(a: Int64, b: Int64): Bool {
    extracted = a < b
    return extracted
}
main() {
    println(less(1, 2))
}
```

### 17. 类型判断

选中内容：`value is Int64`。

提取前：

```cangjie
func isInteger(value: Any): Bool {
    return value is Int64
}
main() {
    println(isInteger(1))
}
```

提取后：

```cangjie
private var extracted: Bool = false
func isInteger(value: Any): Bool {
    extracted = value is Int64
    return extracted
}
main() {
    println(isInteger(1))
}
```

### 18. 类型转换

选中内容：`value as Int64`。

提取前：

```cangjie
func toInteger(value: Any): Int64 {
    return value as Int64
}
main() {
    println(toInteger(1))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func toInteger(value: Any): Int64 {
    extracted = value as Int64
    return extracted
}
main() {
    println(toInteger(1))
}
```

### 19. 位运算表达式

选中内容：`value & 0xff`。

提取前：

```cangjie
func mask(value: UInt64): UInt64 {
    return value & 0xff
}
main() {
    println(mask(0x123))
}
```

提取后：

```cangjie
private var extracted: UInt64 = 0
func mask(value: UInt64): UInt64 {
    extracted = value & 0xff
    return extracted
}
main() {
    println(mask(0x123))
}
```

### 20. 区间表达式

选中内容：`1..=10`。

提取前：

```cangjie
func numbers(): Range<Int64> {
    return 1..=10
}
main() {
    println(numbers())
}
```

提取后：

```cangjie
private var extracted: Range<Int64> = 1..=10
func numbers(): Range<Int64> {
    return extracted
}
main() {
    println(numbers())
}
```

### 21. 逻辑表达式

选中内容：`enabled && ready`。

提取前：

```cangjie
func accepted(enabled: Bool, ready: Bool): Bool {
    return enabled && ready
}
main() {
    println(accepted(true, false))
}
```

提取后：

```cangjie
private var extracted: Bool = false
func accepted(enabled: Bool, ready: Bool): Bool {
    extracted = enabled && ready
    return extracted
}
main() {
    println(accepted(true, false))
}
```

### 22. coalescing 表达式

选中内容：`option ?? 0`。

提取前：

```cangjie
func value(option: ?Int64): Int64 {
    return option ?? 0
}
main() {
    println(value(Some(1)))
}
```

提取后：

```cangjie
private var extracted: Int64 = 0
func value(option: ?Int64): Int64 {
    extracted = option ?? 0
    return extracted
}
main() {
    println(value(Some(1)))
}
```

### 23. 管道表达式

选中内容：`value |> double`。

提取前：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func calculate(value: Int64): Int64 {
    return value |> double
}
main() {
    println(calculate(2))
}
```

提取后：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

private var extracted: Int64 = 0
func calculate(value: Int64): Int64 {
    extracted = value |> double
    return extracted
}
main() {
    println(calculate(2))
}
```

### 24. 组合表达式

选中内容：`组合表达式`。

提取前：

```cangjie
func increment(value: Int64): Int64 {
    return value + 1
}
func double(value: Int64): Int64 {
    return value * 2
}

func transform(): (Int64) -> Int64 {
    return increment ~> double
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func increment(value: Int64): Int64 {
    return value + 1
}
func double(value: Int64): Int64 {
    return value * 2
}

private var extracted: (Int64) -> Int64 = increment ~> double
func transform(): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform()(2))
}
```

### 25. 指针构造表达式

选中内容：`构造表达式`。

提取前：

```cangjie
func pointer(): CPointer<Int64> {
    return CPointer<Int64>()
}
main() {
    let p = pointer()
}
```

提取后：

```cangjie
private var extracted: CPointer<Int64> = CPointer<Int64>()
func pointer(): CPointer<Int64> {
    return extracted
}
main() {
    let p = pointer()
}
```

### 26. 数组字面量/构造

选中内容：`[1, 2]`。

提取前：

```cangjie
func values(): Array<Int64> {
    return [1, 2]
}
main() {
    println(values())
}
```

提取后：

```cangjie
private var extracted: Array<Int64> = [1, 2]
func values(): Array<Int64> {
    return extracted
}
main() {
    println(values())
}
```

### 27. 元组字面量

选中内容：`整个元组`。

提取前：

```cangjie
func pair(): (Int64, String) {
    return (1, "one")
}
main() {
    println(pair())
}
```

提取后：

```cangjie
private var extracted: (Int64, String) = (1, "one")
func pair(): (Int64, String) {
    return extracted
}
main() {
    println(pair())
}
```

## 提取入参案例

### 1. 字面量

选中内容：`42`。

提取前：

```cangjie
func value(): Int64 {
    return 42
}
main() {
    println(value())
}
```

提取后：

```cangjie
func value(extracted: Int64): Int64 {
    return extracted
}
main() {
    println(value(42))
}
```

### 2. 变量名

选中内容：`source`。

提取前：

```cangjie
let source: Int64 = 42

func value(): Int64 {
    return source
}
main() {
    println(value())
}
```

提取后：

```cangjie
let source: Int64 = 42

func value(extracted: Int64): Int64 {
    return extracted
}
main() {
    println(value(source))
}
```

### 3. 函数名

选中内容：`double`。

提取前：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func transform(): (Int64) -> Int64 {
    return double
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func transform(extracted: (Int64) -> Int64): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform(double)(2))
}
```

### 4. 泛型函数名

选中内容：`identity<Int64>`。

提取前：

```cangjie
func identity<T>(value: T): T {
    return value
}

func transform(): (Int64) -> Int64 {
    return identity<Int64>
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func identity<T>(value: T): T {
    return value
}

func transform(extracted: (Int64) -> Int64): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform(identity<Int64>)(2))
}
```

### 5. 条件表达式

选中内容：`整个 if 表达式`。

提取前：

```cangjie
func choose(enabled: Bool): Int64 {
    return if (enabled) {
        1
    } else {
        2
    }
}
main() {
    println(choose(true))
}
```

提取后：

```cangjie
func choose(enabled: Bool, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(choose(true, if (true) {
        1
    } else {
        2
    }))
}
```

### 6. 模式匹配表达式

选中内容：`整个 match 表达式`。

提取前：

```cangjie
func describe(value: Int64): String {
    return match (value) {
        case 0 => "zero"
        case _ => "other"
    }
}
main() {
    println(describe(0))
}
```

提取后：

```cangjie
func describe(value: Int64, extracted: String): String {
    return extracted
}
main() {
    println(describe(0, match (0) {
        case 0 => "zero"
        case _ => "other"
    }))
}
```

### 7. 数值类型转换

选中内容：`Int64(value)`。

提取前：

```cangjie
func convert(value: Int32): Int64 {
    return Int64(value)
}
main() {
    println(convert(2))
}
```

提取后：

```cangjie
func convert(value: Int32, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(convert(2, Int64(2)))
}
```

### 8. this 表达式

选中内容：`this`。

提取前：

```cangjie
class Box {

func self(): Box {
    return this
}
}
main() {
    let box = Box()
    println(box.self())
}

class Box {
```

提取后：

```cangjie
class Box {

func self(extracted: Box): Box {
    return extracted
}
}
main() {
    let box = Box()
    println(box.self(box))
}
```

### 9. 括号表达式

选中内容：`(a + b)`。

提取前：

```cangjie
func calculate(a: Int64, b: Int64): Int64 {
    return (a + b) * 2
}
main() {
    println(calculate(1, 2))
}
```

提取后：

```cangjie
func calculate(a: Int64, b: Int64, extracted: Int64): Int64 {
    return extracted * 2
}
main() {
    println(calculate(1, 2, (1 + 2)))
}
```

### 10. 成员访问

选中内容：`this.count`。

提取前：

```cangjie
class Counter {
    public var count: Int64 = 1

func current(): Int64 {
    return this.count
}
}
main() {
    let counter = Counter()
    println(counter.current())
}

class Counter {
    public var count: Int64 = 1
    private var extracted: Int64 = 0
```

提取后：

```cangjie
class Counter {
    public var count: Int64 = 1

func current(extracted: Int64): Int64 {
    return extracted
}
}
main() {
    let counter = Counter()
    println(counter.current(counter.count))
}
```

### 11. 函数调用

选中内容：`source()`。

提取前：

```cangjie
func source(): Int64 {
    return 42
}

func value(): Int64 {
    return source()
}
main() {
    println(value())
}
```

提取后：

```cangjie
func source(): Int64 {
    return 42
}

func value(extracted: Int64): Int64 {
    return extracted
}
main() {
    println(value(source()))
}
```

### 12. 索引访问

选中内容：`整个 values[0]`。

提取前：

```cangjie
let values = [10, 20]

func first(): Int64 {
    return values[0]
}
main() {
    println(first())
}
```

提取后：

```cangjie
let values = [10, 20]

func first(extracted: Int64): Int64 {
    return extracted
}
main() {
    println(first(values[0]))
}
```

### 13. 问号操作符

选中内容：`box?.value`。

提取前：

```cangjie
class Box {
    public let value: Int64 = 1
}
let box: ?Box = Some(Box())

func optionalValue(): ?Int64 {
    return box?.value
}
main() {
    println(optionalValue())
}
```

提取后：

```cangjie
class Box {
    public let value: Int64 = 1
}
let box: ?Box = Some(Box())

func optionalValue(extracted: ?Int64): ?Int64 {
    return extracted
}
main() {
    println(optionalValue(box?.value))
}
```

### 14. 一元表达式

选中内容：`-value`。

提取前：

```cangjie
func negate(value: Int64): Int64 {
    return -value
}
main() {
    println(negate(2))
}
```

提取后：

```cangjie
func negate(value: Int64, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(negate(2, -2))
}
```

### 15. 算术表达式

选中内容：`a + b`。

提取前：

```cangjie
func add(a: Int64, b: Int64): Int64 {
    return a + b
}
main() {
    println(add(1, 2))
}
```

提取后：

```cangjie
func add(a: Int64, b: Int64, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(add(1, 2, 1 + 2))
}
```

### 16. 关系表达式

选中内容：`a < b`。

提取前：

```cangjie
func less(a: Int64, b: Int64): Bool {
    return a < b
}
main() {
    println(less(1, 2))
}
```

提取后：

```cangjie
func less(a: Int64, b: Int64, extracted: Bool): Bool {
    return extracted
}
main() {
    println(less(1, 2, 1 < 2))
}
```

### 17. 类型判断

选中内容：`value is Int64`。

提取前：

```cangjie
func isInteger(value: Any): Bool {
    return value is Int64
}
main() {
    println(isInteger(1))
}
```

提取后：

```cangjie
func isInteger(value: Any, extracted: Bool): Bool {
    return extracted
}
main() {
    println(isInteger(1, 1 is Int64))
}
```

### 18. 类型转换

选中内容：`value as Int64`。

提取前：

```cangjie
func toInteger(value: Any): Int64 {
    return value as Int64
}
main() {
    println(toInteger(1))
}
```

提取后：

```cangjie
func toInteger(value: Any, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(toInteger(1, 1 as Int64))
}
```

### 19. 位运算表达式

选中内容：`value & 0xff`。

提取前：

```cangjie
func mask(value: UInt64): UInt64 {
    return value & 0xff
}
main() {
    println(mask(0x123))
}
```

提取后：

```cangjie
func mask(value: UInt64, extracted: UInt64): UInt64 {
    return extracted
}
main() {
    println(mask(0x123, 0x123 & 0xff))
}
```

### 20. 区间表达式

选中内容：`1..=10`。

提取前：

```cangjie
func numbers(): Range<Int64> {
    return 1..=10
}
main() {
    println(numbers())
}
```

提取后：

```cangjie
func numbers(extracted: Range<Int64>): Range<Int64> {
    return extracted
}
main() {
    println(numbers(1..=10))
}
```

### 21. 逻辑表达式

选中内容：`enabled && ready`。

提取前：

```cangjie
func accepted(enabled: Bool, ready: Bool): Bool {
    return enabled && ready
}
main() {
    println(accepted(true, false))
}
```

提取后：

```cangjie
func accepted(enabled: Bool, ready: Bool, extracted: Bool): Bool {
    return extracted
}
main() {
    println(accepted(true, false, true && false))
}
```

### 22. coalescing 表达式

选中内容：`option ?? 0`。

提取前：

```cangjie
func value(option: ?Int64): Int64 {
    return option ?? 0
}
main() {
    println(value(Some(1)))
}
```

提取后：

```cangjie
func value(option: ?Int64, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(value(Some(1), Some(1) ?? 0))
}
```

### 23. 管道表达式

选中内容：`value |> double`。

提取前：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func calculate(value: Int64): Int64 {
    return value |> double
}
main() {
    println(calculate(2))
}
```

提取后：

```cangjie
func double(value: Int64): Int64 {
    return value * 2
}

func calculate(value: Int64, extracted: Int64): Int64 {
    return extracted
}
main() {
    println(calculate(2, 2 |> double))
}
```

### 24. 组合表达式

选中内容：`组合表达式`。

提取前：

```cangjie
func increment(value: Int64): Int64 {
    return value + 1
}
func double(value: Int64): Int64 {
    return value * 2
}

func transform(): (Int64) -> Int64 {
    return increment ~> double
}
main() {
    println(transform()(2))
}
```

提取后：

```cangjie
func increment(value: Int64): Int64 {
    return value + 1
}
func double(value: Int64): Int64 {
    return value * 2
}

func transform(extracted: (Int64) -> Int64): (Int64) -> Int64 {
    return extracted
}
main() {
    println(transform(increment ~> double)(2))
}
```

### 25. 复合赋值表达式（仅提取入参）

选中内容：`整个复合赋值表达式`。

复合赋值采用值传递转换：调用点先计算赋值后的值，原函数用新参数替代被更新的旧参数。

提取前：

```cangjie
func printAdded(value: Int64, delta: Int64) {
    value += delta
    println(value)
}
main() {
    printAdded(1, 2)
}
```

提取后：

```cangjie
func printAdded(delta: Int64, extracted: Int64) {
    println(extracted)
}
main() {
    printAdded(2, 1 + 2)
}
```

### 26. 指针构造表达式

选中内容：`构造表达式`。

提取前：

```cangjie
func pointer(): CPointer<Int64> {
    return CPointer<Int64>()
}
main() {
    let p = pointer()
}
```

提取后：

```cangjie
func pointer(extracted: CPointer<Int64>): CPointer<Int64> {
    return extracted
}
main() {
    let p = pointer(CPointer<Int64>())
}
```

### 27. 数组字面量/构造

选中内容：`[1, 2]`。

提取前：

```cangjie
func values(): Array<Int64> {
    return [1, 2]
}
main() {
    println(values())
}
```

提取后：

```cangjie
func values(extracted: Array<Int64>): Array<Int64> {
    return extracted
}
main() {
    println(values([1, 2]))
}
```

### 28. 元组字面量

选中内容：`整个元组`。

提取前：

```cangjie
func pair(): (Int64, String) {
    return (1, "one")
}
main() {
    println(pair())
}
```

提取后：

```cangjie
func pair(extracted: (Int64, String)): (Int64, String) {
    return extracted
}
main() {
    println(pair((1, "one")))
}
```

## 结果约束

- 提取字段时，字面量及不依赖方法局部上下文的表达式可以直接作为字段初始化器；依赖形参、局部变量或当前调用状态的表达式会在原方法中生成字段赋值。
- 提取入参会同步修改当前包和依赖包中的调用点，并用每个调用点的实际实参替换原表达式中的形参引用。
- 表中“支持”只表示表达式类型允许提取，最终仍需通过作用域、类型、可见性、常量初始化和目标函数等既有校验。
