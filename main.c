#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>   
#include <pthread.h>
#include <stdatomic.h>
#include <wchar.h>
#include <signal.h>

#define BUF_SIZE 1024
#define RESET   "\033[0m"
#define RED     "\033[91m"
#define GREEN   "\033[92m"
#define YELLOW  "\033[93m"
#define BLUE    "\033[94m"
#define MAGENTA "\033[95m"
#define CYAN    "\033[96m"
#define WHITE   "\033[97m"
#define BOLD    "\033[1m"

void cache_put(wchar_t *path, long long weight);
void free_cache();
void *loading(void *str);
void fsys_size(wchar_t *winput);
void normalize_path(wchar_t *path);
void cached_space(wchar_t *winput);
void space_hand(wchar_t *winput);
void all_dirent(wchar_t *winput);
void calc_weight(long long *bytes, long long *kb, long long *mb, long long *gb);

int get_input(wchar_t *winput);

long long cache_get(wchar_t *path);
long long dir_weight(wchar_t *path);


// Cache all data on PC
typedef struct{
    wchar_t *path;
    long long weight;
} FileSys;

typedef struct {
    FileSys *data;
    int count;
    int capacity;
} CacheArray;

CacheArray cache = {NULL, 0, 0};

long long total_files = 0;      // Success files
long long total_errors = 0;     // Errors with open/
long long total_no_access = 0;  // сколько каталогов не открылось

atomic_bool done = false;

void sigint_handler(int sig) {
    printf(RESET);   // сброс цвета
    fflush(stdout);
    exit(0);
}

int main()
{
    signal(SIGINT, sigint_handler);

    printf(WHITE BOLD "Welcome to native file manager\n\n" RESET);
    printf(YELLOW BOLD"! 1 to analyze dir(all entrys), 2 to analyze dir on your pc !\n");
    printf("! 3 to scan your disk C:\\ and go to find most fat dir !\n" RESET);

    printf(RED "By default you go to 3(scan all disk) \n" RESET);
    
    wchar_t winput[BUF_SIZE];

    int res = get_input(winput);

    if (res == 1) { 
        all_dirent(winput);
    } else if (res == 2) {   
        space_hand(winput);                  
    } else if (res == 3) {                     
        cached_space(winput);
    } else if (res == -1) {
        printf("WRITE 'exit' to end program\n");
    } else {
        printf(WHITE BOLD "Invalid input -> scan disk\n" RESET);
        cached_space(winput);
    }
    
    free_cache();
    cache.data = NULL;
    cache.count = 0;
    cache.capacity = 0;

    return 0;
}


long long cache_get(wchar_t *path)
{
    for (int i = 0; i < cache.count; i++){
        if (wcscmp(cache.data[i].path, path) == 0) {
            printf(GREEN "Found in cache: \n" RESET);
            return cache.data[i].weight;
        }
    }
    return -1;
};

void cache_put(wchar_t *path, long long weight)
{
    if (cache.count >= cache.capacity) {
        int new_cap = cache.capacity == 0 ? 10000 : cache.capacity * 2;
        FileSys *new_data = realloc(cache.data, new_cap * sizeof(FileSys));
        if (!new_data) return;
        cache.data = new_data;
        cache.capacity = new_cap;
    }
    cache.data[cache.count].path = malloc((wcslen(path) + 1) * sizeof(wchar_t));
    wcscpy(cache.data[cache.count].path, path);
    cache.data[cache.count].weight = weight;
    cache.count++;
    // Обновляем счётчик на месте (с выравниванием вправо, ширина 8)
    if(cache.count % 10000 == 0){
        printf(MAGENTA "\rCache: %20d" RESET, cache.count);
        fflush(stdout);
    }
};

void free_cache()
{
    for (int i = 0; i < cache.count; i++){
        free(cache.data[i].path);
    }
    free(cache.data);
};

// void *loading(void *str)
// {
//     int dots = 0;
//     while(!atomic_load(&done)){
//         printf(BOLD "\rDownload" RESET);

//         for (int i = 0; i < dots; i++) printf(".");
//         for (int i = dots; i < 3; i++) printf(" ");

//         fflush(stdout);
//         usleep(125000);

