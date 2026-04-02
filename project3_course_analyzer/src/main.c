#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NAME_LEN 100
#define COURSE_LEN 100
#define INPUT_LEN 256
#define MAX_SUBJECTS 10

#define DATA_FILE "data/records.txt"

typedef struct {
    int id;
    char name[NAME_LEN];
    char course[COURSE_LEN];
    int age;
    float grades[MAX_SUBJECTS];
    int gradeCount;
    float gpa;
} Student;

typedef struct {
    Student *items;
    size_t count;
    size_t capacity;
    int isDirty;
} Database;

static void init_database(Database *db) {
    db->items = NULL;
    db->count = 0;
    db->capacity = 0;
    db->isDirty = 0;
}

static void free_database(Database *db) {
    free(db->items);
    db->items = NULL;
    db->count = 0;
    db->capacity = 0;
    db->isDirty = 0;
}

static int ensure_capacity(Database *db, size_t required) {
    if (required <= db->capacity) {
        return 1;
    }

    size_t newCap = (db->capacity == 0) ? 4 : db->capacity * 2;
    while (newCap < required) {
        newCap *= 2;
    }

    Student *tmp = (Student *)realloc(db->items, newCap * sizeof(Student));
    if (!tmp) {
        return 0;
    }

    db->items = tmp;
    db->capacity = newCap;
    return 1;
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static int read_line(const char *prompt, char *buffer, size_t size) {
    if (prompt) {
        printf("%s", prompt);
    }

    if (!fgets(buffer, (int)size, stdin)) {
        return 0;
    }

    if (strchr(buffer, '\n') == NULL) {
        int ch = 0;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            ;
        }
    }

    trim_newline(buffer);
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

static int parse_float(const char *s, float *out) {
    char *end = NULL;
    float value = strtof(s, &end);
    if (end == s || *end != '\0') {
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
            printf("Invalid integer input. Try again.\n");
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

static int read_float_in_range(const char *prompt, float min, float max, float *out) {
    char input[INPUT_LEN];
    float value = 0.0f;

    while (1) {
        if (!read_line(prompt, input, sizeof(input))) {
            return 0;
        }

        if (!parse_float(input, &value)) {
            printf("Invalid number input. Try again.\n");
            continue;
        }

        if (value < min || value > max) {
            printf("Value must be between %.2f and %.2f.\n", min, max);
            continue;
        }

        *out = value;
        return 1;
    }
}

static float calculate_gpa(const float *grades, int count) {
    if (!grades || count <= 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += grades[i];
    }
    return sum / (float)count;
}

static int find_index_by_id(const Database *db, int id) {
    for (size_t i = 0; i < db->count; i++) {
        if (db->items[i].id == id) {
            return (int)i;
        }
    }
    return -1;
}

static void print_student_header(void) {
    printf("\n%-6s %-20s %-15s %-5s %-8s %s\n", "ID", "Name", "Course", "Age", "GPA", "Grades");
    printf("-------------------------------------------------------------------------------\n");
}

static void print_student(const Student *s) {
    printf("%-6d %-20s %-15s %-5d %-8.2f ", s->id, s->name, s->course, s->age, s->gpa);
    for (int i = 0; i < s->gradeCount; i++) {
        printf("%.1f", s->grades[i]);
        if (i < s->gradeCount - 1) {
            printf(",");
        }
    }
    printf("\n");
}

static int add_student(Database *db) {
    Student s;
    char input[INPUT_LEN];

    if (!read_int_in_range("Enter student ID (1-999999): ", 1, 999999, &s.id)) {
        return 0;
    }

    if (find_index_by_id(db, s.id) >= 0) {
        printf("Error: Student ID already exists.\n");
        return 0;
    }

    if (!read_line("Enter student name: ", s.name, sizeof(s.name)) || strlen(s.name) == 0) {
        printf("Error: Name cannot be empty.\n");
        return 0;
    }

    if (!read_line("Enter course name: ", s.course, sizeof(s.course)) || strlen(s.course) == 0) {
        printf("Error: Course cannot be empty.\n");
        return 0;
    }

    if (!read_int_in_range("Enter age (15-100): ", 15, 100, &s.age)) {
        return 0;
    }

    if (!read_int_in_range("Enter number of grades (1-10): ", 1, MAX_SUBJECTS, &s.gradeCount)) {
        return 0;
    }

    for (int i = 0; i < s.gradeCount; i++) {
        snprintf(input, sizeof(input), "Grade %d (0-100): ", i + 1);
        if (!read_float_in_range(input, 0.0f, 100.0f, &s.grades[i])) {
            return 0;
        }
    }

    s.gpa = calculate_gpa(s.grades, s.gradeCount);

    if (!ensure_capacity(db, db->count + 1)) {
        printf("Error: Memory allocation failed while adding student.\n");
        return 0;
    }

    db->items[db->count++] = s;
    db->isDirty = 1;
    printf("Student added successfully.\n");
    return 1;
}

static void display_all_students(const Database *db) {
    if (db->count == 0) {
        printf("No student records available.\n");
        return;
    }

    print_student_header();
    for (size_t i = 0; i < db->count; i++) {
        print_student(&db->items[i]);
    }
}

static void search_by_id(const Database *db) {
    int id = 0;
    if (!read_int_in_range("Enter ID to search: ", 1, 999999, &id)) {
        return;
    }

    int idx = find_index_by_id(db, id);
    if (idx < 0) {
        printf("No record found for ID %d.\n", id);
        return;
    }

    print_student_header();
    print_student(&db->items[idx]);
}

static int contains_case_insensitive(const char *text, const char *query) {
    if (!text || !query) {
        return 0;
    }

    size_t tLen = strlen(text);
    size_t qLen = strlen(query);

    if (qLen == 0 || qLen > tLen) {
        return 0;
    }

    for (size_t i = 0; i <= tLen - qLen; i++) {
        size_t j = 0;
        while (j < qLen) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)query[j])) {
                break;
            }
            j++;
        }
        if (j == qLen) {
            return 1;
        }
    }

    return 0;
}

