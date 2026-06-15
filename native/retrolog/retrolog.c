// retrolog — native bridge for the libretro variadic log callback.
//
// retro_log_printf_t is variadic: void (*)(enum retro_log_level, const char *fmt, ...). A managed
// (C#) delegate can't portably read C varargs — on arm64 (AAPCS) they're passed on the stack, not
// in the integer registers a fixed managed signature captures, so "%s" args are garbage and crash
// or get dropped. The correct fix is to let C format the message: the core calls this native
// variadic function, we vsnprintf it, and hand the finished string to a non-variadic managed sink.
// Cross-platform and ABI-correct.
#include <stdarg.h>
#include <stdio.h>

typedef void (*retrolog_sink_t)(int level, const char *msg);
static retrolog_sink_t g_sink = 0;

// Register the managed sink that receives fully-formatted messages. Pass 0 to detach.
void retrolog_set_sink(retrolog_sink_t sink) { g_sink = sink; }

// The variadic callback handed to the core as its retro_log_printf_t. Formats and forwards.
static void retrolog_vlog(int level, const char *fmt, ...)
{
    if (!g_sink || !fmt) return;
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_sink(level, buf);
}

// Function pointer the core should store as its retro_log_printf_t.
void *retrolog_get_callback(void) { return (void *)retrolog_vlog; }
