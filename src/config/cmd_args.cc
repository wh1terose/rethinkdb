
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config/cmd_args.hpp"
#include "utils.hpp"

void usage(const char *name) {
    printf("Usage:\n");
    printf("\t%s [OPTIONS] [FILE]\n", name);
    
    printf("\nOptions:\n");

    //     "                        24 characters start here.
    printf("  -h, --help            Print these usage options.\n");
    printf("  -v, --verbose         Print extra information to standard output.\n");
    printf("      --create          Create a new database.\n");
    printf("      --force           Used with the --create flag to create a new database\n"
           "                        even if there already is one.\n");

    printf("  -f, --file            Path to file or block device where database goes. Can be\n"
           "                        specified multiple times to use multiple files.\n");
#ifdef SEMANTIC_SERIALIZER_CHECK
    printf("  -S, --semantic-file   Path to the semantic file for the previously specified database file.\n"
           "                        Can only be specified after the path to the database file.\n"
           "                        Default is the name of the database file with '%s' appended.\n", DEFAULT_SEMANTIC_EXTENSION);
#endif

    printf("  -c, --cores           Number of cores to use for handling requests.\n");
    printf("  -m, --max-cache-size  Maximum amount of RAM to use for caching disk\n");
    printf("                        blocks, in megabytes.\n");
    printf("  -l, --log-file        File to log to. If not provided, messages will be printed to stderr.\n");
    printf("  -p, --port            Socket port to listen on. Defaults to %d.\n", DEFAULT_LISTEN_PORT);
    printf("      --wait-for-flush  Do not respond to commands until changes are durable. Expects\n"
           "                        'y' or 'n'.\n");
    printf("      --flush-timer     Time in milliseconds that the server should allow changes to sit\n"
           "                        in memory before flushing it to disk. Pass 'disable' to allow modified data to\n"
           "                        sit in memory indefinitely.\n");
    if (DEFAULT_FLUSH_TIMER_MS == NEVER_FLUSH) {
        printf("                        Defaults to 'disable'.\n");
    }
    else {
        printf("                        Defaults to %dms.\n", DEFAULT_FLUSH_TIMER_MS);
    }
    printf("      --flush-threshold If more than X%% of the server's maximum cache size is\n"
           "                        modified data, the server will flush it all to disk. Pass 0 to flush\n"
           "                        immediately when changes are made.\n");
    printf("      --gc-range low-high  (e.g. --gc-range 0.5-0.75)\n"
           "                        The proportion of garbage maintained by garbage collection.\n");
    printf("      --active-data-extents\n"
           "                        How many places in the file to write to at once.\n");
    printf("\nOptions for new databases:\n");
    printf("  -s, --slices          Shards total.\n");
    printf("      --block-size      Size of a block, in bytes.\n");
    printf("      --extent-size     Size of an extent, in bytes.\n");
    
    exit(-1);
}

void init_config(cmd_config_t *config) {

    bzero(config, sizeof(*config));
    
    config->verbose = false;
    config->port = DEFAULT_LISTEN_PORT;
    config->n_workers = get_cpu_count();
    
    config->log_file_name[0] = 0;
    config->log_file_name[MAX_LOG_FILE_NAME - 1] = 0;
    
    config->store_dynamic_config.serializer.gc_low_ratio = DEFAULT_GC_LOW_RATIO;
    config->store_dynamic_config.serializer.gc_high_ratio = DEFAULT_GC_HIGH_RATIO;
    config->store_dynamic_config.serializer.num_active_data_extents = DEFAULT_ACTIVE_DATA_EXTENTS;
    config->store_dynamic_config.serializer.file_size = 0;   // Unlimited file size
    config->store_dynamic_config.serializer.file_zone_size = GIGABYTE;
    
    config->store_dynamic_config.cache.max_size = DEFAULT_MAX_CACHE_RATIO * get_available_ram();
    config->store_dynamic_config.cache.wait_for_flush = false;
    config->store_dynamic_config.cache.flush_timer_ms = DEFAULT_FLUSH_TIMER_MS;
    config->store_dynamic_config.cache.flush_threshold_percent = DEFAULT_FLUSH_THRESHOLD_PERCENT;
    
    config->create_store = false;
    config->force_create = false;
    config->shutdown_after_creation = false;
    
    config->store_static_config.serializer.extent_size = DEFAULT_EXTENT_SIZE;
    config->store_static_config.serializer.block_size = DEFAULT_BTREE_BLOCK_SIZE;
    
    config->store_static_config.btree.n_slices = DEFAULT_BTREE_SHARD_FACTOR;
}