static void search_by_name(const Database *db) {
    char name[NAME_LEN];
    int found = 0;

    if (!read_line("Enter name to search (partial allowed): ", name, sizeof(name))) {
        return;
    }

    if (strlen(name) == 0) {
        printf("Search query cannot be empty.\n");
        return;
    }

    print_student_header();
    for (size_t i = 0; i < db->count; i++) {
        if (contains_case_insensitive(db->items[i].name, name)) {
            print_student(&db->items[i]);
            found = 1;
        }
    }

    if (!found) {
        printf("No records matched query: %s\n", name);
    }
}

static int update_student(Database *db) {
    int id = 0;
    if (!read_int_in_range("Enter ID to update: ", 1, 999999, &id)) {
        return 0;
    }

    int idx = find_index_by_id(db, id);
    if (idx < 0) {
        printf("No record found for ID %d.\n", id);
        return 0;
    }

    Student *s = &db->items[idx];
    char input[INPUT_LEN];

    printf("Updating student %s (ID %d). Leave blank to keep text fields unchanged.\n", s->name, s->id);

    if (!read_line("New name: ", input, sizeof(input))) {
        return 0;
    }
    if (strlen(input) > 0) {
        strncpy(s->name, input, sizeof(s->name) - 1);
        s->name[sizeof(s->name) - 1] = '\0';
    }

    if (!read_line("New course: ", input, sizeof(input))) {
        return 0;
    }
    if (strlen(input) > 0) {
        strncpy(s->course, input, sizeof(s->course) - 1);
        s->course[sizeof(s->course) - 1] = '\0';
    }

    if (!read_int_in_range("New age (15-100): ", 15, 100, &s->age)) {
        return 0;
    }

    if (!read_int_in_range("New number of grades (1-10): ", 1, MAX_SUBJECTS, &s->gradeCount)) {
        return 0;
    }

    for (int i = 0; i < s->gradeCount; i++) {
        snprintf(input, sizeof(input), "New grade %d (0-100): ", i + 1);
        if (!read_float_in_range(input, 0.0f, 100.0f, &s->grades[i])) {
            return 0;
        }
    }

    s->gpa = calculate_gpa(s->grades, s->gradeCount);
    db->isDirty = 1;
    printf("Record updated successfully.\n");
    return 1;
}

