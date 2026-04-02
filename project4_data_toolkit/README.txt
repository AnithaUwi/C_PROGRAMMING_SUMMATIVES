Project 4: Data Analysis Toolkit Using Function Pointers and Callbacks

Build (GCC):
  gcc -Wall -Wextra -pedantic -std=c11 src/main.c -o toolkit

Run:
  ./toolkit

Implemented features:
- Dynamic menu dispatcher using function pointers (no long conditional chain)
- Callback-based filtering and transformation
- Comparison callback used by sort (qsort adapter)
- Dynamic dataset memory management (malloc/realloc/free)
- File load/save with input validation and error handling
- Sum/average, min/max, search, reset, display

Default data file:
  data/dataset.txt
