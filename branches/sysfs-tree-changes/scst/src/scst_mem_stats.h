#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)

struct scst_debugfs_attr;

int scst_sgv_pool_debugfs_create(struct sgv_pool *pool);
void scst_sgv_pool_debugfs_remove(struct sgv_pool *pool);
ssize_t sgv_pool_stats_show(void *data, const struct scst_debugfs_attr *attr,
			    char *buffer);
ssize_t sgv_pool_stats_store(void *data, const struct scst_debugfs_attr *attr,
			     const char *buffer, size_t count);
ssize_t sgv_global_stats_show(void *data, const struct scst_debugfs_attr *attr,
			      char *buffer);
ssize_t sgv_global_stats_store(void *data, const struct scst_debugfs_attr *attr,
			       const char *buffer, size_t count);

#else /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/

static inline int scst_sgv_pool_debugfs_create(struct sgv_pool *pool)
{
	return 0;
}

static inline void scst_sgv_pool_debugfs_remove(struct sgv_pool *pool)
{
}

#endif /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/