static int delete_student(Database *db) {
    int id = 0;
    if (!read_int_in_range("Enter ID to delete: ", 1, 999999, &id)) {
        return 0;
    }

    int idx = find_index_by_id(db, id);
    if (idx < 0) {
        printf("No record found for ID %d.\n", id);
        return 0;
    }

    for (size_t i = (size_t)idx; i + 1 < db->count; i++) {
        db->items[i] = db->items[i + 1];
    }

    db->count--;
    db->isDirty = 1;
    printf("Record deleted successfully.\n");
    return 1;
}

static void swap_students(Student *a, Student *b) {
    Student temp = *a;
    *a = *b;
    *b = temp;
}

static void sort_by_id(Database *db) {
    for (size_t i = 0; i < db->count; i++) {
        for (size_t j = 0; j + 1 < db->count - i; j++) {
            if (db->items[j].id > db->items[j + 1].id) {
                swap_students(&db->items[j], &db->items[j + 1]);
            }
        }
    }
    printf("Sorted by ID.\n");
}

static void sort_by_name(Database *db) {
    for (size_t i = 0; i < db->count; i++) {
        for (size_t j = 0; j + 1 < db->count - i; j++) {
            if (strcmp(db->items[j].name, db->items[j + 1].name) > 0) {
                swap_students(&db->items[j], &db->items[j + 1]);
            }
        }
    }
    printf("Sorted by name.\n");
}

static void sort_by_gpa_desc(Database *db) {
    for (size_t i = 0; i < db->count; i++) {
        for (size_t j = 0; j + 1 < db->count - i; j++) {
            if (db->items[j].gpa < db->items[j + 1].gpa) {
                swap_students(&db->items[j], &db->items[j + 1]);
            }
        }
    }
    printf("Sorted by GPA (descending).\n");
}

static float class_average_gpa(const Database *db) {
    if (db->count == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < db->count; i++) {
        sum += db->items[i].gpa;
    }
    return sum / (float)db->count;
}

static float median_gpa(const Database *db) {
    if (db->count == 0) {
        return 0.0f;
    }

    float *arr = (float *)malloc(db->count * sizeof(float));
    if (!arr) {
        return -1.0f;
    }

    for (size_t i = 0; i < db->count; i++) {
        arr[i] = db->items[i].gpa;
    }

    for (size_t i = 0; i < db->count; i++) {
        for (size_t j = 0; j + 1 < db->count - i; j++) {
            if (arr[j] > arr[j + 1]) {
                float t = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = t;
            }
        }
    }

    float result;
    if (db->count % 2 == 1) {
        result = arr[db->count / 2];
    } else {
        result = (arr[db->count / 2 - 1] + arr[db->count / 2]) / 2.0f;
    }

    free(arr);
    return result;
}

static void top_n_students(const Database *db) {
    if (db->count == 0) {
        printf("No records available.\n");
        return;
    }

    int n = 0;
    if (!read_int_in_range("Enter N: ", 1, (int)db->count, &n)) {
        return;
    }

    Student *copy = (Student *)malloc(db->count * sizeof(Student));
    if (!copy) {
        printf("Error: Allocation failed.\n");
        return;
    }

    memcpy(copy, db->items, db->count * sizeof(Student));

    for (size_t i = 0; i < db->count; i++) {
        for (size_t j = 0; j + 1 < db->count - i; j++) {
            if (copy[j].gpa < copy[j + 1].gpa) {
                Student temp = copy[j];
                copy[j] = copy[j + 1];
                copy[j + 1] = temp;
            }
        }
    }

    print_student_header();
    for (int i = 0; i < n; i++) {
        print_student(&copy[i]);
    }

    free(copy);
}

