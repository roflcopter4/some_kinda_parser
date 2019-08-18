#include "Common.h"

#include "util/list.h"

#define RUNTIME_ERROR() \
        do { warnx("Error in %s, func %s, line %d", __FILE__, __func__, __LINE__); abort(); } while (0)
#define RETURN_NULL() abort();


static unsigned
snapUpSize(unsigned i)
{
        if (i < 8) {
                i = 8;
        } else {
                unsigned j = i;
                j |= (j >> 1);
                j |= (j >> 2);
                j |= (j >> 4);
                j |= (j >> 8); /* Ok, since int >= 16 bits */
#if (UINT_MAX != 0xFFFF)
                j |= (j >> 16); /* For 32 bit int systems */
#  if (UINT_MAX > 0xFFFFFFFFllu)
                j |= (j >> 32); /* For 64 bit int systems */
#  endif
#endif
                /* Least power of two greater than i */
                j++;
                if (j >= i)
                        i = j;
        }

        return i;
}

#define RWLOCK_LOCK(GENLIST)                                   \
        __extension__({                                        \
                genlist * list_ = (GENLIST);                   \
                pthread_t self_ = pthread_self();              \
                if (!pthread_equal(self_, list_->whodunnit)) { \
                        pthread_mutex_lock(&list_->lock);      \
                        list_->whodunnit = self_;              \
                }                                              \
        })
#define mutex_unlock(GENLIST)                       \
        __extension__({                             \
                genlist *list_ = (GENLIST);         \
                pthread_mutex_unlock(&list_->lock); \
                list_->whodunnit = 0;               \
        })
        
genlist *
genlist_create(void *talloc_ctx)
{
        genlist *list = talloc(talloc_ctx, genlist);
        if (!list)
                abort();
        list->lst     = talloc_array(list, void *, 2);
        if (!list->lst)
                abort();
        list->qty     = 0;
        list->mlen    = 2;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&list->mut, &attr);

        return list;
}

genlist *
genlist_create_alloc(const unsigned msz)
{
        const unsigned size = (msz <= 2) ? 2 : msz;
        genlist *list = talloc(NULL, genlist);
        list->lst     = talloc_array(list, void *, size);
        list->qty     = 0;
        list->mlen    = size;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&list->mut, &attr);

        return list;
}


int
genlist_destroy(genlist *list)
{
        if (!list)
                return (-1);
        pthread_mutex_lock(&list->mut);

        pthread_mutex_t mut;
        memcpy(&mut, &list->mut, sizeof(pthread_mutex_t));

        talloc_free(list);
        pthread_mutex_unlock(&mut);
        return 0;
}


int
genlist_alloc(genlist *list, const unsigned msz)
{
        if (!list || msz == 0 || !list->lst || list->mlen == 0 || list->qty > list->mlen)
                RUNTIME_ERROR();
        if (list->mlen >= msz)
                return 0;
        pthread_mutex_lock(&list->mut);

        void     **ptr;
        unsigned   smsz = snapUpSize(msz);
        size_t     nsz  = ((size_t)smsz) * sizeof(void *);

        if (nsz < (size_t)smsz) {
                pthread_mutex_unlock(&list->mut);
                RUNTIME_ERROR();
        }

        ptr = talloc_realloc(NULL, list->lst, void *, nsz);

        list->mlen = smsz;
        list->lst  = ptr;

        pthread_mutex_unlock(&list->mut);
        return 0;
}

int
genlist_append(genlist *list, void *item)
{
        if (!list || !list->lst)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        if (list->qty == (list->mlen - 1)) {
                void **ptr = talloc_realloc(NULL, list->lst, void *, (list->mlen *= 2));
                list->lst  = ptr;
        }

                list->lst[list->qty++] = item;
                talloc_steal(list->lst, item);

        pthread_mutex_unlock(&list->mut);
        return 0;
}

int 
genlist_remove(genlist *list, const void *obj)
{
        if (!list || !list->lst || !obj)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        int ret = (-1);

        for (unsigned i = 0; i < list->qty; ++i) {
                if (list->lst[i] == obj) {
                        xfree(list->lst[i]);
                        list->lst[i] = NULL;

                        if (i == list->qty - 1)
                                --list->qty;
                        else
                                memmove(list->lst + i, list->lst + i + 1, --list->qty - i);
                        ret = 0;
                        break;
                }
        }

        pthread_mutex_unlock(&list->mut);
        return ret;
}

