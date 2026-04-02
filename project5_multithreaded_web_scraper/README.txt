Project 5: Multi-threaded Web Scraper (POSIX threads)

Overview
- Reads URLs from a file (default: urls.txt).
- Spawns one pthread per URL.
- Each thread downloads HTML in parallel by invoking curl command-line.
- Each thread writes output to a separate file in output/.
- Handles invalid URLs, network errors, HTTP errors, and file-write errors.

Files
- src/main.c
- urls.txt
- output/

Linux build and run
1) Install dependencies:
   sudo apt update
   sudo apt install -y build-essential curl

2) Build:
   gcc -Wall -Wextra -pedantic -std=c11 src/main.c -o scraper -lpthread

3) Run with default URL file:
   ./scraper

4) Run with custom URL file:
   ./scraper my_urls.txt

Expected output
- Console progress per thread, for example:
  [Thread 1] OK https://example.com -> output/thread_01_https___example_com.html (1256 bytes)
- HTML files saved in output/.

Notes
- URLs must start with http:// or https://
- Blank lines and lines starting with # are ignored.
- curl command must be available in PATH at runtime.
