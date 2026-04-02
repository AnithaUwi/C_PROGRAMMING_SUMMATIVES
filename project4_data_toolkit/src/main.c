#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INPUT_LEN 256
#define DEFAULT_DATA_FILE "data/dataset.txt"
#define SEARCH_EPSILON 1e-6

typedef struct {
    double *items;
    size_t count;
    size_t capacity;
} Dataset;

typedef int (*MenuHandler)(Dataset *ds);
typedef int (*FilterCallback)(double value, const void *ctx);
typedef double (*TransformCallback)(double value, const void *ctx);
typedef int (*CompareValues)(double a, double b);

typedef struct {
    int choice;
    const char *label;
    MenuHandler handler;
} MenuEntry;

typedef struct {
    double threshold;
} ThresholdContext;

typedef struct {
    double factor;
} ScaleContext;

typedef struct {
    double delta;
} ShiftContext;

static CompareValues g_compare_value_fn = NULL;

static void init_dataset(Dataset *ds) {
    ds->items = NULL;
    ds->count = 0;
    ds->capacity = 0;
}

static void free_dataset(Dataset *ds) {
    free(ds->items);
    ds->items = NULL;
    ds->count = 0;
    ds->capacity = 0;
}

static int ensure_capacity(Dataset *ds, size_t required) {
    if (required <= ds->capacity) {
        return 1;
    }

    size_t new_cap = (ds->capacity == 0) ? 8 : ds->capacity * 2;
    while (new_cap < required) {
        new_cap *= 2;
    }

    double *tmp = (double *)realloc(ds->items, new_cap * sizeof(double));
    if (!tmp) {
        return 0;
    }

    ds->items = tmp;
    ds->capacity = new_cap;
    return 1;
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

static int read_line(const char *prompt, char *buf, size_t size) {
    if (prompt) {
        printf("%s", prompt);
    }

    if (!fgets(buf, (int)size, stdin)) {
        return 0;
    }

    trim_newline(buf);
    return 1;
}

static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long value = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int parse_double(const char *s, double *out) {
    char *end = NULL;
    double value = strtod(s, &end);
    if (end == s || *end != '\0') {
        return 0;
    }
    if (!isfinite(value)) {
        return 0;
    }
    *out = value;
    return 1;
}

static int read_int_in_range(const char *prompt, int min, int max, int *out) {
    char input[INPUT_LEN];
    int value = 0;

    while (1) {
        if (!read_line(prompt, input, sizeof(input))) {
            return 0;
        }

        if (!parse_int(input, &value)) {
            printf("Invalid integer. Try again.\n");
            continue;
        }

        if (value < min || value > max) {
            printf("Value must be between %d and %d.\n", min, max);
            continue;
        }

        *out = value;
        return 1;
    }
}

static int read_double(const char *prompt, double *out) {
    char input[INPUT_LEN];
    while (1) {
        if (!read_line(prompt, input, sizeof(input))) {
            return 0;
        }

        if (!parse_double(input, out)) {
            printf("Invalid number. Try again.\n");
            continue;
        }

        return 1;
    }
}

static int append_value(Dataset *ds, double value) {
    if (!ensure_capacity(ds, ds->count + 1)) {
        printf("Error: Memory allocation failed while appending value.\n");
        return 0;
    }

    ds->items[ds->count++] = value;
    return 1;
}

static int create_dataset_handler(Dataset *ds) {
    int n = 0;
    if (!read_int_in_range("How many values to create (1-100000): ", 1, 100000, &n)) {
        return 1;
    }

    free_dataset(ds);
    init_dataset(ds);

    if (!ensure_capacity(ds, (size_t)n)) {
        printf("Error: Allocation failed while creating dataset.\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        double value = 0.0;
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "Value %d: ", i + 1);
        if (!read_double(prompt, &value)) {
            return 1;
        }
        ds->items[i] = value;
    }
    ds->count = (size_t)n;

    printf("Dataset created with %zu values.\n", ds->count);
    return 1;
}

static int append_values_handler(Dataset *ds) {
    int n = 0;
    if (!read_int_in_range("How many values to append (1-100000): ", 1, 100000, &n)) {
        return 1;
    }

    for (int i = 0; i < n; i++) {
        double value = 0.0;
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "Append value %d: ", i + 1);
        if (!read_double(prompt, &value)) {
            return 1;
        }
        if (!append_value(ds, value)) {
            return 1;
        }
    }

    printf("Appended %d values. Dataset size is now %zu.\n", n, ds->count);
    return 1;
}

