
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h> // TODO: handle VSC

// we dont want use mmap, use fread instead
#ifdef USE_FALLBACK_MMAP

#include <io.h>
#include <sys/stat.h>
#define MAP_FAILED NULL
#define PROT_READ 0
#define MAP_PRIVATE 0


static char *portable_mmap(const char *filename, size_t *out_size, int *out_fd)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    /* get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(fp);
        return NULL;
    }

    /* allocate and read entire file */
    char *data = (char *)malloc(file_size + 1);
    if (!data)
    {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(data, 1, file_size, fp);
    fclose(fp);

    if (bytes_read != (size_t)file_size)
    {
        free(data);
        return NULL;
    }

    data[file_size] = '\0';
    *out_size = file_size;
    *out_fd = -1; /* no file descriptor on Windows fallback */
    return data;
}

static void portable_munmap(char *data, size_t size, int fd)
{
    (void)size; /* Unused in Windows version */
    (void)fd;   /* Unused in Windows version */
    free(data);
}
#else
    #include <sys/stat.h>
    /** On windows, use mmap (from mman-win32) */
    #if defined(_WIN32) || defined(_WIN64)
    
        #include <external/mman.h>

    #else

        /* Unix-like systems (Linux, macOS, BSD) have mmap */
        #include <sys/mman.h>
         #include <fcntl.h>
        #include <unistd.h>
    #endif

    char *portable_mmap(const char *filename, size_t *out_size, int *out_fd)
    {
        int fd = open(filename, O_RDONLY);
        if (fd < 0)
            return NULL;

        struct stat sb;
        if (fstat(fd, &sb) < 0)
        {
            close(fd);
            return NULL;
        }

        size_t file_size = sb.st_size;
        if (file_size == 0)
        {
            close(fd);
            return NULL;
        }

        char *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED)
        {
            close(fd);
            return NULL;
        }

        *out_size = file_size;
        *out_fd = fd;
        return data;
    }

    void portable_munmap(char *data, size_t size, int fd)
    {
        if (data)
            munmap(data, size);
        if (fd >= 0)
            close(fd);
    }
#endif