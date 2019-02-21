#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "str_func.h"

#define BUF_SZ 128

/* Preprocessor Directives */
#ifndef DEBUG
#define DEBUG 1
#endif
/**
 * Logging functionality. Set DEBUG to 1 to enable logging, 0 to disable.
 */
#define LOG(fmt, ...) \
do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
    __LINE__, __func__, __VA_ARGS__); } while (0)


/* Function prototypes */
void print_usage(char *argv[]);
void get_system();
void get_hardware();
void get_task_info();
void get_task_list();
int open_func(char* loc);
int close_func(int fd, ssize_t read_sz);
int is_num(const char *str);
char *get_curr_state(char c);

/* This struct is a collection of booleans that controls whether or not the
 * various sections of the output are enabled. */
struct view_opts {
    bool hardware;
    bool system;
    bool task_list;
    bool task_summary;
};

/* This struct is a vessel for our task/process information. */
struct task_info {
    int pid;
    char state[BUF_SZ];
    char name[BUF_SZ];
    char user[BUF_SZ];
    int tasks;
};

void print_usage(char *argv[])
{
    printf("Usage: %s [-ahlrst] [-p procfs_dir]\n" , argv[0]);
    printf("\n");
    printf("Options:\n"
        "    * -a              Display all (equivalent to -lrst, default)\n"
        "    * -h              Help/usage information\n"
        "    * -l              Task List\n"
        "    * -p procfs_dir   Change the expected procfs mount point (default: /proc)\n"
        "    * -r              Hardware Information\n"
        "    * -s              System Information\n"
        "    * -t              Task Information\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    /* Default location of the proc file system */
    char *procfs_loc = "/proc";

    /* Set to true if we are using a non-default proc location */
    bool alt_proc = false;

    struct view_opts all_on = { true, true, true, true };
    struct view_opts options = { false, false, false, false };

    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "ahlp:rst")) != -1) {
        switch (c) {
            case 'a':
            options = all_on;
            break;
            case 'h':
            print_usage(argv);
            return 0;
            case 'l':
            options.task_list = true;
            break;
            case 'p':
            procfs_loc = optarg;
            alt_proc = true;
            break;
            case 'r':
            options.hardware = true;
            break;
            case 's':
            options.system = true;
            break;
            case 't':
            options.task_summary = true;
            break;
            case '?':
            if (optopt == 'p') {
                fprintf(stderr,
                    "Option -%c requires an argument.\n", optopt);
            } else if (isprint(optopt)) {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            } else {
                fprintf(stderr,
                    "Unknown option character `\\x%x'.\n", optopt);
            }
            print_usage(argv);
            return 1;
            default:
            abort();
        }
    }

    if (alt_proc == true) {
        LOG("Using alternative proc directory: %s\n", procfs_loc);

        /* Remove two arguments from the count: one for -p, one for the
         * directory passed in: */
        argc = argc - 2;

    	DIR* dir = opendir(procfs_loc);
    	if (dir) {
    	    closedir(dir);
    	} else {
    	    perror("opendir");
    	    return EXIT_FAILURE;
    	}
    }

    if (argc <= 1) {
        /* No args (or -p only). Enable all options: */
        options = all_on;
    }

    LOG("Options selected: %s%s%s%s\n",
        options.hardware ? "hardware " : "",
        options.system ? "system " : "",
        options.task_list ? "task_list " : "",
        options.task_summary ? "task_summary" : "");

    chdir(procfs_loc);

    if (options.system)
    	get_system();

    if (options.hardware)
    	get_hardware();

    if (options.task_summary)
    	get_task_info();
    
    if (options.task_list)
    	get_task_list();

    return 0;
}

