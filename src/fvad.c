/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *  Copyright (c) 2016 Daniel Pirch.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../include/fvad.h"

#include <stdlib.h>
#include <string.h>
#include "vad/vad_core.h"
#include <stdio.h>
#define __disable_irq()  
#define __enable_irq()

typedef struct Circle_Queue
{
    uint8_t *data;    //队列数据
    uint32_t size;    //队列大小
    uint32_t head;    //队列头部
    uint32_t tail;    //队列尾部
    uint32_t len;     //队列有效数据长度
}circle_queue_struct;

/**
  * @biref 环形队列初始化
  * @param  circle_queue_struct队列结构体
            data队列缓冲区
            size队列大小
  * @retval 0成功 1失败
  */
int circle_queue_init(circle_queue_struct *queue,
                       uint8_t* data,
                       uint32_t size)
{
    if(data==NULL || size<2)
        return -1;
    queue->data=data;
    queue->len=0;
    queue->size=size;
    queue->head=0;
    queue->tail=0;
    return 0;
}

/**
  * @biref 环形队列擦除
  * @param  circle_queue_struct队列结构体
  * @retval 0成功 1失败
  */
int circle_queue_erase(circle_queue_struct *queue)
{
    if(queue==NULL)
        return -1;

    __disable_irq();
    //queue->data=NULL;
    queue->len=0;
    queue->size=0;
    queue->head=0;
    queue->tail=0;
    __enable_irq();
    return 0;
}

/**
  * @biref 获取环形队列有效数据长度
  * @retval 环形队列长度
  */
unsigned int circle_queue_len(circle_queue_struct *queue)
{
    return queue->len;
}

/**
  * @biref 环形队列入队操作
  * @param  circle_queue_struct队列结构体
            data入队数据数组指针
            len入队数据长度
  * @retval -1错误指针为空 -2入队数据过长非法 -3队列当前空闲大小不足  -4队列满 0成功
  */
int circle_queue_in(circle_queue_struct *queue,
                    uint8_t *data,
                    uint32_t len)
{
  int ret=0;
  
  __disable_irq();
  
  if(queue==NULL || data==NULL)
  {
    ret=-1;
    goto out;
  }
  if(queue->len >= queue->size)
  {
    ret=-2;
    goto out;
  }
  if(len>(queue->size - queue->len))
  {
    ret=-3;
    goto out;
  }
  
  for(uint32_t i=0;i<len;i++)
  {
    if(queue->len >= queue->size)
    {
      ret=-4;
      goto out;
    }
    queue->data[queue->tail]=*data++;
    queue->tail = (++queue->tail) % (queue->size);
    queue->len++;
  }

out:
  __enable_irq();
  return ret;
}

/**
  * @biref 环形队列出队操作
  * @param  circle_queue_struct队列结构体
            data出队接收数据数组指针
            len需要出队数
  * @retval -1错误指针为空 其他：出队数据字节数
  */
int circle_queue_out(circle_queue_struct *queue,
                     uint8_t *data,
                     uint32_t len)
{
  int ret=0;
  
  __disable_irq();
  
  if(queue==NULL || data==NULL)
  {
    ret=-1;
    goto out;
  }
  if(queue->len==0)
  {
    ret=0;
    goto out;
  }
  if(queue->len<len)
    len=queue->len;
  
  for(uint32_t i=0;i<len;i++)
  {
    *data = queue->data[queue->head];
    data++;
    ret++;
    queue->head=(++queue->head) % queue->size;
    queue->len--;
  }
  
out:
  __enable_irq();
  return ret;
}

/**
  * @biref 环形队列预览数据指令
  * @param  circle_queue_struct队列结构体
            data预览接收数据数组指针
            len需要预览的数据字节数
            offset预览起始数据偏移队列头字节数
  * @note 注意此操作不会出队数据
  * @retval -1错误指针为空 -2有效数据长度不够 其他：出队数据字节数
  */
