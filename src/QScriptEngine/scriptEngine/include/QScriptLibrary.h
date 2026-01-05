// 此文件修改自官方的 "quickjs-libc.h"
#ifndef QSCRIPTENGINE_QSCRIPTLIBRARY_H
#define QSCRIPTENGINE_QSCRIPTLIBRARY_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define JS_EXTERN __attribute__((visibility("default")))
#else
#define JS_EXTERN /* nothing */
#endif

JS_EXTERN JSModuleDef *js_init_module_std(JSContext *ctx,
                                          const char *module_name);
JS_EXTERN JSModuleDef *js_init_module_os(JSContext *ctx,
                                         const char *module_name);
JS_EXTERN JSModuleDef *js_init_module_bjson(JSContext *ctx,
                                            const char *module_name);
JS_EXTERN void js_std_add_helpers(JSContext *ctx, int argc, char **argv);
JS_EXTERN int js_std_loop(JSContext *ctx);
JS_EXTERN JSValue js_std_await(JSContext *ctx, JSValue obj);
JS_EXTERN void js_std_init_handlers(JSRuntime *rt);
JS_EXTERN void js_std_free_handlers(JSRuntime *rt);
JS_EXTERN void js_std_dump_error(JSContext *ctx);
JS_EXTERN uint8_t *js_load_file(JSContext *ctx, size_t *pbuf_len,
                                const char *filename);
JS_EXTERN int js_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
                                        bool use_realpath, bool is_main);
JS_EXTERN JSModuleDef *js_module_loader(JSContext *ctx,
                                        const char *module_name, void *opaque);
JS_EXTERN void js_std_eval_binary(JSContext *ctx, const uint8_t *buf,
                                  size_t buf_len, int flags);
JS_EXTERN void js_std_promise_rejection_tracker(JSContext *ctx,
                                                JSValueConst promise,
                                                JSValueConst reason,
                                                bool is_handled,
                                                void *opaque);
// Defaults to JS_NewRuntime, no-op if compiled without worker support.
// Call before creating the first worker thread.
JS_EXTERN void js_std_set_worker_new_runtime_func(JSRuntime *(*func)(void));
// Defaults to JS_NewContext, no-op if compiled without worker support.
// Call before creating the first worker thread.
JS_EXTERN void js_std_set_worker_new_context_func(JSContext *(*func)(JSRuntime *rt));

#undef JS_EXTERN

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif // QSCRIPTENGINE_QSCRIPTLIBRARY_H
