// 声明（实现在独立 .c 文件中以避免 TokenType 冲突）
extern Value build_http(void);
extern Value build_time(void);
extern Value build_fs(void);
extern Value build_file_map(Value file_class);
extern Value build_random(void);
extern Value build_process(void);
extern Value build_path(void);
extern Value build_encoding(void);