enum {
    wait_for_flush = 256, // Start these values above the ASCII range.
    flush_timer,
    flush_threshold,
    gc_range,
    active_data_extents,
    block_size,
    extent_size,
    create_database,
    force_create
};

void parse_cmd_args(int argc, char *argv[], cmd_config_t *config)
{
    init_config(config);

    std::vector<log_serializer_private_dynamic_config_t>& private_configs = config->store_dynamic_config.serializer_private;
    
    optind = 1; // reinit getopt
    while(1)
    {
        int do_help = 0;
        int do_create_database = 0;
        int do_force_create = 0;
        struct option long_options[] =
            {
                {"wait-for-flush",       required_argument, 0, wait_for_flush},
                {"flush-timer",          required_argument, 0, flush_timer},
                {"flush-threshold",      required_argument, 0, flush_threshold},
                {"gc-range",             required_argument, 0, gc_range},
                {"block-size",           required_argument, 0, block_size},
                {"extent-size",          required_argument, 0, extent_size},
                {"active-data-extents",  required_argument, 0, active_data_extents},
                {"cores",                required_argument, 0, 'c'},
                {"slices",               required_argument, 0, 's'},
                {"file",                 required_argument, 0, 'f'},
#ifdef SEMANTIC_SERIALIZER_CHECK
                {"semantic-file",        required_argument, 0, 'S'},
#endif
                {"max-cache-size",       required_argument, 0, 'm'},
                {"log-file",             required_argument, 0, 'l'},
                {"port",                 required_argument, 0, 'p'},
                {"verbose",              no_argument, (int*)&config->verbose, 1},
                {"create",               no_argument, &do_create_database, 1},
                {"force",                no_argument, &do_force_create, 1},
                {"help",                 no_argument, &do_help, 1},
                {0, 0, 0, 0}
            };

        int option_index = 0;
        int c = getopt_long(argc, argv, "vc:s:f:S:m:l:p:h", long_options, &option_index);

        if (do_help)
            c = 'h';
        if (do_create_database)
            c = create_database;
        if (do_force_create)
            c = force_create;
     
        /* Detect the end of the options. */
        if (c == -1)
            break;
     
        switch (c)
        {
        case 0:
            break;
        case 'v':
            config->verbose = true;
            break;
        case 'p':
            config->port = atoi(optarg);
            break;
        case 'l':
            strncpy(config->log_file_name, optarg, MAX_LOG_FILE_NAME);
            break;
        case 'c':
            config->n_workers = atoi(optarg);
            // Subtract one because of utility cpu
            if(config->n_workers > MAX_CPUS - 1) {
                fail("Maximum number of CPUs is %d\n", MAX_CPUS - 1);
                abort();
            }
            break;
        case 's':
            config->store_static_config.btree.n_slices = atoi(optarg);
            if(config->store_static_config.btree.n_slices > MAX_SLICES) {
                fail("Maximum number of slices is %d", MAX_SLICES);
            }
            break;
        case 'f':
            {
                if (private_configs.size() >= MAX_SERIALIZERS) {
                    fail("Cannot use more than %d files.", MAX_SERIALIZERS);
                }
                struct log_serializer_private_dynamic_config_t db_info;
                db_info.db_filename = optarg;
#ifdef SEMANTIC_SERIALIZER_CHECK
                db_info.semantic_filename = std::string(optarg) + DEFAULT_SEMANTIC_EXTENSION;
#endif
                private_configs.push_back(db_info);
                break;
            }
#ifdef SEMANTIC_SERIALIZER_CHECK
        case 'S':
            {
                if (private_configs.size() == 0) {
                    fail("You can specify the semantic file name only after specifying a database file name.");
                }
                assert(private_configs.size() <= MAX_SERIALIZERS);
                private_configs.back().semantic_filename = optarg;
                break;
            }
#endif
        case 'm':
            config->store_dynamic_config.cache.max_size = atoll(optarg) * 1024 * 1024;
            break;
        case wait_for_flush:
        	if (strcmp(optarg, "y")==0) config->store_dynamic_config.cache.wait_for_flush = true;
        	else if (strcmp(optarg, "n")==0) config->store_dynamic_config.cache.wait_for_flush = false;
        	else fail("wait-for-flush expects 'y' or 'n'");
            break;
        case flush_timer:
            if (strcmp(optarg, "disable")==0) config->store_dynamic_config.cache.flush_timer_ms = NEVER_FLUSH;
            else {
                config->store_dynamic_config.cache.flush_timer_ms = atoi(optarg);
                check("flush timer should not be negative; use 'disable' to allow changes "
                    "to sit in memory indefinitely",
                    config->store_dynamic_config.cache.flush_timer_ms < 0);
            }
            break;
        case flush_threshold:
            config->store_dynamic_config.cache.flush_threshold_percent = atoi(optarg);
            break;
        case gc_range: {
            float low = 0.0;
            float high = 0.0;
            int consumed = 0;
            if (3 != sscanf(optarg, "%f-%f%n", &low, &high, &consumed) || ((size_t)consumed) != strlen(optarg)) {
                usage(argv[0]);
            }
            if (!(MIN_GC_LOW_RATIO <= low && low < high && high <= MAX_GC_HIGH_RATIO)) {
                fail("gc-range expects \"low-high\", with %f <= low < high <= %f",
                     MIN_GC_LOW_RATIO, MAX_GC_HIGH_RATIO);
            }
            config->store_dynamic_config.serializer.gc_low_ratio = low;
            config->store_dynamic_config.serializer.gc_high_ratio = high;
            break;
        }
        case active_data_extents:
            config->store_dynamic_config.serializer.num_active_data_extents = atoi(optarg);
            if (config->store_dynamic_config.serializer.num_active_data_extents < 1 ||
                config->store_dynamic_config.serializer.num_active_data_extents > MAX_ACTIVE_DATA_EXTENTS) {
                fail("--active-data-extents must be less than or equal to %d", MAX_ACTIVE_DATA_EXTENTS);
            }
            break;
        case block_size:
            config->store_static_config.serializer.block_size = atoi(optarg);
            if (config->store_static_config.serializer.block_size % DEVICE_BLOCK_SIZE != 0) {
                fail("--block-size must be a multiple of %d", DEVICE_BLOCK_SIZE);
            }
            if (config->store_static_config.serializer.block_size <= 0 || config->store_static_config.serializer.block_size > DEVICE_BLOCK_SIZE * 1000) {
                fail("--block-size value is not reasonable.");
            }
            break;
        case extent_size:
            config->store_static_config.serializer.extent_size = atoi(optarg);
            if (config->store_static_config.serializer.extent_size <= 0 || config->store_static_config.serializer.extent_size > TERABYTE) {
                fail("--extent-size value is not reasonable.");
            }
            break;
        case create_database:
            config->create_store = true;
            config->shutdown_after_creation = true;
            break;
        case force_create:
            config->force_create = true;
            break;
        case 'h':
            usage(argv[0]);
            break;
     
        default:
            /* getopt_long already printed an error message. */
            usage(argv[0]);
        }
    }

    if (optind < argc) {
        fail("Unexpected extra argument: \"%s\"", argv[optind]);
    }
    
    /* "Idiot mode" -- do something reasonable for novice users */
    
    if (private_configs.empty() && !config->create_store) {        
        struct log_serializer_private_dynamic_config_t db_info;
        db_info.db_filename = DEFAULT_DB_FILE_NAME;
#ifdef SEMANTIC_SERIALIZER_CHECK
        db_info.semantic_filename = std::string(DEFAULT_DB_FILE_NAME) + DEFAULT_SEMANTIC_EXTENSION;
#endif
        private_configs.push_back(db_info);
        
        int res = access(DEFAULT_DB_FILE_NAME, F_OK);
        if (res == 0) {
            /* Found a database file -- try to load it */
            config->create_store = false;   // This is redundant
        } else if (res == -1 && errno == ENOENT) {
            /* Create a new database */
            config->create_store = true;
            config->shutdown_after_creation = false;
        } else {
            fail("Could not access() path \"%s\": %s", DEFAULT_DB_FILE_NAME, strerror(errno));
        }
    }
    
    /* Sanity-check the input */
    
    if (private_configs.empty()) {
        fail("You must explicitly specify one or more paths with -f.");
    }
    
    if (config->store_dynamic_config.cache.wait_for_flush == true &&
        config->store_dynamic_config.cache.flush_timer_ms == NEVER_FLUSH &&
        config->store_dynamic_config.cache.flush_threshold_percent != 0) {
    	printf("WARNING: Server is configured to wait for data to be flushed\n"
               "to disk before returning, but also configured to wait\n"
               "indefinitely before flushing data to disk. Setting wait-for-flush\n"
               "to 'no'.\n\n");
    	config->store_dynamic_config.cache.wait_for_flush = false;
    }
    
    if (config->store_static_config.serializer.extent_size % config->store_static_config.serializer.block_size != 0) {
        fail("Extent size (%d) is not a multiple of block size (%d).", 
            config->store_static_config.serializer.extent_size,
            config->store_static_config.serializer.block_size);
    }
    
    if (config->store_static_config.serializer.extent_size == config->store_dynamic_config.serializer.file_zone_size) {
        printf("WARNING: You made the extent size the same as the file zone size.\n"
               "This is not a big problem, but it is better to use a huge or\n"
               "unlimited zone size to get the effect you probably want.\n");
    }
}