int
genlist_remove_index(genlist *list, const unsigned index)
{
        if (!list || !list->lst || index >= list->qty)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        xfree(list->lst[index]);
        list->lst[index] = NULL;

        if (index == list->qty - 1)
                --list->qty;
        else
                memmove(list->lst + index, list->lst + index + 1, --list->qty - index);

        pthread_mutex_unlock(&list->mut);
        return 0;
}

genlist *
genlist_copy(genlist *list, const genlist_copy_func cpy)
{
        if (!list || !list->lst)
                RETURN_NULL();
        pthread_mutex_lock(&list->mut);

        genlist *ret = genlist_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                cpy(&ret->lst[ret->qty], list->lst[i]);
                ++ret->qty;
        }

        pthread_mutex_unlock(&list->mut);
        return ret;
}

void *
genlist_pop(genlist *list)
{
        if (!list || !list->lst || !list->qty)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        void *ret            = list->lst[--list->qty];
        list->lst[list->qty] = NULL;

        pthread_mutex_unlock(&list->mut);
        return ret;
}

void *
genlist_dequeue(genlist *list)
{
        if (!list || !list->lst || !list->qty)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        void *ret    = list->lst[0];
        list->lst[0] = NULL;

        if (list->qty > 1)
                memmove(list->lst, list->lst + 1, list->qty - 1);
        --list->qty;

        pthread_mutex_unlock(&list->mut);
        return ret;
}


/*======================================================================================*/


struct argument_vector *
argv_create(unsigned len)
{
        len = (len) ? len : 1;

        struct argument_vector *argv = xmalloc(sizeof(struct argument_vector));
        argv->lst                    = nmalloc(len, sizeof(char *));
        argv->lst[0]                 = NULL;
        argv->qty                    = 0;
        argv->mlen                   = len;

        return argv;
}


void
argv_append(struct argument_vector *argv, const char *str, const bool cpy)
{
        if (argv->qty == (argv->mlen - 1)) {
                auto_type tmp = nrealloc(argv->lst, (argv->mlen *= 2), sizeof(char *));
                argv->lst = tmp;
        }

        if (cpy)
                argv->lst[argv->qty++] = strdup(str);
        else
                argv->lst[argv->qty++] = (char *)str;
}


void
argv_fmt(struct argument_vector *argv, const char *const restrict fmt, ...)
{
        char *_buf = NULL;
        va_list _ap;
        va_start(_ap, fmt);

#ifdef HAVE_VASPRINTF
        if (vasprintf(&_buf, fmt, _ap) == (-1))
                err(1, "vasprintf failed");
#else
        bstring *tmp = b_vformat(fmt, _ap);
        _buf         = BS(tmp);
        xfree(tmp);
#endif

        va_end(_ap);
        argv_append(argv, _buf, false);
}


void
argv_destroy(struct argument_vector *argv)
{
        for (unsigned i = 0; i < argv->qty; ++i)
                if (argv->lst[i])
                        xfree(argv->lst[i]);
        xfree(argv->lst);
        xfree(argv);
}


void
argv_dump__(FILE *fp,
            const struct argument_vector *const restrict argv,
            const char *const restrict listname,
            const char *const restrict file,
            const int line)
{
        fprintf(fp, "Dumping list \"%s\" (%s at %d)\n", listname, file, line);
        for (unsigned i = 0; i < argv->qty; ++i)
                fprintf(fp, "%s\n", argv->lst[i]);
        fputc('\n', fp);
}


void
argv_dump_fd__(const int fd,
               const struct argument_vector *const restrict argv,
               const char *const restrict listname,
               const char *const restrict file,
               const int line)
{
        dprintf(fd, "Dumping list \"%s\" (%s at %d)\n", listname, file, line);
        for (unsigned i = 0; i < argv->qty; ++i)
                dprintf(fd, "%s\n", argv->lst[i]);
        if (write(fd, "\n", 1) != 1)
                err(1, "write");
}
