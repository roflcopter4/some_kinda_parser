#include "util.h"
#include <sys/stat.h>

#define STARTSIZE 1024
#define GUESS 100
#define INC   10

#ifdef DOSISH
#  define restrict __restrict
   char program_invocation_short_name[2048];
   __attribute__((__constructor__)) void setpisn(void)
  {
          strcpy(program_invocation_short_name, "x4c");
  }
#endif

#define SAFE_STAT(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)

#ifdef HAVE_EXECINFO_H
#  include <execinfo.h>
#  define SHOW_STACKTRACE()                        \
        __extension__({                            \
                void * arr[128];                   \
                size_t num = backtrace(arr, 128);  \
                fflush(stderr);                    \
                dprintf(2, "STACKTRACE: \n");      \
                backtrace_symbols_fd(arr, num, 2); \
        })
#  define FATAL_ERROR(...)                                         \
        __extension__({                                            \
                void * arr[128];                                   \
                char   buf[8192];                                  \
                size_t num = backtrace(arr, 128);                  \
                snprintf(buf, 8192, __VA_ARGS__);                  \
                                                                   \
                warnx("Fatal error in func %s in %s, line "        \
                      "%d\n%sSTACKTRACE: ",                        \
                      FUNC_NAME, __FILE__, __LINE__, buf);         \
                fflush(stderr);                                    \
                backtrace_symbols_fd(arr, num, 2);                 \
                abort();                                           \
        })
#else
#  define FATAL_ERROR(...)                                    \
        do {                                                \
                warnx("Fatal error in func %s in %s, line " \
                      "%d\n%sSTACKTRACE: ",                 \
                      FUNC_NAME, __FILE__, __LINE__, buf);  \
                fflush(stderr);                             \
        } while (0)
#  define SHOW_STACKTRACE(...)
#endif

static bool file_is_reg(const char *filename);

FILE *
safe_fopen(const char *filename, const char *mode)
{
        FILE *fp = fopen(filename, mode);
        if (!fp)
                err(1, "Failed to open file \"%s\"", filename);
        if (!file_is_reg(filename))
                errx(1, "Invalid filetype \"%s\"\n", filename);
        return fp;
}

FILE *
safe_fopen_fmt(const char *const restrict fmt,
               const char *const restrict mode,
               ...)
{
        va_list va;
        va_start(va, mode);
        char buf[SAFE_PATH_MAX + 1];
        vsnprintf(buf, SAFE_PATH_MAX + 1, fmt, va);
        va_end(va);

        FILE *fp = fopen(buf, mode);
        if (!fp)
                err(1, "Failed to open file \"%s\"", buf);
        if (!file_is_reg(buf))
                errx(1, "Invalid filetype \"%s\"\n", buf);

        return fp;
}

int
safe_open(const char *const filename, const int flags, const int mode)
{
#ifdef DOSISH
        const int fd = open(filename, flags, _S_IREAD|_S_IWRITE);
#else
        const int fd = open(filename, flags, mode);
#endif
        //if (fd == (-1))
        //        err(1, "Failed to open file '%s'", filename);
        if (fd == (-1)) {
                fprintf(stderr, "Failed to open file \"%s\": %s\n", filename, strerror(errno));
                abort();
        }
        return fd;
}

int
safe_open_fmt(const char *const restrict fmt,
              const int flags, const int mode, ...)
{
        va_list va;
        va_start(va, mode);
        char buf[SAFE_PATH_MAX + 1];
        vsnprintf(buf, SAFE_PATH_MAX + 1, fmt, va);
        va_end(va);

        errno = 0;
#ifdef DOSISH
        const int fd = open(buf, flags, _S_IREAD|_S_IWRITE);
#else
        const int fd = open(buf, flags, mode);
#endif
        if (fd == (-1)) {
                fprintf(stderr, "Failed to open file \"%s\": %s\n", buf, strerror(errno));
                abort();
        }

        return fd;
}

