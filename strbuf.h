#ifndef STRBUF_H
#define STRBUF_H

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#define REALLOC_ARRAY(x, alloc) (x) = realloc((x), (alloc) * sizeof(*(x)))
#define alloc_nr(x) (((x) + 16) * 3 / 2)
#define ALLOC_GROW(x, nr, alloc)                                               \
    do {                                                                       \
        if ((nr) > alloc) {                                                    \
            if (alloc_nr(alloc) < (nr))                                        \
                alloc = (nr);                                                  \
            else                                                               \
                alloc = alloc_nr(alloc);                                       \
            REALLOC_ARRAY(x, alloc);                                           \
        }                                                                      \
    } while (0)

#ifndef die
#define die(...)                                                          \
    do {                                                                       \
        fprintf(stderr, "fatal");                                              \
        fprintf(stderr, ": ");                                                 \
        fprintf(stderr, __VA_ARGS__);                                     \
        exit(1);                                                               \
    } while (0)
#endif

/**
 * This is the string buffer structure. The `len` member can be used to
 * determine the current length of the string, and `buf` member provides
 * access to the string itself.
 */
struct strbuf {
    size_t alloc;
    size_t len;
    char *buf;
};

extern char strbuf_slopbuf[];
#define STRBUF_INIT                                                            \
    {                                                                          \
        0, 0, strbuf_slopbuf                                                   \
    }

/**
 * Life Cycle Functions
 * --------------------
 */

/**
 * Initialize the structure. The second parameter can be zero or a bigger
 * number to allocate memory, in case you want to prevent further reallocs.
 */
extern void strbuf_init(struct strbuf *, size_t);

/**
 * Release a string buffer and the memory it used. You should not use the
 * string buffer after using this function, unless you initialize it again.
 */
extern void strbuf_release(struct strbuf *);

/**
 * Detach the string from the strbuf and returns it; you now own the
 * storage the string occupies and it is your responsibility from then on
 * to release it with `free(3)` when you are done with it.
 */
extern char *strbuf_detach(struct strbuf *, size_t *);

/**
 * Attach a string to a buffer. You should specify the string to attach,
 * the current length of the string and the amount of allocated memory.
 * The amount must be larger than the string length, because the string you
 * pass is supposed to be a NUL-terminated string.  This string _must_ be
 * malloc()ed, and after attaching, the pointer cannot be relied upon
 * anymore, and neither be free()d directly.
 */
extern void strbuf_attach(struct strbuf *, void *, size_t, size_t);

/**
 * Swap the contents of two string buffers.
 */
static inline void strbuf_swap(struct strbuf *a, struct strbuf *b)
{
    struct strbuf tmp = *a;
    *a = *b;
    *b = tmp;
}

/**
 * Functions related to the size of the buffer
 * -------------------------------------------
 */

/**
 * Determine the amount of allocated but unused memory.
 */
static inline size_t strbuf_avail(const struct strbuf *sb)
{
    return sb->alloc ? sb->alloc - sb->len - 1 : 0;
}

/**
 * Ensure that at least this amount of unused memory is available after
 * `len`. This is used when you know a typical size for what you will add
 * and want to avoid repetitive automatic resizing of the underlying buffer.
 * This is never a needed operation, but can be critical for performance in
 * some cases.
 */
extern void strbuf_grow(struct strbuf *, size_t);

/**
 * Set the length of the buffer to a given value. This function does *not*
 * allocate new memory, so you should not perform a `strbuf_setlen()` to a
 * length that is larger than `len + strbuf_avail()`. `strbuf_setlen()` is
 * just meant as a 'please fix invariants from this strbuf I just messed
 * with'.
 */
static inline int strbuf_setlen(struct strbuf *sb, size_t len)
{
    if (len > (sb->alloc ? sb->alloc - 1 : 0))
        return -1;
    sb->len = len;
    sb->buf[len] = '\0';
    return 0;
}

/**
 * Empty the buffer by setting the size of it to zero.
 */
#define strbuf_reset(sb) strbuf_setlen(sb, 0)

/**
 * Functions related to the contents of the buffer
 * -----------------------------------------------
 */

/**
 * Strip whitespace from the beginning (`ltrim`), end (`rtrim`), or both side
 * (`trim`) of a string.
 */
extern void strbuf_trim(struct strbuf *);
extern void strbuf_rtrim(struct strbuf *);
extern void strbuf_ltrim(struct strbuf *);

/**
 * Lowercase each character in the buffer using `tolower`.
 */
extern void strbuf_tolower(struct strbuf *sb);

