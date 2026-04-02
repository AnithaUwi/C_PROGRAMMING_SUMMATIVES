Project 3: Course Performance and Academic Records Analyzer in C

Build (GCC):
  gcc -Wall -Wextra -pedantic -std=c11 src/main.c -o analyzer

Run:
  ./analyzer

Features implemented:
- Dynamic array storage with malloc/realloc/free
- CRUD operations for student records
- File save/load with validation and malformed-line handling
- Search by ID and partial name
- Manual sorting by ID, name, GPA
- Reports: class average, median, highest/lowest, top N, course averages, best per course
- Input validation and duplicate ID prevention
- Startup auto-load from default file when available
- Unsaved-change prompt before loading another file state or exiting

Default data file:
  data/records.txt

Quick verification steps
1. Build and run the program.
2. Add records, then display all.
3. Update and delete by student ID.
4. Test search by ID and name.
5. Test sort by ID, name, and GPA.
6. Run all report options.
7. Save, restart, and confirm data persists.