static void course_average_report(const Database *db) {
    if (db->count == 0) {
        printf("No records available.\n");
        return;
    }

    char courses[200][COURSE_LEN];
    float sums[200] = {0};
    int counts[200] = {0};
    int uniqueCount = 0;

    for (size_t i = 0; i < db->count; i++) {
        int idx = -1;
        for (int j = 0; j < uniqueCount; j++) {
            if (strcmp(courses[j], db->items[i].course) == 0) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            if (uniqueCount >= 200) {
                printf("Too many unique courses to analyze.\n");
                return;
            }
            strncpy(courses[uniqueCount], db->items[i].course, COURSE_LEN - 1);
            courses[uniqueCount][COURSE_LEN - 1] = '\0';
            idx = uniqueCount;
            uniqueCount++;
        }

        sums[idx] += db->items[i].gpa;
        counts[idx]++;
    }

    printf("\nCourse Performance Summary:\n");
    for (int i = 0; i < uniqueCount; i++) {
        printf("- %s: Avg GPA = %.2f (%d student%s)\n",
               courses[i],
               sums[i] / (float)counts[i],
               counts[i],
               counts[i] == 1 ? "" : "s");
    }
}

static void best_performing_by_course(const Database *db) {
    if (db->count == 0) {
        printf("No records available.\n");
        return;
    }

    char courses[200][COURSE_LEN];
    int bestIndex[200];
    int uniqueCount = 0;

    for (size_t i = 0; i < db->count; i++) {
        int idx = -1;
        for (int j = 0; j < uniqueCount; j++) {
            if (strcmp(courses[j], db->items[i].course) == 0) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            if (uniqueCount >= 200) {
                printf("Too many unique courses to analyze.\n");
                return;
            }
            strncpy(courses[uniqueCount], db->items[i].course, COURSE_LEN - 1);
            courses[uniqueCount][COURSE_LEN - 1] = '\0';
            bestIndex[uniqueCount] = (int)i;
            uniqueCount++;
        } else {
            if (db->items[i].gpa > db->items[bestIndex[idx]].gpa) {
                bestIndex[idx] = (int)i;
            }
        }
    }

    printf("\nBest Performing Student Per Course:\n");
    for (int i = 0; i < uniqueCount; i++) {
        Student *s = &db->items[bestIndex[i]];
        printf("- %s: %s (ID %d), GPA %.2f\n", courses[i], s->name, s->id, s->gpa);
    }
}

static void overall_report(const Database *db) {
    if (db->count == 0) {
        printf("No records available for analysis.\n");
        return;
    }

    size_t highestIdx = 0;
    size_t lowestIdx = 0;

    for (size_t i = 1; i < db->count; i++) {
        if (db->items[i].gpa > db->items[highestIdx].gpa) {
            highestIdx = i;
        }
        if (db->items[i].gpa < db->items[lowestIdx].gpa) {
            lowestIdx = i;
        }
    }

    float avg = class_average_gpa(db);
    float med = median_gpa(db);

    if (med < 0.0f) {
        printf("Error: Could not compute median due to allocation failure.\n");
        return;
    }

    printf("\nOverall Performance Report:\n");
    printf("- Total Students: %zu\n", db->count);
    printf("- Class Average GPA: %.2f\n", avg);
    printf("- Median GPA: %.2f\n", med);
    printf("- Highest GPA: %.2f (%s, ID %d)\n",
           db->items[highestIdx].gpa,
           db->items[highestIdx].name,
           db->items[highestIdx].id);
    printf("- Lowest GPA: %.2f (%s, ID %d)\n",
           db->items[lowestIdx].gpa,
           db->items[lowestIdx].name,
           db->items[lowestIdx].id);
}

static int save_to_file(const Database *db, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Could not open file for writing: %s\n", filename);
        return 0;
    }

    for (size_t i = 0; i < db->count; i++) {
        const Student *s = &db->items[i];
        fprintf(fp, "%d|%s|%s|%d|%d|",
                s->id, s->name, s->course, s->age, s->gradeCount);

        for (int g = 0; g < s->gradeCount; g++) {
            fprintf(fp, "%.2f", s->grades[g]);
            if (g < s->gradeCount - 1) {
                fputc(',', fp);
            }
        }
        fputc('\n', fp);
    }

    fclose(fp);
    printf("Saved %zu records to %s\n", db->count, filename);
    return 1;
}

