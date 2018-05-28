/*
 * Created by Le Min 2017/12/12
 */

#include <minos/minos.h>
#include <minos/percpu.h>
#include <minos/irq.h>
#include <minos/mm.h>
#include <asm/arch.h>
#include <virt/vcpu.h>
#include <minos/pm.h>
#include <minos/init.h>
#include <minos/sched.h>
#include <minos/smp.h>
#include <minos/atomic.h>
#include <minos/minos_config.h>

extern void softirq_init(void);
extern void init_timers(void);
extern void hypervisor_init(void);
extern void cpu_idle_task();

extern struct minos_config minos_config;
struct minos_config *mv_config = &minos_config;

struct list_head hook_lists[MINOS_HOOK_TYPE_UNKNOWN];

static void hooks_init(void)
{
	int i;

	for (i = 0; i < MINOS_HOOK_TYPE_UNKNOWN; i++)
		init_list(&hook_lists[i]);
}

int register_hook(hook_func_t fn, enum hook_type type)
{
	struct hook *hook;

	if ((fn == NULL) || (type >= MINOS_HOOK_TYPE_UNKNOWN)) {
		pr_error("Hook info is invaild\n");
		return -EINVAL;
	}

	hook = (struct hook *)malloc(sizeof(struct hook));
	if (!hook)
		return -ENOMEM;

	memset((char *)hook, 0, sizeof(struct hook));
	hook->fn = fn;
	init_list(&hook->list);

	list_add_tail(&hook_lists[type], &hook->list);

	return 0;
}

int do_hooks(struct task *task, void *context, enum hook_type type)
{
	struct hook *hook;

	list_for_each_entry(hook, &hook_lists[type], list)
		hook->fn(task, context);

	return 0;
}

static void parse_memtags(void)
{
	int i;
	size_t size = mv_config->nr_memtag;
	struct memtag *memtags = mv_config->memtags;

	for (i = 0; i < size; i++)
		register_memory_region(&memtags[i]);

}

void parse_resource(void)
{
	parse_memtags();
}

void *get_module_pdata(unsigned long s, unsigned long e,
		int (*check)(struct module_id *module))
{
	int i, count;
	struct module_id *module;

	if (e <= s)
		return NULL;

	count = (e - s) / sizeof(struct module_id);
	if (count == 0)
		return NULL;

	for (i = 0; i < count; i++) {
		module = (struct module_id *)s;
		if (check(module))
			return module->data;

		s += sizeof(struct module_id);
	}

	return NULL;
}

void boot_main(void)
{
	log_init();

	pr_info("Starting mVisor ...\n");

	mm_init();
	mmu_init();

	hooks_init();

	early_init();
	early_init_percpu();

	percpus_init();

	if (get_cpu_id() != 0)
		panic("cpu is not cpu0");

	arch_init();
	arch_init_percpu();

	pcpus_init();
	smp_init();
	irq_init();
	softirq_init();
	init_timers();

	subsys_init();
	subsys_init_percpu();

	module_init();
	module_init_percpu();

	hypervisor_init();

	parse_resource();

	device_init();
	device_init_percpu();

	create_idle_task();
	tasks_init();

	smp_cpus_up();
	sched_init();

	local_irq_enable();

	while (1) {
		sched();
		cpu_idle_task();
	}
}

void boot_secondary(void)
{
	uint32_t cpuid = get_cpu_id();

	pr_info("cpu-%d is up\n", cpuid);

	early_init_percpu();

	arch_init_percpu();

	irq_secondary_init();

	subsys_init_percpu();

	module_init_percpu();

	device_init_percpu();

	create_idle_task();

	sched_init();

	local_irq_enable();

	while (1) {
		sched();
		cpu_idle_task();
	}
}