/**
 * Compare two buffers. Returns an integer less than, equal to, or greater
 * than zero if the first buffer is found, respectively, to be less than,
 * to match, or be greater than the second buffer.
 */
extern int strbuf_cmp(const struct strbuf *, const struct strbuf *);

/**
 * Adding data to the buffer
 * -------------------------
 *
 * NOTE: All of the functions in this section will grow the buffer as
 * necessary.  If they fail for some reason other than memory shortage and the
 * buffer hadn't been allocated before (i.e. the `struct strbuf` was set to
 * `STRBUF_INIT`), then they will free() it.
 */

/**
 * Add a single character to the buffer.
 */
static inline void strbuf_addch(struct strbuf *sb, int c)
{
    if (!strbuf_avail(sb)) strbuf_grow(sb, 1);
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

/**
 * Add a character the specified number of times to the buffer.
 */
extern void strbuf_addchars(struct strbuf *sb, int c, size_t n);

/**
 * Insert data to the given position of the buffer. The remaining contents
 * will be shifted, not overwritten.
 */
extern void strbuf_insert(struct strbuf *, size_t pos, const void *, size_t);

static inline void strbuf_setstr(struct strbuf *sb, const char *str)
{
    strbuf_insert(sb, 0, str, strlen(str));
}

/**
 * Remove given amount of data from a given position of the buffer.
 */
extern void strbuf_remove(struct strbuf *, size_t pos, size_t len);

/**
 * Remove the bytes between `pos..pos+len` and replace it with the given
 * data.
 */
extern void strbuf_splice(struct strbuf *, size_t pos, size_t len, const void *,
                          size_t);

/**
 * Add data of given length to the buffer.
 */
extern void strbuf_add(struct strbuf *, const void *, size_t);

/**
 * Add a NUL-terminated string to the buffer.
 *
 * NOTE: This function will *always* be implemented as an inline or a macro
 * using strlen, meaning that this is efficient to write things like:
 *
 *     strbuf_addstr(sb, "immediate string");
 *
 */
static inline void strbuf_addstr(struct strbuf *sb, const char *s)
{
    strbuf_add(sb, s, strlen(s));
}

/**
 * Copy the contents of another buffer at the end of the current one.
 */
static inline void strbuf_addbuf(struct strbuf *sb, const struct strbuf *sb2)
{
    strbuf_grow(sb, sb2->len);
    strbuf_add(sb, sb2->buf, sb2->len);
}

/**
 * Copy part of the buffer from a given position till a given length to the
 * end of the buffer.
 */
extern void strbuf_adddup(struct strbuf *sb, size_t pos, size_t len);

/**
 * Read a given size of data from a FILE* pointer to the buffer.
 *
 * NOTE: The buffer is rewound if the read fails. If -1 is returned,
 * `errno` must be consulted, like you would do for `read(3)`.
 * `strbuf_read()`, `strbuf_read_file()` and `strbuf_getline()` has the
 * same behaviour as well.
 */
extern size_t strbuf_fread(struct strbuf *, size_t, FILE *);

/**
 * Read the contents of a given file descriptor. The third argument can be
 * used to give a hint about the file size, to avoid reallocs.  If read fails,
 * any partial read is undone.
 */
extern ssize_t strbuf_read(struct strbuf *, int fd, size_t hint);

/**
 * Read the contents of a given file descriptor partially by using only one
 * attempt of xread. The third argument can be used to give a hint about the
 * file size, to avoid reallocs. Returns the number of new bytes appended to
 * the sb.
 */
extern ssize_t strbuf_read_once(struct strbuf *, int fd, size_t hint);

/**
 * Read the contents of a file, specified by its path. The third argument
 * can be used to give a hint about the file size, to avoid reallocs.
 */
extern ssize_t strbuf_read_file(struct strbuf *sb, const char *path,
                                size_t hint);

/**
 * Read a line from a FILE *, overwriting the existing contents
 * of the strbuf. The second argument specifies the line
 * terminator character, typically `'\n'`.
 * Reading stops after the terminator or at EOF.  The terminator
 * is removed from the buffer before returning.  Returns 0 unless
 * there was nothing left before EOF, in which case it returns `EOF`.
 */
extern int strbuf_getline(struct strbuf *, FILE *, int);

/**
 * Like `strbuf_getline`, but keeps the trailing terminator (if
 * any) in the buffer.
 */
extern int strbuf_getwholeline(struct strbuf *, FILE *, int);

/**
 * Like `strbuf_getwholeline`, but operates on a file descriptor.
 * It reads one character at a time, so it is very slow.  Do not
 * use it unless you need the correct position in the file
 * descriptor.
 */
extern int strbuf_getwholeline_fd(struct strbuf *, int, int);

/**
 * Set the buffer to the path of the current working directory.
 */
extern int strbuf_getcwd(struct strbuf *sb);

/**
 * Add a path to a buffer, converting a relative path to an
 * absolute one in the process.  Symbolic links are not
 * resolved.
 */
extern void strbuf_add_absolute_path(struct strbuf *sb, const char *path);

/**
 * Split str (of length slen) at the specified terminator character.
 * Return a null-terminated array of pointers to strbuf objects
 * holding the substrings.  The substrings include the terminator,
 * except for the last substring, which might be unterminated if the
 * original string did not end with a terminator.  If max is positive,
 * then split the string into at most max substrings (with the last
 * substring containing everything following the (max-1)th terminator
 * character).
 *
 * The most generic form is `strbuf_split_buf`, which takes an arbitrary
 * pointer/len buffer. The `_str` variant takes a NUL-terminated string,
 * the `_max` variant takes a strbuf, and just `strbuf_split` is a convenience
 * wrapper to drop the `max` parameter.
 */
extern struct strbuf **strbuf_split_buf(const char *, size_t,
                    int terminator, int max);

static inline struct strbuf **strbuf_split_str(const char *str,
                           int terminator, int max)
{
    return strbuf_split_buf(str, strlen(str), terminator, max);
}

static inline struct strbuf **strbuf_split_max(const struct strbuf *sb,
                        int terminator, int max)
{
    return strbuf_split_buf(sb->buf, sb->len, terminator, max);
}

static inline struct strbuf **strbuf_split(const struct strbuf *sb,
                       int terminator)
{
    return strbuf_split_max(sb, terminator, 0);
}


/**
 * Free a NULL-terminated list of strbufs (for example, the return
 * values of the strbuf_split*() functions).
 */
extern void strbuf_list_free(struct strbuf **);

/**
 * Strip whitespace from a buffer. The second parameter controls if
 * comments are considered contents to be removed or not.
 */
extern void strbuf_stripspace(struct strbuf *buf, int skip_comments);

/**
 * Temporary alias until all topic branches have switched to use
 * strbuf_stripspace directly.
 */
static inline void stripspace(struct strbuf *buf, int skip_comments)
{
    strbuf_stripspace(buf, skip_comments);
}

extern void strbuf_add_lines(struct strbuf *sb, const char *prefix,
                             const char *buf, size_t size);

/**
 * "Complete" the contents of `sb` by ensuring that either it ends with the
 * character `term`, or it is empty.  This can be used, for example,
 * to ensure that text ends with a newline, but without creating an empty
 * blank line if there is no content in the first place.
 */
static inline void strbuf_complete(struct strbuf *sb, char term)
{
    if (sb->len && sb->buf[sb->len - 1] != term) strbuf_addch(sb, term);
}

static inline void strbuf_complete_line(struct strbuf *sb)
{
    strbuf_complete(sb, '\n');
}

/**
 * print routines
 * ----------------------------
 *
 * print the strbuf's buf to the standard output
 */
static inline void strbuf_print(struct strbuf *sb)
{
    fwrite(sb->buf, sizeof(char), sb->len, stdout);
}

/**
 * print all the details of the strbuf
 */
static inline void strbuf_print_debug(struct strbuf *sb)
{
    fprintf(stdout, "Length: %zu, Alloc: %zu\nContents: %s", sb->len, sb->alloc,
            sb->buf);
}

/**
 * Add a formatted string to the buffer.
 */
__attribute__((format(printf, 2, 3))) extern void
strbuf_addf(struct strbuf *sb, const char *fmt, ...);

__attribute__((format(printf, 2, 0))) extern void
strbuf_vaddf(struct strbuf *sb, const char *fmt, va_list ap);

/**
 * count the number of characters present in the string
 */
static inline size_t strbuf_count(struct strbuf *buf, char ch)
{
    size_t count = 0;

    for (size_t i = 0; i < buf->len; i++) {
        if (buf->buf[i] == ch)
            count++;
    }
    return count;
}

static inline ssize_t strbuf_findch(struct strbuf *buf, char ch)
{
    char *pos = strchr(buf->buf, ch);
    return pos ? pos - buf->buf : -1;
}

extern void strbuf_replace_chars(struct strbuf *sb, char replace, char subs);

extern void strbuf_humanise_bytes(struct strbuf *buf, off_t bytes);

#endif /* STRBUF_H */