/* Printing the configuration */
void print_runtime_flags(cmd_config_t *config) {
    printf("--- Runtime ----\n");
    printf("Threads............%d\n", config->n_workers);
    
    printf("Block cache........%lldMB\n", config->store_dynamic_config.cache.max_size / 1024 / 1024);
    printf("Wait for flush.....");
    if(config->store_dynamic_config.cache.wait_for_flush) {
        printf("Y\n");
    } else {
        printf("N\n");
    }
    printf("Flush timer........");
    if(config->store_dynamic_config.cache.flush_timer_ms == NEVER_FLUSH) {
        printf("Never\n");
    } else {
        printf("%dms\n", config->store_dynamic_config.cache.flush_timer_ms);
    }

    printf("Active writers.....%d\n", config->store_dynamic_config.serializer.num_active_data_extents);
    printf("GC range...........%g - %g\n",
           config->store_dynamic_config.serializer.gc_low_ratio,
           config->store_dynamic_config.serializer.gc_high_ratio);
    
    printf("Port...............%d\n", config->port);
}

void print_database_flags(cmd_config_t *config) {
    printf("--- Database ---\n");
    printf("Slices.............%d\n", config->store_static_config.btree.n_slices);
    printf("Block size.........%ldKB\n", config->store_static_config.serializer.block_size / KILOBYTE);
    printf("Extent size........%ldKB\n", config->store_static_config.serializer.extent_size / KILOBYTE);
    
    const std::vector<log_serializer_private_dynamic_config_t>& private_configs = config->store_dynamic_config.serializer_private;
    
    for (size_t i = 0; i != private_configs.size(); i++) {
        const log_serializer_private_dynamic_config_t& db_info = private_configs[i];
        printf("File %.2u............%s\n", (uint) i + 1, db_info.db_filename.c_str());
#ifdef SEMANTIC_SERIALIZER_CHECK
        printf("Semantic file %.2u...%s\n", (uint) i + 1, db_info.semantic_filename.c_str());
#endif
    }
}

void print_system_spec(cmd_config_t *config) {
    printf("--- Hardware ---\n");
    // CPU and RAM
    printf("CPUs...............%d\n" \
           "Total RAM..........%ldMB\nFree RAM...........%ldMB (%.2f%%)\n",
           get_cpu_count(),
           get_total_ram() / 1024 / 1024,
           get_available_ram() / 1024 / 1024,
           (double)get_available_ram() / (double)get_total_ram() * 100.0f);
    // TODO: print CPU topology
    // TODO: print disk and filesystem information
}

void print_config(cmd_config_t *config) {
    if(!config->verbose)
        return;
    
    print_runtime_flags(config);
    printf("\n");
    print_database_flags(config);
    printf("\n");
    print_system_spec(config);
}