static int display_dataset_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    printf("\nDataset (%zu values):\n", ds->count);
    for (size_t i = 0; i < ds->count; i++) {
        printf("[%zu] %.4f\n", i, ds->items[i]);
    }
    return 1;
}

static double compute_sum(const Dataset *ds) {
    double sum = 0.0;
    for (size_t i = 0; i < ds->count; i++) {
        sum += ds->items[i];
    }
    return sum;
}

static int sum_average_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    double sum = compute_sum(ds);
    double avg = sum / (double)ds->count;

    printf("Sum: %.4f\n", sum);
    printf("Average: %.4f\n", avg);
    return 1;
}

static int min_max_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    double min = ds->items[0];
    double max = ds->items[0];

    for (size_t i = 1; i < ds->count; i++) {
        if (ds->items[i] < min) {
            min = ds->items[i];
        }
        if (ds->items[i] > max) {
            max = ds->items[i];
        }
    }

    printf("Min: %.4f\n", min);
    printf("Max: %.4f\n", max);
    return 1;
}

static int value_above_threshold(double value, const void *ctx) {
    const ThresholdContext *th = (const ThresholdContext *)ctx;
    return value > th->threshold;
}

static int value_below_threshold(double value, const void *ctx) {
    const ThresholdContext *th = (const ThresholdContext *)ctx;
    return value < th->threshold;
}

static int filter_dataset(const Dataset *src, Dataset *dst, FilterCallback cb, const void *ctx) {
    if (!src || !dst || !cb) {
        return 0;
    }

    free_dataset(dst);
    init_dataset(dst);

    if (!ensure_capacity(dst, src->count)) {
        return 0;
    }

    for (size_t i = 0; i < src->count; i++) {
        if (cb(src->items[i], ctx)) {
            dst->items[dst->count++] = src->items[i];
        }
    }

    return 1;
}

static int filter_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    int option = 0;
    printf("\nFilter options:\n");
    printf("1. Keep values above threshold\n");
    printf("2. Keep values below threshold\n");

    if (!read_int_in_range("Choose option: ", 1, 2, &option)) {
        return 1;
    }

    ThresholdContext ctx;
    if (!read_double("Threshold: ", &ctx.threshold)) {
        return 1;
    }

    FilterCallback callback = NULL;
    if (option == 1) {
        callback = value_above_threshold;
    } else if (option == 2) {
        callback = value_below_threshold;
    }

    if (!callback) {
        printf("Error: Invalid filter callback.\n");
        return 1;
    }

    Dataset filtered;
    init_dataset(&filtered);

    if (!filter_dataset(ds, &filtered, callback, &ctx)) {
        printf("Error: Filtering failed due to memory or callback issue.\n");
        free_dataset(&filtered);
        return 1;
    }

    free_dataset(ds);
    *ds = filtered;

    printf("Filter applied. Dataset now has %zu values.\n", ds->count);
    return 1;
}

static double scale_value(double value, const void *ctx) {
    const ScaleContext *sc = (const ScaleContext *)ctx;
    return value * sc->factor;
}

static double shift_value(double value, const void *ctx) {
    const ShiftContext *sh = (const ShiftContext *)ctx;
    return value + sh->delta;
}

static int transform_dataset(Dataset *ds, TransformCallback cb, const void *ctx) {
    if (!ds || !cb) {
        return 0;
    }

    for (size_t i = 0; i < ds->count; i++) {
        ds->items[i] = cb(ds->items[i], ctx);
    }

    return 1;
}

