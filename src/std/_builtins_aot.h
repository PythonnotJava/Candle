// _builtins_aot.h — AOT bridge for all C builtin modules
#ifndef CANDLE_BUILTINS_AOT_H
#define CANDLE_BUILTINS_AOT_H

#include "candle_runtime.h"

/* ===== std.math ===== */
#define math_File       ((candle_int)0)
candle_double candle_math_sqrt(candle_double x);
candle_double candle_math_sin(candle_double x);
candle_double candle_math_cos(candle_double x);
candle_double candle_math_tan(candle_double x);
candle_double candle_math_pow(candle_double x, candle_double y);
candle_double candle_math_log(candle_double x);
candle_double candle_math_log10(candle_double x);
candle_double candle_math_exp(candle_double x);
candle_double candle_math_floor(candle_double x);
candle_double candle_math_ceil(candle_double x);
candle_double candle_math_round(candle_double x);
candle_double candle_math_abs(candle_double x);
candle_double candle_math_asin(candle_double x);
candle_double candle_math_acos(candle_double x);
candle_double candle_math_atan(candle_double x);
candle_double candle_math_atan2(candle_double y, candle_double x);
candle_double candle_math_sinh(candle_double x);
candle_double candle_math_cosh(candle_double x);
candle_double candle_math_tanh(candle_double x);
candle_double candle_math_radians(candle_double deg);
candle_double candle_math_degrees(candle_double rad);
candle_double candle_math_cbrt(candle_double x);
candle_double candle_math_hypot(candle_double x, candle_double y);
candle_double candle_math_log2(candle_double x);
candle_double candle_math_trunc(candle_double x);
candle_int   candle_math_min(candle_double a, candle_double b);
candle_int   candle_math_max(candle_double a, candle_double b);
candle_int   candle_math_sign(candle_double x);
candle_int   candle_math_random(candle_int n);
candle_int   candle_math_randomRange(candle_int lo, candle_int hi);
candle_int   candle_math_seed(candle_int s);
extern const candle_double candle_math_PI;
#define math_PI  candle_math_PI
extern const candle_double candle_math_E;
#define math_E   candle_math_E

#define math_sqrt       candle_math_sqrt
#define math_sin        candle_math_sin
#define math_cos        candle_math_cos
#define math_tan        candle_math_tan
#define math_pow        candle_math_pow
#define math_log        candle_math_log
#define math_log10      candle_math_log10
#define math_exp        candle_math_exp
#define math_floor      candle_math_floor
#define math_ceil       candle_math_ceil
#define math_round      candle_math_round
#define math_abs        candle_math_abs
#define math_asin       candle_math_asin
#define math_acos       candle_math_acos
#define math_atan       candle_math_atan
#define math_atan2      candle_math_atan2
#define math_sinh       candle_math_sinh
#define math_cosh       candle_math_cosh
#define math_tanh       candle_math_tanh
#define math_radians    candle_math_radians
#define math_degrees    candle_math_degrees
#define math_cbrt       candle_math_cbrt
#define math_hypot      candle_math_hypot
#define math_log2       candle_math_log2
#define math_trunc      candle_math_trunc
#define math_min        candle_math_min
#define math_max        candle_math_max
#define math_sign       candle_math_sign
#define math_random     candle_math_random
#define math_randomRange candle_math_randomRange
#define math_seed       candle_math_seed
#define math_SQRT2      ((candle_double)1.41421356237309504880)
#define math_SQRT1_2    ((candle_double)0.70710678118654752440)
#define math_LN2        ((candle_double)0.69314718055994530942)
#define math_LN10       ((candle_double)2.30258509299404568402)

/* ===== std.io ===== */
candle_int candle_io_writeln(candle_string s);
#define io_writeln      candle_io_writeln
#define io_write        candle_io_writeln

/* ===== std.http ===== */
candle_string candle_http_get(candle_string url);
candle_string candle_http_post(candle_string url, candle_string body);
candle_string candle_http_postJson(candle_string url, candle_string json_body, candle_string auth_token);
candle_string candle_http_put(candle_string url, candle_string body, candle_string headers);
candle_string candle_http_delete(candle_string url, candle_string headers);
candle_string candle_http_request(candle_string method, candle_string url, candle_string body, candle_string headers);
candle_string candle_json_escape(candle_string s);
candle_string candle_json_get(candle_string json, candle_string key);
#define http_get        candle_http_get
#define http_post       candle_http_post
#define http_postJson   candle_http_postJson
#define http_put        candle_http_put
#define http_delete     candle_http_delete
#define http_request    candle_http_request
#define json_escape     candle_json_escape
#define json_get        candle_json_get

