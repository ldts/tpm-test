#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

const char *zone[] = {
	"/sys/class/tpm/tpm0/owned",
};

const char *back ="\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		  "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		  "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		  "\b\b\b\b\b\b\b\b\b\b\b\b";

const char *log_file = "/tmp/log-owned.txt";

#define TPM_OWNED     0x0

void usage()
{
	printf("This programs read /sys/class/tpm/tpm0/owned\n"
	       "This value will always return 0x0 \n"
	       "The program will log errors reading the value\n"
	       "\n"
	       "  tpm -[e|t|a|r]:\n"
	       "                e: stop on errors\n"
	       "                t: stop on timeouts\n"
	       "                a: stop on errors or timeouts\n"
	       "                r: run (will not stop)\n"
	       "\n"
	       "notice:: logs are captured to /tmp/log-owned.txt\n");
}

int main (int argc, char *argv[])
{
	unsigned long long iterations = 0;
	unsigned int timeouts = 0;
	unsigned int errors = 0;
	unsigned int owned;
	bool stop_on_timeouts = false;
	bool stop_on_errors = false;
	bool stop_on_all = false;
	int options_passed = 0, option_index = 0;
	clock_t start;
	int rc = 0;

	while ((option_index = getopt(argc, argv, "reta")) != -1) {
		options_passed = 1;
		switch(option_index) {
		case 'e':
			printf("Program will stop on errors\n");
			stop_on_errors = true;
			break;
		case 't':
			printf("Program will stop on timeouts\n");
			stop_on_timeouts = true;
			break;
		case 'a':
			printf("Program will stop on all issues\n");
			stop_on_all = true;
			break;
		case 'r':
			printf("Program will not stop on issues\n");
			break;
		default:
			usage();
			return 0;
		}
	}

	if (!options_passed) {
		usage();
		return 0;
	}

	FILE *log = fopen(log_file, "w");
	if (!log)
		return -EINVAL;

	start = clock();
	for (;;) {
		FILE *fp = fopen(zone[0], "r");
		if (!fp)
			return -EINVAL;

		printf("reads: %010llu, timeouts: %04d, errors: %04d, secs: %04d%s",
			iterations, timeouts, errors, (clock() - start)/CLOCKS_PER_SEC, back);
		fflush(stdout);

		iterations++;
		owned = 0xDEAD;
		if (fscanf (fp, "%d", &owned) == 1) {
			if (owned != TPM_OWNED) {
				errors++;
				fprintf(log, "spi read8 failure event:\n"
					" - iterations :%lld\n"
					" - value read : %d\n"
					" - timeouts   : %d\n",
					iterations, owned, timeouts);
				fflush(log);

				if (stop_on_errors || stop_on_all) {
					fprintf(log, "stopping on error\n");
					printf("\nstopping on error!\n");
					return -EIO;
				}

			}
			fclose(fp);
			continue;
		}

		/* timeout due to flow control (recoverable) */
		timeouts++;

		if (ferror(fp))
			rc = errno;
		else
			rc = 0;

		fprintf(log, "spi read8 timeout event:\n"
			" - iterations : %lld\n"
			" - value read : %d ... should be 0 (seed)\n"
			" - timeouts   : %d\n",
			" - error      : %d\n",
			iterations, owned, timeouts, rc);
		fflush(log);

		if (stop_on_timeouts || stop_on_all) {
			fprintf(log, "stopping on timeout\n");
			printf("\nstopping on timeout!\n");
			return -EIO;
		}
	}

	return 0;
}