bool
file_is_reg(const char *filename)
{
        struct stat st;
        SAFE_STAT(filename, &st);
        return S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode);
}

int64_t
xatoi__(const char *const str, const bool strict)
{
        char         *endptr;
        const int64_t val = strtoll(str, &endptr, 10);

        if ((endptr == str) || (strict && *endptr != '\0'))
                errx(30, "Invalid integer \"%s\".\n", str);

        return val;
}

#ifdef DOSISH
char *
basename(char *path)
{
        assert(path != NULL && *path != '\0');
        const size_t len = strlen(path);
        char *ptr = path + len;
        while (*ptr != '/' && *ptr != '\\' && ptr != path)
                --ptr;
        
        return (*ptr == '/' || *ptr == '\\') ? ptr + 1 : ptr;
}
#endif

#define ERRSTACKSIZE (6384)
void
err_(UNUSED const int status, const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        vfprintf(stderr, buf, ap);
        va_end(ap);

        SHOW_STACKTRACE();

        exit(status);
}

extern FILE *echo_log;
void
warn_(const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap1, ap2;
        va_start(ap1, fmt);
        va_start(ap2, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        vfprintf(stderr, buf, ap1);

/* #ifdef DEBUG
        vfprintf(echo_log, buf, ap2);
        fflush(echo_log);
#endif */

        va_end(ap1);
        va_end(ap2);
}

unsigned
find_num_cpus(void)
{
#if defined(DOSISH)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif defined(MACOS)
        int nm[2];
        size_t len = 4;
        uint32_t count;

        nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);

        if (count < 1) {
                nm[1] = HW_NCPU;
                sysctl(nm, 2, &count, &len, NULL, 0);
                if (count < 1) { count = 1; }
        }
        return count;
#elif defined(__unix__) || defined(__linux__) || defined(BSD)
        return sysconf(_SC_NPROCESSORS_ONLN);
#else
#  error "Cannot determine operating system."
#endif
}


#if defined(__GNUC__) && !defined(__clang__) && !defined(__cplusplus)
const char *
ret_func_name__(const char *const function, const size_t size)
{
        if (size + 2 > 256)
                return function;
        static thread_local char buf[256];
        memcpy(buf, function, size - 1);
        buf[size]   = '(';
        buf[size+1] = ')';
        buf[size+2] = '\0';
        return buf;
}
#endif

/*============================================================================*/
/* List operations */

void
free_all__(void *ptr, ...)
{
        va_list ap;
        va_start(ap, ptr);

        do xfree(ptr);
        while ((ptr = va_arg(ap, void *)) != NULL);

        va_end(ap);
}

#define READ_FD  (0)
#define WRITE_FD (1)

#if defined HAVE_FORK
#  include <wait.h>

bstring *
get_command_output(const char *command, char *const *const argv, bstring *input)
{
        int fds[2][2], pid, status;

#ifdef HAVE_PIPE2
        if (pipe2(fds[0], O_CLOEXEC) == (-1) || pipe2(fds[1], O_CLOEXEC) == (-1)) 
                err(1, "pipe() failed\n");
#else
        if (pipe(fds[0]) == (-1) || pipe(fds[1]) == (-1)) 
                err(1, "pipe() failed\n");
        fcntl(fds[0][0], F_SETFL, O_CLOEXEC);
        fcntl(fds[0][1], F_SETFL, O_CLOEXEC);
        fcntl(fds[1][0], F_SETFL, O_CLOEXEC);
        fcntl(fds[1][1], F_SETFL, O_CLOEXEC);
#endif
        if ((pid = fork()) == 0) {
                if (dup2(fds[0][READ_FD], 0) == (-1) || dup2(fds[1][WRITE_FD], 1) == (-1))
                        err(1, "dup2() failed\n");
                if (execvp(command, argv) == (-1))
                        err(1, "exec() failed\n");
        }

        if (input)
                b_write(fds[0][WRITE_FD], input);
        close(fds[0][READ_FD]);
        close(fds[1][WRITE_FD]);
        close(fds[0][WRITE_FD]);
        waitpid(pid, &status, 0);
        bstring *rd = b_read_fd(fds[1][READ_FD]);
        close(fds[1][READ_FD]);

        if ((status <<= 8) != 0)
                warnx("Command failed with status %d\n", status);
        return rd;
}
#elif defined DOSISH
#  define READ_BUFSIZE (8192LLU << 2)