int circle_queue_preview(circle_queue_struct *queue,
                         uint8_t *data,
                         uint32_t len,
                         uint32_t offset)
{
  int ret=0;
  uint32_t queue_head=0;
  
  __disable_irq();
  if(queue==NULL || data==NULL)
  {
    ret=-1;
    goto out;
  }
  if(queue->len==0)
  {
    ret=-2;
    goto out;
  }
  if(queue->len < (offset+len))
  {
    ret=-3;
    goto out;
  }
  
  queue_head=(queue->head+offset) % queue->size;
  for(uint32_t i=0;i<len;i++)
  {
    *data = queue->data[queue_head];
    data++;
    ret++;
    queue_head++;
    queue_head %= queue->size;
  }
  
out:
  __enable_irq();
  return ret;
}

/**
  * @biref 环形队列删除数据指令
  * @param  circle_queue_struct队列结构体
            len需要删除的数据字节数
  * @retval -1错误指针为空 其他：实际删除数据字节数
  */
int circle_queue_delete_data(circle_queue_struct *queue,
                             uint32_t len)
{
  int ret=0;
  
  __disable_irq();
  if(queue==NULL)
  {
    ret=-1;
    goto out;
  }
  if(queue->len<len)
    len=queue->len;
  queue->head=(queue->head+len)%queue->size;
  queue->len-=len;
  
  ret=len;
out:
  __enable_irq();
  return ret;
}



static uint8_t g_queue[1024+319];
circle_queue_struct queue_entity;

static uint8_t bos_queue[1024 * 64];
circle_queue_struct bos_entity;

// valid sample rates in kHz
static const int valid_rates[] = { 8, 16, 32, 48 };

// VAD process functions for each valid sample rate
static int (*const process_funcs[])(VadInstT*, const int16_t*, size_t) = {
    WebRtcVad_CalcVad8khz,
    WebRtcVad_CalcVad16khz,
    WebRtcVad_CalcVad32khz,
    WebRtcVad_CalcVad48khz,
};

// valid frame lengths in ms
static const size_t valid_frame_times[] = { 10, 20, 30 };


struct Fvad {
    VadInstT core;
    size_t rate_idx; // index in valid_rates and process_funcs arrays
    size_t count;
    size_t speech_count;
    size_t speech_time;
    size_t time;
    size_t addbos;
    FVAD_CB cb;
};

#define save_file 0
#if save_file
FILE *fp;
#endif

Fvad *fvad_new(void)
{
	Fvad *inst = malloc(sizeof *inst);
	if (inst) fvad_reset(inst);
	inst->count = 0;
	inst->addbos = 0;
	inst->speech_count = 0;
	inst->cb = NULL;
	circle_queue_init(&queue_entity,
			g_queue,
			1024+319);
	circle_queue_init(&bos_entity,
			bos_queue,
			1024*64);
#if save_file
	time_t t;
	char t_buf[1024] = {0};
	time(&t);
	ctime_r(&t, t_buf);
	printf("file name: %s\n", t_buf);
	if ((fp = fopen(t_buf,"w")) < 0)
		printf("open sound.pcm fial\n");
#endif
	return inst;
}


void fvad_free(Fvad *inst)
{
    assert(inst);
    free(inst);
}


void fvad_reset(Fvad *inst)
{
    assert(inst);

    int rv = WebRtcVad_InitCore(&inst->core);
    assert(rv == 0);
    inst->rate_idx = 1;
    //fvad_set_mode(inst, 3);
}


int fvad_set_mode(Fvad* inst, int mode)
{
    assert(inst);
    int rv = WebRtcVad_set_mode_core(&inst->core, mode);
    assert(rv == 0 || rv == -1);
    return rv;
}


int fvad_set_sample_rate(Fvad* inst, int sample_rate)
{
    assert(inst);
    for (size_t i = 0; i < arraysize(valid_rates); i++) {
        if (valid_rates[i] * 1000 == sample_rate) {
            inst->rate_idx = i;
            return 0;
        }
    }
    return -1;
}

static bool valid_length(size_t rate_idx, size_t length)
{
    int samples_per_ms = valid_rates[rate_idx];
    for (size_t i = 0; i < arraysize(valid_frame_times); i++) {
        if (valid_frame_times[i] * samples_per_ms == length)
            return true;
    }
    return false;
}


