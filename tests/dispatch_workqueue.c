#include <bsdtests.h>
#include "dispatch_test.h"

#if defined(__linux__)
// For pthread_getaffinity_np()
#include <pthread.h>
#elif defined(__FreeBSD__)
// for pthread_getaffinity_np / cpu_set_t
#include <pthread.h>
#include <pthread_np.h>
#endif

struct test_context {
	uint32_t ncpu;
	int flag;
};

static void
timeout(void *context)
{
	struct test_context *ctx = (struct test_context *)context;
	sleep(2);  // Give the monitor the best chance of firing.
	test_int32_format(ctx->flag, 1, "flag");
	test_stop();
}

static void
raise_flag(void *context)
{
	struct test_context *ctx = (struct test_context *)context;
	ctx->flag++;
}

static void
spin(void *context)
{
	struct test_context *ctx = (struct test_context *)context;
	sleep(ctx->ncpu * 2);
}

static uint32_t
activecpu(void)
{
        uint32_t activecpu = 0xa1a1a1;
#if defined(__linux__) || defined(__OpenBSD__) || defined(__FreeBSD__)
        activecpu = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);

#if defined(__FreeBSD__) || (defined(__linux__) && __USE_GNU)
        cpu_set_t cpuset;
        if (pthread_getaffinity_np(pthread_self(),
                                   sizeof(cpu_set_t),
                                   &cpuset) == 0)
          activecpu = (uint32_t)CPU_COUNT(&cpuset);
#endif

#elif defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        activecpu = si.dwNumberOfProcessors;
#else
        size_t s = sizeof(activecpu);
        sysctlbyname("hw.activecpu", &activecpu, &s, NULL, 0);
#endif
	return activecpu;
}

struct test_context ctx;

int
main(void)
{
	uint32_t ncpu = activecpu();

	dispatch_test_start("Dispatch workqueue");

	dispatch_queue_t global = dispatch_get_global_queue(0, 0);
	test_ptr_notnull("dispatch_get_global_queue", global);

	ctx.ncpu = ncpu;
	dispatch_async_f(global, &ctx, timeout);

	for(int i = 0; i < (int)ncpu - 1; i++) {
		dispatch_async_f(global, &ctx, spin);
	}

	// All cpus are tied up at this point. Workqueue
	// should execute this function by overcommit.
	dispatch_async_f(global, &ctx, raise_flag);

	dispatch_main();
	return 0;
}