void get_system() {
    /* Variables */
    char buffer[BUF_SZ];
    char hostname[BUF_SZ];
    char kernel[BUF_SZ];
    char uptime_line[BUF_SZ];
    char *temp;
    double uptime;
    int year, day, hour, min, sec;
    int addComma = 0;
    int fd;
    ssize_t read_sz;

    /* initialize buffer */
    memset(buffer, 0, BUF_SZ);

    /* Open hostname file */
    fd = open_func("./sys/kernel/hostname");
    
    /* Read file. If there's something... it's probably
     * the hostname */
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
        char *tok = buffer;
    	strcpy(hostname, next_token(&tok, " "));
    }

    /* Close file descriptor and reset buffer */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* Open osrelease file */
    fd = open_func("./sys/kernel/osrelease");
    /* Read that file... */
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
    	char *tok = buffer;
    	strcpy(kernel, next_token(&tok, " "));
    }

    /* Close file descriptor and reset buffer */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* Open uptime file */
    fd = open_func("./uptime");
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
    	strcpy(uptime_line, buffer);
    }

    /* Close file descriptor */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* grab a copy of the uptime line */
    temp = uptime_line;
    /* tokenize it so we grab only first item */
    uptime = atof(next_token(&temp, " "));

    /* calculate uptime */
    sec = (int) uptime;
    min = (sec / 60) % 60;
    hour = (sec / 3600) % 24;
    day = (sec / 86400) % 365;
    year = sec / 31536000;
    sec = sec % 60;

    /* print stuff */
    printf("System Information\n");
    printf("------------------\n");
    printf("Hostname: %s", hostname);
    printf("Kernel Version: %s", kernel);
    printf("Uptime:");

    /* checking for any specifics for output */
    if (year != 0) {
    	printf(" %d years", year);
    	addComma = 1;
    }
    if (day != 0 && addComma) {
    	printf(", %d days", day);
    	addComma = 1;
    } else if (day != 0) {
    	printf(" %d days", day);
    	addComma = 1;
    }
    if (hour != 0 && addComma) {
    	printf(", %d hours", hour);
    	addComma = 1;
    } else if (hour != 0) {
    	printf(" %d hours", hour);
    	addComma = 1;
    }
    if (addComma) {
    	printf(", %d minutes", min);
    } else {
    	printf(" %d minutes", min);
    }

    printf(", %d seconds", sec);
    printf("\n\n");
}

void get_hardware() {
    /* Variables */
    char buffer[BUF_SZ], model[BUF_SZ], load_avg[BUF_SZ];
    int processing_units;
    double cpu_usage;
    long double cpu_total, cpu_total_1, cpu_total_2;
    double cpu_idle, cpu_idle_1, cpu_idle_2;
    double mem_total, mem_active;
    int fd;
    ssize_t read_sz;

    /* initialize buffer */
    memset(buffer, 0, BUF_SZ);
    memset(load_avg, 0, BUF_SZ);

    /* Open CPU info file */
    fd = open_func("./cpuinfo");
    while ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
        /* these look for the lines we're trying to get */
    	char *model_line = strstr(buffer, "model name");
    	char *siblings_line = strstr(buffer, "siblings");
        /* if string exists, that means we found it! */
    	if (model_line) {
            /* tokenize */
    	    char *tok = model_line;
    	    char *next = next_token(&tok, ":");
    	    next = next_token(&tok, ":\n");
            /* grabbing the goods */
    	    strcpy(model, &next[1]);
    	}
    	if (siblings_line) {
            /* tokenize */
    	    char *tok = siblings_line;
    	    char *next = next_token(&tok, ":");
    	    next = next_token(&tok, " ");
            /* calculate processing units */
    	    processing_units = atoi(next) * 2;
    	    break;
    	}
    }

    /* close the file */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* open load average file */
    fd = open_func("./loadavg");
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
        /* tokenize and grab information */
    	char *tok = buffer;
    	strcat(load_avg, next_token(&tok, " "));
    	strcat(load_avg, " ");
    	strcat(load_avg, next_token(&tok, " "));
    	strcat(load_avg, " ");
    	strcat(load_avg, next_token(&tok, " "));
    }

    /* close file */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* open file */
    fd = open_func("./stat");
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
        /* tokenize */
    	char *tok = buffer;
    	next_token(&tok, " ");
        /* add up totals */
    	int i = 0;
    	while (i++ < 9) {
    	    char *num = next_token(&tok, " ");
    	    cpu_total_1 += strtod(num, NULL);
    	    if (i == 4)
    		cpu_idle_1 = atof(num);
    	}
    }

    /* close file  + sleep */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);
    sleep(1);

    /* open stat file */
    fd = open_func("./stat");
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
    	/* tokenize */
        char *tok = buffer;
    	next_token(&tok, " ");
    	/* add up totals */
        int i = 0;
    	while (i++ < 9) { 
    	    char *num = next_token(&tok, " ");
    	    cpu_total_2 += strtod(num, NULL);
    	    if (i == 4)
    		cpu_idle_2 = atof(num);
    	}
    }

    /* close file */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* compare totals and get cpu usage */
    cpu_total = cpu_total_2 - cpu_total_1;
    cpu_idle = cpu_idle_2 - cpu_idle_1;
    cpu_usage = 1 - (cpu_idle / cpu_total);
    
    /*printf("CPU Total: %Lf\nCPU Idle: %f\nCPU Usage: %f\n", 
	    cpu_total, cpu_idle, cpu_usage);*/

    /* open meminfo file */
    fd = open_func("./meminfo");
    if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
        /* tokenize */
    	char *tok = buffer;
    	char *next = next_token(&tok, " ");
    	next = next_token(&tok, " ");
        /* get mem total */
    	mem_total = atof(next);
        /* reset buffer */
    	memset(buffer, 0, BUF_SZ);
        /* read once more to get active mem */
    	if ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
    	    tok = buffer;
    	    next = next_token(&tok, "\n"); 
    	    next = next_token(&tok, "\n");
    	    next = next_token(&tok, " ");
    	    next = next_token(&tok, " ");
    	    mem_active = atof(next);
    	}
    }

    /* convert! */
    mem_total /= (1024 * 1024);
    mem_active /= (1024 * 1024);

    /* close */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* print stuff */
    printf("Hardware Information\n");
    printf("--------------------\n");
    printf("CPU Model: %s\n", model);
    printf("Processing Units: %d\n", processing_units);
    printf("Load Average (1/5/15 min): %s\n", load_avg);
    printf("CPU Usage:    [");
    /* hashes */
    int hashes = cpu_usage / 0.05;
    int i = 0;
    while (i < 20)
    	if (i++ < hashes)
    	    printf("#");
    	else
    	    printf("-");

    /* check if cpu_usage is nan */
    if (!isnan(cpu_usage))
        printf("] %.1f%%\n", cpu_usage * 100);
    else
    	printf("] 0.0%%\n");

    /* print more stuff */
    printf("Memory Usage: [");
    hashes = (mem_active / mem_total) / 0.05;

    /* hashes */
    i = 0;
    while (i < 20)
        if (i++ < hashes)
            printf("#");
        else
            printf("-");

    /* print... */
    printf("] %.1f%% (%.1f GB / %.1f GB)\n\n", 
	    (mem_active / mem_total) * 100, mem_active, mem_total);
}

