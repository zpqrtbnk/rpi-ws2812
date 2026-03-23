#define LOG(...) if (verbose) printf(__VA_ARGS__)
#define ERR(...) fprintf(stderr, __VA_ARGS__)

extern int verbose;
void log_verbose(int v);