static bstring    *ReadFromPipe(HANDLE han);
static inline void ErrorExit(const char *msg, DWORD dw);

bstring *
get_command_output(UNUSED const char *command, char *const*const argv, bstring *input)
{
        bstring *commandline = b_fromcstr("");

        for (char **s = (char **)argv; *s; ++s) {
                b_catchar(commandline, '"');
                b_catcstr(commandline, *s);
                b_catlit(commandline, "\" ");
        }

        bstring *ret = _win32_get_command_output(BS(commandline), input);
        b_destroy(commandline);
        return ret;
}

bstring *
_win32_get_command_output(char *argv, bstring *input)
{
        HANDLE              handles[2][2];
        DWORD               status, written;
        STARTUPINFOA        info;
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES attr = {sizeof(attr), NULL, true};

        if (!CreatePipe(&handles[0][0], &handles[0][1], &attr, 0)) 
                ErrorExit("CreatePipe()", GetLastError());
        if (!CreatePipe(&handles[1][0], &handles[1][1], &attr, 0)) 
                ErrorExit("CreatePipe()", GetLastError());
        if (!SetHandleInformation(handles[0][WRITE_FD], HANDLE_FLAG_INHERIT, 0))
                ErrorExit("Stdin SetHandleInformation", GetLastError());
        if (!SetHandleInformation(handles[1][READ_FD], HANDLE_FLAG_INHERIT, 0))
                ErrorExit("Stdout SetHandleInformation", GetLastError());

        memset(&info, 0, sizeof(info));
        memset(&pi, 0, sizeof(pi));
        info = (STARTUPINFOA){
            .cb         = sizeof(info),
            .dwFlags    = STARTF_USESTDHANDLES,
            .hStdInput  = handles[0][READ_FD],
            .hStdOutput = handles[1][WRITE_FD],
            .hStdError  = GetStdHandle(STD_ERROR_HANDLE),
        };

        if (!CreateProcessA(NULL, argv, NULL, NULL, true, 0, NULL, NULL, &info, &pi))
                ErrorExit("CreateProcess() failed", GetLastError());
        CloseHandle(handles[0][READ_FD]);
        CloseHandle(handles[1][WRITE_FD]);

        if (!WriteFile(handles[0][WRITE_FD], input->data,
                       input->slen, &written, NULL) || written != input->slen)
                ErrorExit("WriteFile()", GetLastError());
        CloseHandle(handles[0][WRITE_FD]);
        
        bstring *ret = ReadFromPipe(handles[1][READ_FD]);
        CloseHandle(handles[1][READ_FD]);

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &status);
        if (status != 0)
                SHOUT("Command failed with status %ld\n", status);
        
        return ret;
}

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data. 
static bstring *
ReadFromPipe(HANDLE han)
{
        DWORD    dwRead;
        CHAR     chBuf[READ_BUFSIZE];
        bstring *ret = b_alloc_null(READ_BUFSIZE);

        for (;;) {
                bool bSuccess = ReadFile(han, chBuf, READ_BUFSIZE, &dwRead, NULL);
                if (dwRead)
                        b_catblk(ret, chBuf, (unsigned)dwRead);
                if (!bSuccess || dwRead == 0)
                        break;
        }

        return ret;
} 

static inline void
ErrorExit(const char *msg, DWORD dw)
{
        char *lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
        SHOUT("%s: Error: %s: %s\n", program_invocation_short_name, msg, lpMsgBuf);
        fflush(stderr);
        LocalFree(lpMsgBuf);
        abort();
}
#else
#  error "Impossible operating system detected. OS/2? System/360? Maybe DOS?"
#endif
