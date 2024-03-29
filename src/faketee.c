#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crash_dump.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tee_drv.h>
#include <linux/tee.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

static void faketee_get_version(struct tee_device *tee_device,
				struct tee_ioctl_version_data *ver)
{
	pr_info("[fake_tee]: Requested version\n");
	ver->impl_id = 1234;
	ver->impl_caps = 0xcafebabe;
	ver->gen_caps = 0xdeadb33f;
}

static int faketee_open(struct tee_context *ctx)
{
	pr_info("[fake_tee]: faketee_open was called\n");

	return 0;
}

static void faketee_release(struct tee_context *ctx)
{
	pr_info("[fake_tee]: faketee_release was called\n");
}

static int faketee_open_session(struct tee_context *ctx,
				struct tee_ioctl_open_session_arg *arg,
				struct tee_param *param)
{
	pr_info("[fake_tee]: faketee_open_session was called\n");

	pr_info("[fake_tee]: requested uuid is '%x%x%x%x-%x%x-%x%x-%x%x-%x%x%x%x%x%x\n",
		arg->uuid[0], arg->uuid[1], arg->uuid[2], arg->uuid[3],
		arg->uuid[4], arg->uuid[5], arg->uuid[6], arg->uuid[7],
		arg->uuid[8], arg->uuid[9], arg->uuid[10], arg->uuid[11],
		arg->uuid[12], arg->uuid[13], arg->uuid[14], arg->uuid[15]);
	pr_info("[fake_tee]: params number: %u\n", arg->num_params);
	arg->session = 1234;
	arg->ret = 4321;
	arg->ret_origin = 2;
	
	return 0;
}

static int faketee_close_session(struct tee_context *ctx, u32 session)
{
	pr_info("[fake_tee]: faketee_close_session\n");
	return 0;
}

static int faketee_invoke_func(struct tee_context *ctx,
			       struct tee_ioctl_invoke_arg *arg,
			       struct tee_param *param)
{
	int i, j;
	uint8_t *buf;

	pr_info("[fake_tee]: faketee_invoke_func was called\n");
	pr_info("[fake_tee]: session_id is %u, function is %u\n", arg->session, arg->func);
	switch (arg->func) {
	case 1234:
		pr_info("[fake_tee/increment_number]: called\n");
		pr_info("[fake_tee/increment_number]: a=%llu b=%llu c=%llu\n", param->u.value.a, param->u.value.b, param->u.value.c);
		arg->ret = 0;
		arg->ret_origin = 2;
		param->u.value.a++;
		param->u.value.b++;
		param->u.value.c++;

		return 0;
	case 1235:
		pr_info("[fake_tee/memref-test]: called\n");
		arg->ret = 0;
		arg->ret_origin = 2;

		for (i = 0; i < arg->num_params; i++) {
			#define IS_MEMREF(a) (a == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT || a == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT || a == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT)
			if (!IS_MEMREF(param->attr)) {
				pr_info("[fake_tee/memref-test]: arg %d is not a memref\n", i);
				continue;
			}
			buf = &((uint8_t*) param[i].u.memref.shm->kaddr)[param[i].u.memref.shm_offs];
			pr_info("[fake_tee/memref-test]: arg %d has ptr %px and id: %d\n", i, (void*) buf, param[i].u.memref.shm->id);
			if (param->attr == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT) {
				pr_info("[fake_tee/memref-test]: arg %d is memref INPUT\n", i);
				for (j = 0; j < param[i].u.memref.size; j++)
					pr_info("0x%X ", buf[j]);
			} else if (param->attr == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT) {
				pr_info("[fake_tee/memref-test]: arg %d is memref INOUT\n", i);
				for (j = 0; j < param[i].u.memref.size; j++)
					buf[j]++;
			} else if (param->attr == TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT) {
				pr_info("[fake_tee/memref-test]: arg %d is memref OUTPUT\n", i);
				for (j = 0; j < param[i].u.memref.size; j++)
					buf[j] = j;
			}
			unreachable();
		}
		return 0;
		break;
	default:
		pr_info("[fake_tee]: no function is defined with that id\n");
		return ENOTSUPP;
	}
}