//         dots = (dots + 1) % 4;
//     }
//     printf("\n   \n");
//     fflush(stdout);

//     return NULL;
// }

long long dir_weight(wchar_t *path)
{
    struct _stat64i32 st;

    _WDIR *d = _wopendir(path);
    if (!d){
        total_no_access++;
        return 0;
    }

    struct _wdirent *entry;

    long long total = 0; 

    wchar_t fullpath[1024];
    
    while ((entry = _wreaddir(d)) != NULL){

        if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0) continue;
        
        size_t len = wcslen(path);

        if (len > 0 && (path[len-1] == L'\\' || path[len-1] == L'/')){
            swprintf(fullpath, sizeof(fullpath)/sizeof(wchar_t), L"%ls%ls", path, entry->d_name);
        } else {
            swprintf(fullpath, sizeof(fullpath)/sizeof(wchar_t), L"%ls\\%ls", path, entry->d_name);
        }

        if (_wstat(fullpath, &st) == -1) {
            total_errors++;
            continue;
        }

        if (S_ISREG(st.st_mode)){
            total_files++;
            total += st.st_size;
        }

        if (S_ISDIR(st.st_mode)) total += dir_weight(fullpath);
    }

    _wclosedir(d);
    cache_put(path, total);

    return total;
}

void calc_weight(long long *bytes, long long *kb, long long *mb, long long *gb)
{
    *kb = (*bytes + 1024 - 1) / 1024;
    if (*bytes >= 1024LL * 1024) {*mb = (*bytes + 1024LL * 1024 - 1) / (1024LL * 1024);}
    if (*bytes >= 1024LL * 1024 * 1024) {*gb = (*bytes + 1024LL * 1024 * 1024 - 1) / (1024LL * 1024 * 1024);}
} 

void fsys_size(wchar_t *winput)
{
    time_t start = time(NULL);
    
    wprintf(L"START PROCESSING WITH %ls\n\n", winput);

    // pthread_t thread_load;
    // atomic_store(&done, false);
    // pthread_create(&thread_load, NULL, loading, NULL);

    total_files = total_errors = total_no_access = 0;

    long long bytes, kb, mb, gb;

    printf(BOLD "DOWNLOAD" RESET);

    bytes = dir_weight(winput);
    calc_weight(&bytes, &kb,&mb, &gb);
    printf("\nIn B: %lld\n", "In KB: %lld\n", YELLOW BOLD "In MB: %lld\n" RESET, RED BOLD "In GB: %lld\n" RESET, bytes, kb, mb, gb);

    // atomic_store(&done, true);
    // pthread_join(thread_load, NULL);

    printf(MAGENTA BOLD"\n--- Diagnostic ---\n" RESET);
    printf(GREEN "Success: %lld\n" RESET, total_files);
    printf(RED "Errors: %lld\n" RESET, total_errors);
    printf(YELLOW "No access catalogs: %lld\n" RESET, total_no_access); 

    time_t end = time(NULL);
    printf(WHITE "Compile time: %.0f secs\n" RESET, difftime(end, start));
}

void normalize_path(wchar_t *path) 
{
    if (!path) return;

    for (wchar_t *p = path; *p; p++) {
        if (*p == L'/') *p = L'\\';
    }

    wchar_t *src = path;
    wchar_t *dst = path;

    int unc = (path[0] == L'\\' && path[1] == L'\\');

    if (unc) {
        *dst++ = *src++;
        *dst++ = *src++; 
    }

    while (*src) {
        if (*src == L'\\') {
            if (dst > path && *(dst - 1) == L'\\') {
                src++;
                continue;
            }
        }
        *dst++ = *src++;
    }
    *dst = L'\0';
}
int get_input(wchar_t *winput)
{
    printf(WHITE BOLD "-------------------------------------------------\n");
            
    wprintf(L"Enter path: ");

    fflush(stdout);

    if (fgetws(winput, BUF_SIZE, stdin) == NULL) return -2; 

    size_t len = wcslen(winput);

    if (len > 0 && winput[len-1] == L'\n') winput[len-1] = L'\0';

    if (len == 0 || winput[0] == L'\0') return -3;

    if (wcscmp(winput, L"exit") == 0) return -1;

    if (len > 0 && wcscmp(winput, L"1") == 0) return 1; // режим анализа диска
            
    if (len > 0 && wcscmp(winput, L"2") == 0) return 2;// режим ручного сканирования

    if (len > 0 && wcscmp(winput, L"3") == 0) return 3;// режим анализа папки

    normalize_path(winput);
            
    wprintf(L"You entered:", MAGENTA BOLD"'%ls'\n\n"RESET, winput);
    return 0; // введён путь
}

