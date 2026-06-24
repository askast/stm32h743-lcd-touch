/*
 * Minimal newlib syscall stubs. The important one is _write(), which retargets
 * stdout/stderr (and therefore printf) to USART1. The rest are stubs so the C
 * library links cleanly; _sbrk() backs malloc()/printf buffering.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "uart.h"

#undef errno
extern int errno;

int _write(int fd, const char *buf, int len)
{
    (void)fd;
    uart_write(buf, (size_t)len);
    return len;
}

caddr_t _sbrk(int incr)
{
    extern char _end;        /* set by the linker, just past .bss */
    static char *heap = 0;
    char *prev;

    if (heap == 0) {
        heap = &_end;
    }
    prev = heap;
    heap += incr;
    return (caddr_t)prev;
}

int _read(int fd, char *buf, int len)   { (void)fd; (void)buf; (void)len; return 0; }
int _close(int fd)                      { (void)fd; return -1; }
int _lseek(int fd, int ptr, int dir)    { (void)fd; (void)ptr; (void)dir; return 0; }
int _isatty(int fd)                     { (void)fd; return 1; }

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _kill(int pid, int sig)             { (void)pid; (void)sig; errno = EINVAL; return -1; }
int _getpid(void)                       { return 1; }
void _exit(int code)                    { (void)code; for (;;) { } }
