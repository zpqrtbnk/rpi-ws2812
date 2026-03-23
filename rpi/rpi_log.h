#define LOG(...) if (is_verbose()) printf(__VA_ARGS__)
#define ERR(...) fprintf(stderr, __VA_ARGS__)

int is_verbose();
void log_verbose(int v);
