# Candle 使用标准

- 可执行的Candle文件下必须有main函数作为入口
- 语言设计、API设计风格遵循Dart语言的
- 语言实现路线：AST→C→GCC

.candle 源码
                         │
          ┌──────────────┴──────────────┐
          │                             │
     ① 解释执行                    ② AOT 编译
    candlec --run                  candlec -c
          │                             │
    树遍历解释器                  转译 → C 源码
    直接运算每一行                ↓
          │                      GCC 编译
          │                        ↓
          │                      .exe 可执行文件
          │                        ↓
          │                     直接运行
          │
    结果输出
