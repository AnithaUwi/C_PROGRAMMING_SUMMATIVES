#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_URL_LEN 2048
#define MAX_PATH_LEN 512
#define DEFAULT_URL_FILE "urls.txt"
#define OUTPUT_DIR "output"

typedef struct {
    char *url;
    int threadIndex;
} ThreadTask;

static char *trim_line(char *line) {
    if (line == NULL) {
        return NULL;
    }

    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') {
        line++;
    }

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[len - 1] = '\0';
        len--;
    }

    return line;
}

static int is_http_url(const char *url) {
    if (url == NULL) {
        return 0;
    }
    return strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0;
}

static void sanitize_filename_component(const char *url, char *out, size_t outSize) {
    size_t j = 0;
    for (size_t i = 0; url[i] != '\0' && j + 1 < outSize; i++) {
        char c = url[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) {
            out[j++] = c;
        } else {
            out[j++] = '_';
        }
    }

    if (j == 0 && outSize > 1) {
        out[j++] = 'u';
        out[j++] = 'r';
        out[j++] = 'l';
    }

    out[j] = '\0';
}

static int ensure_output_dir_exists(void) {
    if (mkdir(OUTPUT_DIR, 0755) == 0) {
        return 1;
    }

    if (errno == EEXIST) {
        struct stat st;
        if (stat(OUTPUT_DIR, &st) == 0 && S_ISDIR(st.st_mode)) {
            return 1;
        }
    }

    fprintf(stderr, "Could not create or access output directory '%s': %s\n", OUTPUT_DIR, strerror(errno));
    return 0;
}

static void *fetch_worker(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    if (task == NULL || task->url == NULL) {
        fprintf(stderr, "[Thread ?] Invalid task.\n");
        return NULL;
    }

    char safePart[128];
    sanitize_filename_component(task->url, safePart, sizeof(safePart));

    char outPath[MAX_PATH_LEN];
    snprintf(outPath, sizeof(outPath), OUTPUT_DIR "/thread_%02d_%s.html", task->threadIndex, safePart);

    char cmd[4096];
    snprintf(cmd,
             sizeof(cmd),
             "curl -L --max-time 30 --connect-timeout 10 --silent --show-error \"%s\" -o \"%s\"",
             task->url,
             outPath);

    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "[Thread %d] Fetch failed for %s (curl exit=%d)\n", task->threadIndex, task->url, rc);
        return NULL;
    }

    FILE *fp = fopen(outPath, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[Thread %d] Output missing after fetch: %s\n", task->threadIndex, outPath);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        fprintf(stderr, "[Thread %d] Could not measure output size for %s\n", task->threadIndex, outPath);
        return NULL;
    }

    long fileSize = ftell(fp);
    fclose(fp);

    if (fileSize < 0) {
        fprintf(stderr, "[Thread %d] Could not read output size for %s\n", task->threadIndex, outPath);
        return NULL;
    }

    printf("[Thread %d] OK %s -> %s (%ld bytes)\n", task->threadIndex, task->url, outPath, fileSize);
    return NULL;
}

static int load_urls(const char *path, char ***urlsOut, size_t *countOut) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open URL file %s: %s\n", path, strerror(errno));
        return 0;
    }

    char **urls = NULL;
    size_t count = 0;
    size_t capacity = 0;

    char line[MAX_URL_LEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *trimmed = trim_line(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (!is_http_url(trimmed)) {
            fprintf(stderr, "Skipping unsupported URL: %s\n", trimmed);
            continue;
        }

        if (count == capacity) {
            size_t newCap = (capacity == 0) ? 8 : capacity * 2;
            char **tmp = (char **)realloc(urls, newCap * sizeof(char *));
            if (tmp == NULL) {
                fprintf(stderr, "Memory allocation failed while loading URLs.\n");
                for (size_t i = 0; i < count; i++) {
                    free(urls[i]);
                }
                free(urls);
                fclose(fp);
                return 0;
            }
            urls = tmp;
            capacity = newCap;
        }

        urls[count] = (char *)malloc(strlen(trimmed) + 1);
        if (urls[count] == NULL) {
            fprintf(stderr, "Memory allocation failed for URL string.\n");
            for (size_t i = 0; i < count; i++) {
                free(urls[i]);
            }
            free(urls);
            fclose(fp);
            return 0;
        }

        strcpy(urls[count], trimmed);
        count++;
    }

    fclose(fp);

    *urlsOut = urls;
    *countOut = count;
    return 1;
}

static void free_urls(char **urls, size_t count) {
    if (urls == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        free(urls[i]);
    }
    free(urls);
}

int main(int argc, char **argv) {
    const char *urlFile = (argc > 1) ? argv[1] : DEFAULT_URL_FILE;

    if (!ensure_output_dir_exists()) {
        return 1;
    }

    char **urls = NULL;
    size_t urlCount = 0;

    if (!load_urls(urlFile, &urls, &urlCount)) {
        return 1;
    }

    if (urlCount == 0) {
        fprintf(stderr, "No valid URLs found in %s\n", urlFile);
        free_urls(urls, urlCount);
        return 1;
    }

    printf("Loaded %zu URL(s). Starting parallel fetch...\n", urlCount);

    pthread_t *threads = (pthread_t *)malloc(urlCount * sizeof(pthread_t));
    ThreadTask *tasks = (ThreadTask *)malloc(urlCount * sizeof(ThreadTask));

    if (threads == NULL || tasks == NULL) {
        fprintf(stderr, "Memory allocation failed for thread resources.\n");
        free(threads);
        free(tasks);
        free_urls(urls, urlCount);
        return 1;
    }

    for (size_t i = 0; i < urlCount; i++) {
        tasks[i].url = urls[i];
        tasks[i].threadIndex = (int)(i + 1);

        int rc = pthread_create(&threads[i], NULL, fetch_worker, &tasks[i]);
        if (rc != 0) {
            fprintf(stderr, "Failed to create thread %zu for %s\n", i + 1, urls[i]);
            threads[i] = 0;
        }
    }

    for (size_t i = 0; i < urlCount; i++) {
        if (threads[i] != 0) {
            pthread_join(threads[i], NULL);
        }
    }

    printf("Done. Check the output folder for downloaded HTML files.\n");

    free(threads);
    free(tasks);
    free_urls(urls, urlCount);
    return 0;
}