void cached_space(wchar_t *winput) // Режим 3 (стоит классически)
{
    if (cache_get(L"C:\\") == -1) {
        fsys_size(L"C:\\");
    }

    while(1){
        int res = get_input(winput);

        if (res == -1) break;
        if (res == -2) break;
        if (res == -3) continue;

        if (res == 1) { 
            all_dirent(winput);
            return;
        }

        if (res == 2) {                     
            space_hand(winput);
            return;
        }

        if (res == 3) {                     
            printf(YELLOW "! We already in mode 3" RESET);
            continue;
        }

        long long bytes, kb, mb, gb;
        
        bytes = cache_get(winput);

        if (bytes == -1){
            printf(YELLOW"! Strange things: we don't scan this before(check path or what you writed)\n" RESET);
            bytes = dir_weight(winput);
        }

        bytes = dir_weight(winput);
        calc_weight(&bytes, &kb,&mb, &gb);
        printf("\nIn B: %lld\n", "In KB: %lld\n", YELLOW BOLD "In MB: %lld\n" RESET, RED BOLD "In GB: %lld\n" RESET, bytes, kb, mb, gb);
    }
}

void space_hand(wchar_t *winput) // 2 Ручной выбор папки для анализа веса
{
    while(1){
        int res = get_input(winput);

        if (res == -1) break;
        if (res == -2) break;
        if (res == -3) continue;

        if (res == 1) { 
            all_dirent(winput);
            return;
        }
        if (res == 2) {                     
            printf(YELLOW "! We already in mode 2" RESET);
            continue;
        }
        if (res == 3) {                     
            cached_space(winput);
            return;
        }

        fsys_size(winput);
    }
}

void all_dirent(wchar_t *winput) //Режим 1
{
    while(1){
        int res = get_input(winput);

        if (res == -1) break;
        if (res == -2) break;
        if (res == -3) continue;

        if (res == 1) { 
            printf(YELLOW "! We already in mode 1" RESET);
            continue;
        }
        if (res == 2) {   
            space_hand(winput);                  
            return;
        }

        if (res == 3) {                     
            cached_space(winput);
            return;
        }

        _WDIR *d = _wopendir(winput);
        if (!d){
            printf(RED "! Check the path or your input, this dir is not allowed" RESET);
            continue;
        }

        struct _wdirent *entry;
        struct _stat64i32 st;
        wchar_t fullpath[1024];

        printf("%ls\n", winput);
        long long bytes, kb, mb, gb;
        while ((entry = _wreaddir(d)) != NULL){

            bytes = kb = mb = gb = 0;

            if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0) continue;
            
            size_t len = wcslen(winput);

            if (len > 0 && (winput[len-1] == L'\\' || winput[len-1] == L'/')){
                swprintf(fullpath, sizeof(fullpath)/sizeof(wchar_t), L"%ls%ls", winput, entry->d_name);
            } else {
                swprintf(fullpath, sizeof(fullpath)/sizeof(wchar_t), L"%ls\\%ls", winput, entry->d_name);
            }

            if (_wstat(fullpath, &st) == -1) {
                continue;
            }

            if (S_ISREG(st.st_mode)){
                bytes = st.st_size;
            }

            if (S_ISDIR(st.st_mode)) bytes = cache_get(fullpath);
            
            calc_weight(&bytes, &kb,&mb, &gb);
            printf("L %ls\n", entry->d_name);
            printf("\nIn B: %lld\n", "In KB: %lld\n", YELLOW BOLD "In MB: %lld\n" RESET, RED BOLD "In GB: %lld\n" RESET, bytes, kb, mb, gb);
        }

        _wclosedir(d);
    }
}