int fvad_feed(Fvad *inst, const char *buffer, size_t size)
{
	static int flag = 0;
	if (-3 == circle_queue_in(&bos_entity, buffer, size)){
		char bbbb[1024];
		circle_queue_out(&bos_entity, (uint8_t *)bbbb, size);
		circle_queue_in(&bos_entity, buffer, size);
	}

	int s, y, rv = 0, i;
	uint8_t buf[320] = {0};
	uint16_t bb[160] = {0};
	circle_queue_in(&queue_entity, buffer, size);
	s = queue_entity.len/ 320;
	//printf("s: %d, len: %d\n", s, queue_entity.len);
	for (i = 0; i < s; i++ ) {
		//queue_get(buf, 160);
		circle_queue_out(&queue_entity, (uint8_t *)buf, 320);
		memcpy(bb, buf, 320);
		rv = fvad_process(inst, bb, 160);
		if (rv == 0) {
			printf("detect silent.................\n");
			if (inst->cb != NULL)
				inst->cb(rv, buffer, size);
			flag = 0;
#if save_file
			int rc = fwrite(buffer, 1, size, fp);  
			if (rc != size) {
				fprintf(stderr,  "short write: wrote %d bytes/n", rc);  
			}
			fclose(fp);
#endif
			goto out;
		}
		if (rv == 1) {
			printf("detect speech.................\n");
			if (inst->cb != NULL) {
				char bbb[1024*64] = {0};
				int s = 16 * 2 * inst->speech_time + 1024;
				if (s > 1024 * 64) s = 1024 * 64 + 1024;
				circle_queue_out(&bos_entity, (uint8_t *)bbb, s);
				inst->cb(rv, bbb, 1024*64);
				circle_queue_erase(&bos_entity);
			}
			flag = 1;
#if save_file
			int rc = fwrite(buffer, 1, size, fp);  
			if (rc != size) {
				fprintf(stderr,  "short write: wrote %d bytes/n", rc);  
			}
#endif
			goto out;
		}
	}

out:
	if (rv == 2) {
		printf("rv 2 \n");
		if (inst->cb != NULL) {
			if (flag == 1) {
				inst->cb(rv, buffer, size);
#if save_file
				int rc = fwrite(buffer, 1, size, fp);  
				if (rc != size) {
					fprintf(stderr,  "short write: wrote %d bytes/n", rc);  
				}
#endif
			}
		}
	}
	return rv;
}

int fvad_process(Fvad* inst, const int16_t* frame, size_t length)
{
	static int once = 0;
	assert(inst);
	if (!valid_length(inst->rate_idx, length))
		return -1;

	int rv = process_funcs[inst->rate_idx](&inst->core, frame, length);
	assert (rv >= 0);
	if (rv > 0) {
		rv = 2;
		if (once == 0) {
			inst->speech_count++;
			if (inst->speech_count >= inst->speech_time) {
				inst->speech_count = 0;
				rv = 1;
				once++;
				//fvad_getbos_buffer(inst, getbos_buffer, 1024*96);
			}
		} else { //printf("goto detect silent...");
		}
	} else {
		if (inst->addbos == 1) {
			if ((once%2) == 1) {
				printf("slient...\n");
				/*sum the count*/
				inst->count++;
				if (inst->count >= inst->time) {
					inst->count = 0;
					once = 0;
					//circle_queue_erase(&queue_entity);
					//memset(g_queue, 0, 1024+319);
				} else {
					rv = 2;
				}
			} else {
				rv = 2;
			}
		} else {
			printf("slient...\n");
			/*sum the count*/
			inst->count++;
			if (inst->count >= inst->time) {
				inst->count = 0;
				//circle_queue_erase(&queue_entity);
				//memset(g_queue, 0, 1024+319);
			} else {
				rv = 2;
			}
		}
	}

	if (rv == 1)
		printf("speech start\n");
	if (rv == 0)
		printf("detect silent\n");
	return rv;
}

int fvad_settime(Fvad* inst, size_t t)
{
	int retval = 0;
	inst->time = t;
	return retval;
}

int fvad_setspeechtime(Fvad* inst, size_t t)
{
	int retval = 0;
	inst->speech_time = t;
	return retval;
}

int fvad_callback(Fvad *inst, FVAD_CB cb)
{
	inst->cb = cb;
	return 0;
}

int fvad_setfunc(Fvad *inst, int addbos)
{
	inst->addbos = addbos;
	return 0;
}