static int faketee_cancel_req(struct tee_context *ctx, u32 cancel_id,
			      u32 session)
{
	pr_info("[fake_tee]: faketee_cancel_req was called\n");
	return -4321;
}

static const struct tee_driver_ops tee_ops = {
	.get_version = faketee_get_version,
	.open = faketee_open,
	.release = faketee_release,
	.open_session = faketee_open_session,
	.close_session = faketee_close_session,
	.invoke_func = faketee_invoke_func,
	.cancel_req = faketee_cancel_req,
};

static int faketee_pool_alloc(struct tee_shm_pool_mgr *pool,
			      struct tee_shm *shm, size_t size)
{
	unsigned int order = get_order(size);
	unsigned long va;

	/*
	 * Ignore alignment since this is already going to be page aligned
	 * and there's no need for any larger alignment.
	 */
	va = __get_free_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!va)
		return -ENOMEM;

	shm->kaddr = (void *)va;
	shm->paddr = virt_to_phys((void *)va);
	shm->size = PAGE_SIZE << order;

	return 0;
}

static void faketee_pool_free(struct tee_shm_pool_mgr *pool,
			      struct tee_shm *shm)
{
	free_pages((unsigned long)shm->kaddr, get_order(shm->size));
	shm->kaddr = NULL;
}

static void faketee_pool_destroy(struct tee_shm_pool_mgr *pool)
{
	kfree(pool);
}

static const struct tee_shm_pool_mgr_ops pool_ops = {
	.alloc = faketee_pool_alloc,
	.free = faketee_pool_free,
	.destroy_poolmgr = faketee_pool_destroy,
};

static struct tee_shm_pool_mgr *pool_mem_mgr_alloc(void)
{
	struct tee_shm_pool_mgr *mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);

	if (!mgr)
		return ERR_PTR(-ENOMEM);

	mgr->ops = &pool_ops;

	return mgr;
}

static struct tee_shm_pool *faketee_alloc_shm_pool(void)
{
	struct tee_shm_pool_mgr *priv_mgr;
	struct tee_shm_pool_mgr *dmabuf_mgr;
	void *rc;

	rc = pool_mem_mgr_alloc();
	if (IS_ERR(rc))
		return rc;
	priv_mgr = rc;

	rc = pool_mem_mgr_alloc();
	if (IS_ERR(rc)) {
		tee_shm_pool_mgr_destroy(priv_mgr);
		return rc;
	}
	dmabuf_mgr = rc;

	rc = tee_shm_pool_alloc(priv_mgr, dmabuf_mgr);
	if (IS_ERR(rc)) {
		tee_shm_pool_mgr_destroy(priv_mgr);
		tee_shm_pool_mgr_destroy(dmabuf_mgr);
	}

	return rc;
}

static const struct tee_desc tee_desc = {
	.name = "faketee-clnt",
	.ops = &tee_ops,
	.owner = THIS_MODULE,
};

struct tee_device *tee_device;

static int faketee_driver_init(void)
{
	int rc;
	struct tee_shm_pool *pool;

	pr_info("Initializing Fake-TEE\n");

	pr_info("Allocating the shared memory pool\n");
	pool = faketee_alloc_shm_pool();
	if (!pool) {
		pr_err("fatal while allocating shared memory pool\n");
		return -ENOMEM;
	}

	tee_device = tee_device_alloc(&tee_desc, NULL, pool, NULL);
	if (IS_ERR(tee_device)) {
		pr_err("fatal while allocating the device. err: %ld\n",
		       PTR_ERR(tee_device));
		return PTR_ERR(tee_device);
	}

	rc = tee_device_register(tee_device);
	if (rc) {
		pr_err("fatal while registering the device. err: %d\n", rc);
	}
	return rc;
}
module_init(faketee_driver_init);

void faketee_driver_cleanup(void)
{
	pr_info("[fake_tee]: removing the fake tee");
	tee_device_unregister(tee_device);
}
module_exit(faketee_driver_cleanup);

MODULE_AUTHOR("Marco Cutecchia");
MODULE_DESCRIPTION("Fake-TEE driver");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL v2");