void get_task_info() {
    DIR *dir, *test;
    struct dirent *ent;
    char buffer[BUF_SZ];
    long int interrupts = 0, c_switches = 0, forks = 0, tasks_running = 0;
    int fd;
    ssize_t read_sz;

    /* initialize buffer */
    memset(buffer, 0, BUF_SZ);

    /* open directory */
    if ((dir = opendir(".")) != NULL) {
        /* read directory */
    	while ((ent = readdir(dir)) != NULL) {
            /* check if directory name is a num */
    	    if (is_num(ent->d_name)) {
        		char curr[128];
        		strcpy(curr, "./");
        		strcat(curr, ent->d_name);
                /* open directory to check if it exists */
        		if ((test = opendir(curr)) != NULL) {
        		    tasks_running += 1;
        		}
                /* close directory */
                closedir(test);
    	    }
    	}
        /* close directory */
    	closedir(dir);
    } else {
    	perror("opendir");
    	return;
    }

    /* open stat file */
    fd = open_func("./stat");

    /* read stat file */
    while ((read_sz = read(fd, buffer, BUF_SZ * 2)) != 0) {
    	char *intr = strstr(buffer, "intr");
    	char *ctxt = strstr(buffer, "ctxt");
    	char *processes = strstr(buffer, "processes");
    	if (intr) {
    	    char *next = next_token(&intr, " ");
    	    next = next_token(&intr, " ");
    	    interrupts = atol(next);
    	}
    	if (ctxt) {
    	    char *next = next_token(&ctxt, " ");
    	    next = next_token(&ctxt, " ");
    	    c_switches = atol(next);
    	}
    	if (processes) {
    	    char *next = next_token(&processes, " "); 
    	    next = next_token(&processes, " ");
    	    forks = atol(next);
    	    break;
    	}
    	memset(buffer, 0, BUF_SZ * 2);
    }

    /* close file */
    close_func(fd, read_sz);
    memset(buffer, 0, BUF_SZ);

    /* print stuff */
    printf("Task Information\n");
    printf("----------------\n");
    printf("Tasks running: %ld\n", tasks_running);
    printf("Since boot:\n");
    printf("    Interrupts: %ld\n", interrupts);
    printf("    Context Switches: %ld\n", c_switches);
    printf("    Forks: %ld\n\n", forks);
}