/* ===== std.time ===== */
candle_double candle_time_now(void);
candle_int    candle_time_sleep(candle_int ms);
#define time_now        candle_time_now
#define time_sleep      candle_time_sleep

/* ===== std.fs ===== */
candle_int    candle_fs_exists(candle_string path);
candle_int    candle_fs_isDir(candle_string path);
candle_int    candle_fs_isFile(candle_string path);
candle_int    candle_fs_fileSize(candle_string path);
candle_string candle_fs_readFile(candle_string path);
candle_int    candle_fs_writeFile(candle_string path, candle_string content);
candle_int    candle_fs_mkdir(candle_string path);
candle_int    candle_fs_delete(candle_string path);
candle_int    candle_fs_listDir(candle_string path);
#define fs_exists       candle_fs_exists
#define fs_isDir        candle_fs_isDir
#define fs_isFile       candle_fs_isFile
#define fs_fileSize     candle_fs_fileSize
#define fs_readFile     candle_fs_readFile
#define fs_writeFile    candle_fs_writeFile
#define fs_mkdir        candle_fs_mkdir
#define fs_delete       candle_fs_delete
#define fs_listDir      candle_fs_listDir

/* ===== std.json ===== */
candle_int    candle_json_parse(candle_string s);
candle_string candle_json_stringify(candle_int v);
candle_string candle_json_pretty(candle_int v);
#define json_parse      candle_json_parse
#define json_stringify  candle_json_stringify
#define json_pretty     candle_json_pretty

/* ===== std.file ===== */
#define file_File       ((candle_int)0)
candle_int    candle_file_open(candle_string path, candle_string mode);
candle_int    candle_file_close(candle_int handle);
candle_string candle_file_read(candle_int handle);
candle_string candle_file_readLine(candle_int handle);
candle_int    candle_file_readBytes(candle_int handle);
candle_int    candle_file_write(candle_int handle, candle_string s);
candle_int    candle_file_writeLine(candle_int handle, candle_string s);
candle_int    candle_file_writeBytes(candle_int handle, candle_int bytes_ptr);
candle_int    candle_file_seek(candle_int handle, candle_int pos);
candle_int    candle_file_tell(candle_int handle);
#define file_open       candle_file_open
#define file_close      candle_file_close
#define file_read       candle_file_read
#define file_readLine   candle_file_readLine
#define file_readBytes  candle_file_readBytes
#define file_write      candle_file_write
#define file_writeLine  candle_file_writeLine
#define file_writeBytes candle_file_writeBytes
#define file_seek       candle_file_seek
#define file_tell       candle_file_tell

/* ===== std.random ===== */
candle_int candle_random_randomInt(candle_int lo, candle_int hi);
candle_double candle_random_randomDouble(void);
candle_int candle_random_seed(candle_int s);
#define random_randomInt     candle_random_randomInt
#define random_randomDouble  candle_random_randomDouble
#define random_seed          candle_random_seed

/* ===== std.process ===== */
candle_int candle_process_exec(candle_string cmd);
candle_string candle_process_execCapture(candle_string cmd);
#define process_exec         candle_process_exec
#define process_execCapture  candle_process_execCapture

/* ===== std.path ===== */
candle_string candle_path_basename(candle_string p);
candle_string candle_path_dirname(candle_string p);
candle_string candle_path_extension(candle_string p);
candle_string candle_path_join(candle_string a, candle_string b);
candle_int    candle_path_isAbsolute(candle_string p);
#define path_basename   candle_path_basename
#define path_dirname    candle_path_dirname
#define path_extension  candle_path_extension
#define path_join       candle_path_join
#define path_isAbsolute candle_path_isAbsolute

/* ===== std.encoding ===== */
candle_int    candle_encoding_encode(candle_string s, candle_string enc);
candle_string candle_encoding_decode(candle_int bytes_ptr, candle_string enc);
candle_int    candle_encoding_hexEncode(candle_int bytes_ptr);
candle_int    candle_encoding_hexDecode(candle_string s);
candle_string candle_encoding_base64Encode(candle_string s);
candle_int    candle_encoding_base64Decode(candle_string s);
#define encoding_encode         candle_encoding_encode
#define encoding_decode         candle_encoding_decode
#define encoding_hexEncode      candle_encoding_hexEncode
#define encoding_hexDecode      candle_encoding_hexDecode
#define encoding_base64Encode   candle_encoding_base64Encode
#define encoding_base64Decode   candle_encoding_base64Decode

/* ===== stubs for less common modules ===== */
#define ffi_alloc       ((candle_int)0)
#define ffi_free(x)     ((void)0)
#define datetime_now    ((candle_int)0)
#define collections_Queue_new() ((candle_int)0)
#define crypto_sha256(s) candle_str("")
#define crypto_uuid4()   candle_str("")

#endif