static int load_from_file(Database *db, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: Could not open file for reading: %s\n", filename);
        return 0;
    }

    free_database(db);
    init_database(db);

    char line[1024];
    int lineNo = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineNo++;
        trim_newline(line);

        if (strlen(line) == 0) {
            continue;
        }

        Student s;
        char *token = strtok(line, "|");
        if (!token || !parse_int(token, &s.id)) {
            printf("Warning: Skipping malformed record at line %d (id).\n", lineNo);
            continue;
        }

        token = strtok(NULL, "|");
        if (!token) {
            printf("Warning: Skipping malformed record at line %d (name).\n", lineNo);
            continue;
        }
        strncpy(s.name, token, NAME_LEN - 1);
        s.name[NAME_LEN - 1] = '\0';

        token = strtok(NULL, "|");
        if (!token) {
            printf("Warning: Skipping malformed record at line %d (course).\n", lineNo);
            continue;
        }
        strncpy(s.course, token, COURSE_LEN - 1);
        s.course[COURSE_LEN - 1] = '\0';

        token = strtok(NULL, "|");
        if (!token || !parse_int(token, &s.age)) {
            printf("Warning: Skipping malformed record at line %d (age).\n", lineNo);
            continue;
        }
        if (s.age < 15 || s.age > 100) {
            printf("Warning: Skipping out-of-range age at line %d.\n", lineNo);
            continue;
        }

        token = strtok(NULL, "|");
        if (!token || !parse_int(token, &s.gradeCount) || s.gradeCount < 1 || s.gradeCount > MAX_SUBJECTS) {
            printf("Warning: Skipping malformed record at line %d (grade count).\n", lineNo);
            continue;
        }

        token = strtok(NULL, "|");
        if (!token) {
            printf("Warning: Skipping malformed record at line %d (grades).\n", lineNo);
            continue;
        }

        char gradesCopy[512];
        strncpy(gradesCopy, token, sizeof(gradesCopy) - 1);
        gradesCopy[sizeof(gradesCopy) - 1] = '\0';

        char *gTok = strtok(gradesCopy, ",");
        int g = 0;
        while (gTok && g < s.gradeCount) {
            if (!parse_float(gTok, &s.grades[g])) {
                break;
            }
            if (s.grades[g] < 0.0f || s.grades[g] > 100.0f) {
                break;
            }
            gTok = strtok(NULL, ",");
            g++;
        }

        if (g != s.gradeCount) {
            printf("Warning: Skipping malformed grade list at line %d.\n", lineNo);
            continue;
        }

        s.gpa = calculate_gpa(s.grades, s.gradeCount);

        if (find_index_by_id(db, s.id) >= 0) {
            printf("Warning: Duplicate ID %d in file; skipping line %d.\n", s.id, lineNo);
            continue;
        }

        if (!ensure_capacity(db, db->count + 1)) {
            printf("Error: Allocation failed while loading records.\n");
            fclose(fp);
            return 0;
        }

        db->items[db->count++] = s;
    }

    fclose(fp);
    db->isDirty = 0;
    printf("Loaded %zu records from %s\n", db->count, filename);
    return 1;
}

static int ask_yes_no(const char *prompt, int *answerYes) {
    char input[INPUT_LEN];
    while (1) {
        if (!read_line(prompt, input, sizeof(input))) {
            return 0;
        }
        if (strlen(input) == 1) {
            char c = (char)tolower((unsigned char)input[0]);
            if (c == 'y') {
                *answerYes = 1;
                return 1;
            }
            if (c == 'n') {
                *answerYes = 0;
                return 1;
            }
        }
        printf("Please enter Y or N.\n");
    }
}

static void sort_menu(Database *db) {
    if (db->count == 0) {
        printf("No records to sort.\n");
        return;
    }

    printf("\nSort Options:\n");
    printf("1. Sort by ID\n");
    printf("2. Sort by Name\n");
    printf("3. Sort by GPA (Descending)\n");

    int choice = 0;
    if (!read_int_in_range("Choose option: ", 1, 3, &choice)) {
        return;
    }

    switch (choice) {
        case 1:
            sort_by_id(db);
            break;
        case 2:
            sort_by_name(db);
            break;
        case 3:
            sort_by_gpa_desc(db);
            break;
        default:
            printf("Invalid sort option.\n");
            break;
    }
}

