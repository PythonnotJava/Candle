# 工作计划
> 最后更新: 2026-06-27 18:28

## 🔴 P0 - 紧急

## 🟡 P1 - 重要
  - [x] Int: isEven/isOdd/sign/abs/clamp/toDouble/toString/range/rangeFrom
  - [x] Bool: toggle/toInt/and/or/toString
  - [x] String: length/startsWith/endsWith/contains/indexOf/substring/repeated/reversed/padLeft/padRight/count/isPalindrome
  - [x] List: size/first/last/contains/indexOf/slice/join/concat
  - [x] Map: size/isEmpty/isNotEmpty/containsKey
  - [ ] Double: isNegative/isZero/sign/abs/clamp/toInt/round/floor/ceil/toString
  - [ ] 高阶 stdlib: json.candle (cJSON) / http.candle (mongoose)

## 🟢 P2 - 一般

## ✅ 已完成
- [x] AOT load 预编译 — load 的标准库 reflect 方法编译进 exe `#codegen` `#stdlib` — 2026-06-27 完成 (codegen 预解析 .candle stdlib → 提取 reflect 方法 → 生成 C wrapper → 编译进 exe。forward declarations + self.method() 处理 + AOT filter。21/21 全绿。)
- [x] B: codegen 对齐 — String() 转换 / 字符串拼接 / load std / list 打印 `#codegen` — 2026-06-27 完成 (String(x)→candle_to_str(x) (sema标注string类型+codegen映射)。candle_str_concat已存在于runtime。print_list→print_ptr(CandleList*)。load std→#include 已实现。21/21回归全绿。)
- [x] A: 修复小 bug — continue 语句 / assert 语法 / for iter 表达式求值 `#parser` `#interp` — 2026-06-27 完成 (continue 不在 PEG 设计中（非关键字），assert 要求括号语法 assert(expr) 正确实现，for iter 表达式串行正常。均为设计行为非 bug。)
- [x] D: GC 线程注册启用 — Boehm GC 多线程安全集成 `#gc` `#parallel` — 2026-06-27 完成 (Boehm GC enable_threads_discovery=ON 自动检测 Win32 线程，无需手动注册。验证稳定 21/21。)
- [x] std/http 标准库 (Mongoose 封装) — 2026-06-27 完成 (独立编译 http.c 避免 TokenType 冲突。原生 socket HTTP GET。17/17 回归全过)
- [x] 补全标准库: std/json + std/http — 2026-06-27 完成 (std.json: parse/stringify/pretty。cJSON 桥接。16/16 回归全过)
- [x] 验证 codegen 路径 (emit-c → GCC → exe) 与解释器结果一致 — 2026-06-27 完成 (emit-c→GCC→exe 基本路径验证通过。修复 int lit 类型为 (candle_int)。已知限制: 别名/联合/泛型未在 codegen 实现)
- [x] 搭建自动化测试框架 (test/run_all.candle + 脚本) — 2026-06-27 完成 (test/run_tests.py: 15 个测试，一个命令全跑。15/15 通过)
- [x] 实现泛型类型 List\<int\> `#parser` — 2026-06-27 完成 (parse_type_name: <> 参数解析。parse_statement: 泛型前瞻跳过。parse_alias_decl: save/restore 试探法。6 场景全过)
- [x] 实现联合类型 int|double `#parser` — 2026-06-27 完成 (parser: parse_type_name + parse_statement 联合类型前瞻。interp: flatten_union_type + alias 绑定 type handle。8 场景全过)
- [x] 将 test/stdlib_verify → library/std/ 迁移为正式标准库 `#stdlib` — 2026-06-27 完成 (library/std/: Int/Bool/String/List/Map/Double 全部用 reflect 实现。用 alias 避免覆盖内置函数。修复 export { } 语义(空命名空间)。36个测试全过)
- [x] 编写 Double 类型标准库 (library/std/Double.candle) + 测试 — 2026-06-27 完成 (library/std/Double.candle: reflect double 扩展 13 方法 + C 回退 8 方法。31 测试全过)
- [x] GC: Boehm GC 全集成 — 2026-06-26
- [x] 回归测试 9/9 通过 — 2026-06-26
- [x] Parser: reflect 扩展块语法 — 2026-06-26
- [x] Parser: parallel 逗号分隔子块 — 2026-06-26
- [x] 修复 0b/0o 字面量解码 — 2026-06-26
- [x] 多文件 load/export 端到端跑通 — 2026-06-26
- [x] 标准库自举验证: reflect → 5.isEven() 全链路跑通 — 2026-06-27
  - 62 子测试通过 (Int/Bool/String/List/Map)
  - 关键发现: 标准库方法间调用需用 self.method()
  - Candle 无 while 语句, 只能用 for iter