void get_task_list() {
    /* variables */
    DIR *dir, *test;
    struct dirent *ent;
    char buffer[BUF_SZ];
    int fd;
    ssize_t read_sz;

    /* initialize buffer */
    memset(buffer, 0, BUF_SZ);

    /* formatting things */
    printf("%5s | %12s | %25s | %15s | %s \n",
	"PID", "State", "Task Name", "User", "Tasks");

    /* open proc directory */
    if ((dir = opendir(".")) != NULL) {
        /* read directory */
        while ((ent = readdir(dir)) != NULL) {
            /* check if its a process dir */
            if (is_num(ent->d_name)) {
                /* open that dir */
                char curr[128];
                strcpy(curr, "./");
                strcat(curr, ent->d_name);
                /* check if that dir actually exists */
                if ((test = opendir(curr)) != NULL) {
                    /* open status file */
                    struct task_info ti = { atoi(ent->d_name), "", "", "", 0 };
        		    strcat(curr, "/status");
        		    fd = open_func(curr);
                    /* read status file */
        		    while ((read_sz = read(fd, buffer, BUF_SZ)) != 0) {
            			char *name = strstr(buffer, "Name");
            			char *state = strstr(buffer, "State");
            			char *uid = strstr(buffer, "Uid");
            			char *threads = strstr(buffer, "Threads");
            			if (name) {
                            char *next = next_token(&name, "\t\n");
            			    next = next_token(&name, "\t\n");
				            if (strlen(next) > 24)
                                next[25] = '\0';
            			    strcat(ti.name, next);
            			}
            			if (state) {
            			    char *next = next_token(&state, "\t");
            			    next = next_token(&state, " ");
            			    strcat(ti.state, get_curr_state(next[0]));
                        }		    
            			if (uid) {
            			    char *next = next_token(&uid, "\t");
            			    next = next_token(&uid, "\t");
            			    int id = atoi(next);
            			    struct passwd *pwu = getpwuid((uid_t) id);
            			    if (pwu != NULL) {
            				    strcat(ti.user, pwu->pw_name);
                            }
            			    else {
            				    strcat(ti.user, next);
                                free(pwu);
                            }
                        }
            			if (threads) {
            			    char *next = next_token(&threads, "\t");
            			    next = next_token(&threads, "\t");
            			    ti.tasks = atoi(next);
            			}
                    }

                    /* close status file */
        		    close_func(fd, read_sz);
        		    memset(buffer, 0, BUF_SZ);

                    /* output that */
        		    printf("%5d | %12s | %25s | %15s | %d \n",
        		            ti.pid, ti.state, ti.name, ti.user, ti.tasks);
                }
                usleep(1000);
                /* close process dir */
                closedir(test);
            }
        }
        /* close proc dir */
        closedir(dir);
    } else {
        perror("opendir");
        return;
    }

    printf("\n");
}

int open_func(char *loc) {
    int fd = open(loc, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    return fd;
}

int close_func(int fd, ssize_t read_sz) {
    close(fd);
    if (read_sz == -1) {
        perror("read");
        return EXIT_FAILURE;
    }
    return 0;
}

int is_num(const char *str) {
    int i = 0;
    int size = strlen(str);

    if (size == 0)
	return 0;

    while (i < size)
	   if (isdigit(str[i++]) == 0)
	       return 0;
    return 1;
}

char *get_curr_state(char c) {
    switch (c) {
    	case 'R':
    	    return "running";
    	case 'S':
    	    return "sleeping";
    	case 'D':
    	    return "disk sleep";
    	case 'Z':
    	    return "zombie";
    	case 'T':
    	    return "tracing stop";
        case 't':
            return "tracing stop";
    	case 'X':
    	    return "dead";
    	default:
    	    return "sleeping";
    }
}