static void report_menu(Database *db) {
    if (db->count == 0) {
        printf("No records available for reports.\n");
        return;
    }

    printf("\nReports Menu:\n");
    printf("1. Overall Performance Report\n");
    printf("2. Top N Students\n");
    printf("3. Course-based Average GPA\n");
    printf("4. Best-performing Student Per Course\n");

    int choice = 0;
    if (!read_int_in_range("Choose option: ", 1, 4, &choice)) {
        return;
    }

    switch (choice) {
        case 1:
            overall_report(db);
            break;
        case 2:
            top_n_students(db);
            break;
        case 3:
            course_average_report(db);
            break;
        case 4:
            best_performing_by_course(db);
            break;
        default:
            printf("Invalid report option.\n");
            break;
    }
}

static void search_menu(const Database *db) {
    if (db->count == 0) {
        printf("No records available to search.\n");
        return;
    }

    printf("\nSearch Menu:\n");
    printf("1. Search by ID\n");
    printf("2. Search by Name\n");

    int choice = 0;
    if (!read_int_in_range("Choose option: ", 1, 2, &choice)) {
        return;
    }

    switch (choice) {
        case 1:
            search_by_id(db);
            break;
        case 2:
            search_by_name(db);
            break;
        default:
            printf("Invalid search option.\n");
            break;
    }
}

static void print_main_menu(void) {
    printf("\nCourse Performance and Academic Records Analyzer\n");
    printf("1. Add new student record\n");
    printf("2. Display all records\n");
    printf("3. Update a record\n");
    printf("4. Delete a record\n");
    printf("5. Search records\n");
    printf("6. Sort records\n");
    printf("7. Generate reports\n");
    printf("8. Save records to file\n");
    printf("9. Load records from file\n");
    printf("10. Exit\n");
}

int main(void) {
    Database db;
    init_database(&db);

    FILE *startupFile = fopen(DATA_FILE, "r");
    if (startupFile) {
        fclose(startupFile);
        load_from_file(&db, DATA_FILE);
    } else {
        printf("No existing data file found at %s. Starting with empty records.\n", DATA_FILE);
    }

    int running = 1;
    while (running) {
        int choice = 0;
        print_main_menu();

        if (!read_int_in_range("Enter your choice: ", 1, 10, &choice)) {
            printf("Input stream closed. Exiting.\n");
            break;
        }

        switch (choice) {
            case 1:
                add_student(&db);
                break;
            case 2:
                display_all_students(&db);
                break;
            case 3:
                update_student(&db);
                break;
            case 4:
                delete_student(&db);
                break;
            case 5:
                search_menu(&db);
                break;
            case 6:
                sort_menu(&db);
                break;
            case 7:
                report_menu(&db);
                break;
            case 8:
                if (save_to_file(&db, DATA_FILE)) {
                    db.isDirty = 0;
                }
                break;
            case 9:
                if (db.isDirty) {
                    int shouldSave = 0;
                    if (!ask_yes_no("Unsaved changes detected. Save before loading? (Y/N): ", &shouldSave)) {
                        printf("Input stream closed. Exiting.\n");
                        running = 0;
                        break;
                    }
                    if (shouldSave) {
                        if (save_to_file(&db, DATA_FILE)) {
                            db.isDirty = 0;
                        } else {
                            printf("Save failed. Load cancelled to protect unsaved data.\n");
                            break;
                        }
                    }
                }
                load_from_file(&db, DATA_FILE);
                break;
            case 10:
                if (db.isDirty) {
                    int shouldSave = 0;
                    if (!ask_yes_no("Unsaved changes detected. Save before exit? (Y/N): ", &shouldSave)) {
                        printf("Input stream closed. Exiting.\n");
                        running = 0;
                        break;
                    }
                    if (shouldSave) {
                        if (save_to_file(&db, DATA_FILE)) {
                            db.isDirty = 0;
                        } else {
                            printf("Save failed. Returning to menu.\n");
                            break;
                        }
                    }
                }
                running = 0;
                break;
            default:
                printf("Invalid choice.\n");
                break;
        }
    }

    free_database(&db);
    printf("Program terminated safely.\n");
    return 0;
}
