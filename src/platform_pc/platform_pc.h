/* platform_pc.h - logging and small helpers shared by the PC HAL. */
#ifndef PEM_PLATFORM_PC_H
#define PEM_PLATFORM_PC_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PC_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
#define PC_PRINTF(f, a)
#endif

void pc_log(const char *fmt, ...) PC_PRINTF(1, 2);
void pc_warn(const char *fmt, ...) PC_PRINTF(1, 2);
_Noreturn void pc_fatal(const char *fmt, ...) PC_PRINTF(1, 2);

#ifdef __cplusplus
}
#endif

#endif /* PEM_PLATFORM_PC_H */
