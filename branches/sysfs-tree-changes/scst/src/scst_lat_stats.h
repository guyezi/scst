#if defined(CONFIG_SCST_MEASURE_LATENCY)

int scst_tgt_dev_lat_create(struct scst_tgt_dev *tgt_dev);
void scst_tgt_dev_lat_remove(struct scst_tgt_dev *tgt_dev);
int scst_sess_lat_create(struct scst_session *sess);
void scst_sess_lat_remove(struct scst_session *sess);

#else /*defined(CONFIG_SCST_MEASURE_LATENCY)*/

static inline int scst_tgt_dev_lat_create(struct scst_tgt_dev *tgt_dev)
{
	return 0;
}

static inline void scst_tgt_dev_lat_remove(struct scst_tgt_dev *tgt_dev)
{
}

static inline int scst_sess_lat_create(struct scst_session *sess)
{
	return 0;
}

static inline void scst_sess_lat_remove(struct scst_session *sess)
{
}

#endif /*defined(CONFIG_SCST_MEASURE_LATENCY)*/