static int transform_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    int option = 0;
    printf("\nTransform options:\n");
    printf("1. Scale all values\n");
    printf("2. Shift all values\n");

    if (!read_int_in_range("Choose option: ", 1, 2, &option)) {
        return 1;
    }

    TransformCallback callback = NULL;
    ScaleContext sc;
    ShiftContext sh;

    if (option == 1) {
        if (!read_double("Scale factor: ", &sc.factor)) {
            return 1;
        }
        callback = scale_value;
        if (!transform_dataset(ds, callback, &sc)) {
            printf("Error: Transform failed due to invalid callback.\n");
            return 1;
        }
    } else {
        if (!read_double("Shift delta: ", &sh.delta)) {
            return 1;
        }
        callback = shift_value;
        if (!transform_dataset(ds, callback, &sh)) {
            printf("Error: Transform failed due to invalid callback.\n");
            return 1;
        }
    }

    printf("Transformation applied successfully.\n");
    return 1;
}

static int compare_asc_value(double a, double b) {
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int compare_desc_value(double a, double b) {
    if (a > b) {
        return -1;
    }
    if (a < b) {
        return 1;
    }
    return 0;
}

static int qsort_compare_adapter(const void *lhs, const void *rhs) {
    const double a = *(const double *)lhs;
    const double b = *(const double *)rhs;

    if (!g_compare_value_fn) {
        return 0;
    }

    return g_compare_value_fn(a, b);
}

static int sort_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    int option = 0;
    printf("\nSort options:\n");
    printf("1. Ascending\n");
    printf("2. Descending\n");

    if (!read_int_in_range("Choose option: ", 1, 2, &option)) {
        return 1;
    }

    CompareValues cmp = (option == 1) ? compare_asc_value : compare_desc_value;
    if (!cmp) {
        printf("Error: Comparison callback is NULL.\n");
        return 1;
    }

    g_compare_value_fn = cmp;
    qsort(ds->items, ds->count, sizeof(double), qsort_compare_adapter);
    g_compare_value_fn = NULL;

    printf("Dataset sorted successfully.\n");
    return 1;
}

static int search_handler(Dataset *ds) {
    if (ds->count == 0) {
        printf("Dataset is empty.\n");
        return 1;
    }

    double target = 0.0;
    if (!read_double("Value to search for: ", &target)) {
        return 1;
    }

    size_t found_count = 0;
    for (size_t i = 0; i < ds->count; i++) {
        if (fabs(ds->items[i] - target) <= SEARCH_EPSILON) {
            printf("Found at index %zu\n", i);
            found_count++;
        }
    }

    if (found_count == 0) {
        printf("Value not found.\n");
    } else {
        printf("Total matches: %zu\n", found_count);
    }

    return 1;
}

static int save_to_file(const Dataset *ds, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not open file for writing: %s\n", filename);
        return 0;
    }

    fprintf(fp, "%zu\n", ds->count);
    for (size_t i = 0; i < ds->count; i++) {
        fprintf(fp, "%.17g\n", ds->items[i]);
    }

    fclose(fp);
    return 1;
}

static int load_from_file(Dataset *ds, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Could not open file for reading: %s\n", filename);
        return 0;
    }

    char line[INPUT_LEN];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        printf("Error: File is empty or unreadable.\n");
        return 0;
    }

    trim_newline(line);
    int count_raw = 0;
    if (!parse_int(line, &count_raw) || count_raw < 0) {
        fclose(fp);
        printf("Error: Invalid dataset count in file.\n");
        return 0;
    }

    Dataset loaded;
    init_dataset(&loaded);

    if (!ensure_capacity(&loaded, (size_t)count_raw)) {
        fclose(fp);
        printf("Error: Allocation failed while loading data.\n");
        return 0;
    }

    for (int i = 0; i < count_raw; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            free_dataset(&loaded);
            printf("Error: File ended before expected number of values.\n");
            return 0;
        }

        trim_newline(line);
        double value = 0.0;
        if (!parse_double(line, &value)) {
            fclose(fp);
            free_dataset(&loaded);
            printf("Error: Invalid numeric value at line %d.\n", i + 2);
            return 0;
        }

        loaded.items[loaded.count++] = value;
    }

    fclose(fp);
    free_dataset(ds);
    *ds = loaded;
    return 1;
}

