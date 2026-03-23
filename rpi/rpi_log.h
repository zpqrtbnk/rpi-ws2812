#define LOG(...) if (verbose) printf(__VA_ARGS__)
#define ERR(...) printf(stderr, __VA_ARGS__)

extern int verbose;
void log_verbose(int v);
