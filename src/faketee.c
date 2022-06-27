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
	return -4321;
}

static int faketee_close_session(struct tee_context *ctx, u32 session)
{
	pr_info("[fake_tee]: faketee_close_session\n");
	return -4321;
}

static int faketee_invoke_func(struct tee_context *ctx,
			       struct tee_ioctl_invoke_arg *arg,
			       struct tee_param *param)
{
	pr_info("[fake_tee]: faketee_invoke_func was called\n");
	return -4321;
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
