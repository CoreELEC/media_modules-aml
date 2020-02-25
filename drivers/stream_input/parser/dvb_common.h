/*
 * ../hardware/amlogic/media_modules/drivers/stream_input/parser/dvb_common.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __DVB_COMMON_H__
#define __DVB_COMMON_H__

#include <linux/amlogic/aml_dtvdemod.h>

#include "dvb_frontend.h"

typedef enum dmx_source {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3,

#ifdef CONFIG_AMLOGIC_DVB_COMPAT
	DMX_SOURCE_FRONT0_OFFSET = 100,
	DMX_SOURCE_FRONT1_OFFSET,
	DMX_SOURCE_FRONT2_OFFSET
#endif
} dmx_source_t;

enum aml_dmx_id_t {
	AM_DMX_0 = 0,
	AM_DMX_1,
	AM_DMX_2,
	AM_DMX_MAX,
};

enum aml_ts_source_t {
	AM_TS_SRC_TS0,
	AM_TS_SRC_TS1,
	AM_TS_SRC_TS2,
	AM_TS_SRC_TS3,

	AM_TS_SRC_S_TS0,
	AM_TS_SRC_S_TS1,
	AM_TS_SRC_S_TS2,
	AM_TS_SRC_S_TS3,

	AM_TS_SRC_HIU,
	AM_TS_SRC_HIU1,
	AM_TS_SRC_DMX0,
	AM_TS_SRC_DMX1,
	AM_TS_SRC_DMX2
};


extern int dvb_attach_tuner(struct dvb_frontend *fe, struct aml_tuner *tuner,
		enum tuner_type *type);
extern int dvb_detach_tuner(enum tuner_type *type);
extern struct dvb_frontend *dvb_attach_demod(struct demod_config *cfg,
		enum dtv_demod_type *type, const char *name);
extern int dvb_detach_demod(enum dtv_demod_type *type);

/* For compile and load warnings */
static inline struct dvb_frontend *mxl661_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *si2151_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *si2159_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *r840_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *r842_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *atbm2040_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *atbm253_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c_adap, struct tuner_config *cfg)
{
	return NULL;
}
/* For attach tuner driver end */

static inline struct dvb_frontend *atbm8881_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *si2168_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *si2168_attach_1(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *avl6762_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *atbm7821_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *mxl248_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *cxd2856_attach(
		const struct demod_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *aml_dtvdm_attach(
		const struct demod_config *cfg)
{
	return NULL;
}
/* For attach demod driver end */

#endif /* __DVB_COMMON_H__ */