static int save_handler(Dataset *ds) {
    int use_default = 0;
    if (!read_int_in_range("Use default save path data/dataset.txt? (1=Yes, 2=No): ", 1, 2, &use_default)) {
        return 1;
    }

    char filename[INPUT_LEN];
    if (use_default == 1) {
        filename[0] = '\0';
    } else {
        if (!read_line("File to save: ", filename, sizeof(filename))) {
            return 1;
        }
        if (strlen(filename) == 0) {
            printf("Error: File path cannot be empty when default is not selected.\n");
            return 1;
        }
    }

    const char *path = (strlen(filename) == 0) ? DEFAULT_DATA_FILE : filename;
    if (!save_to_file(ds, path)) {
        return 1;
    }

    printf("Saved %zu values to %s\n", ds->count, path);
    return 1;
}

static int load_handler(Dataset *ds) {
    int use_default = 0;
    if (!read_int_in_range("Use default load path data/dataset.txt? (1=Yes, 2=No): ", 1, 2, &use_default)) {
        return 1;
    }

    char filename[INPUT_LEN];
    if (use_default == 1) {
        filename[0] = '\0';
    } else {
        if (!read_line("File to load: ", filename, sizeof(filename))) {
            return 1;
        }
        if (strlen(filename) == 0) {
            printf("Error: File path cannot be empty when default is not selected.\n");
            return 1;
        }
    }

    const char *path = (strlen(filename) == 0) ? DEFAULT_DATA_FILE : filename;
    if (!load_from_file(ds, path)) {
        return 1;
    }

    printf("Loaded %zu values from %s\n", ds->count, path);
    return 1;
}

static int reset_handler(Dataset *ds) {
    free_dataset(ds);
    init_dataset(ds);
    printf("Dataset reset.\n");
    return 1;
}

static int exit_handler(Dataset *ds) {
    (void)ds;
    return 0;
}

static void print_menu(const MenuEntry *menu, size_t menu_count) {
    printf("\n===== Data Analysis Toolkit =====\n");
    for (size_t i = 0; i < menu_count; i++) {
        printf("%d. %s\n", menu[i].choice, menu[i].label);
    }
}

int main(void) {
    Dataset ds;
    init_dataset(&ds);

    const MenuEntry menu[] = {
        {1, "Create new dataset", create_dataset_handler},
        {2, "Append values to dataset", append_values_handler},
        {3, "Display dataset", display_dataset_handler},
        {4, "Compute sum and average", sum_average_handler},
        {5, "Find minimum and maximum", min_max_handler},
        {6, "Filter dataset (callback)", filter_handler},
        {7, "Transform dataset (callback)", transform_handler},
        {8, "Sort dataset (comparison callback)", sort_handler},
        {9, "Search value", search_handler},
        {10, "Save dataset to file", save_handler},
        {11, "Load dataset from file", load_handler},
        {12, "Reset dataset", reset_handler},
        {13, "Exit", exit_handler}
    };

    const size_t menu_count = sizeof(menu) / sizeof(menu[0]);
    int running = 1;

    while (running) {
        print_menu(menu, menu_count);

        int choice = 0;
        if (!read_int_in_range("Enter your choice: ", 1, 13, &choice)) {
            printf("Input stream closed. Exiting.\n");
            break;
        }

        MenuHandler handler = NULL;
        for (size_t i = 0; i < menu_count; i++) {
            if (menu[i].choice == choice) {
                handler = menu[i].handler;
                break;
            }
        }

        if (!handler) {
            printf("Error: Handler not found for choice %d.\n", choice);
            continue;
        }

        running = handler(&ds);
    }

    free_dataset(&ds);
    printf("Program exited safely.\n");
    return 0;
}
