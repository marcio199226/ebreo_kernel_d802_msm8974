
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/i2c.h>

#include "tcpal_os.h"
#include "tcc353x_hal.h"
#include <linux/time.h>
#include <linux/ktime.h>

#if defined (_MODEL_F9J_)
#define CPU_AFFINITY
#endif

/*                      */
/*                          */
I32U Tcc353xInterruptProcess(void);
void Tcc353xInterruptGetStream(I32U _fifoSize);

#ifdef CPU_AFFINITY
static int cpu_state = 0;
#endif

#if !defined (_I2C_STS_)
struct spi_device *TCC_GET_SPI_DRIVER(void);
#endif

typedef struct _TcbdIrqData_t
{
#ifdef USE_TCC_WORK_QUEUE
	struct work_struct work;
	struct workqueue_struct *workQueue;
#endif	
	TcpalSemaphore_t lock;
#if !defined (_I2C_STS_)
	int tcbd_irq;
#endif
	int isIrqEnable;
} TcbdIrqData_t;

static TcbdIrqData_t TcbdIrqData;
extern TcpalSemaphore_t Tcc353xDrvSem;

#ifdef USE_TCC_WORK_QUEUE
static void TcbdInterruptWork(struct work_struct *_param)
{
	I32U fifoSize;
#ifdef INT_TIME_CHECK
	ktime_t st, et, delta_t;
	long delta;
#endif
	/*                                                                                    */
	
#ifdef INT_TIME_CHECK
	st = ktime_get();
#endif
	TcpalSemaphoreLock(&Tcc353xDrvSem);

	if (TcbdIrqData.isIrqEnable == 0) {
		  TcpalPrintErr((I08S *)"[MMB_DEBUG] TcbdInterruptWork_irqDisabled\n");
		  TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		  return;
	}
	
	fifoSize = Tcc353xInterruptProcess();
	/*                              */

	if(fifoSize)
		Tcc353xInterruptGetStream(fifoSize);
#ifdef INT_TIME_CHECK
	et = ktime_get();
	delta_t = ktime_sub(et, st);
	delta = (ktime_to_ns(delta_t) >> 10);
	if(delta>(50*1000))
		TcpalPrintStatus((I08S *)"[MMB_DEBUG]delta %ld fifo [%d]\n", delta, fifoSize);
#endif
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
}

static irqreturn_t TcbdIrqHandler(int _irq, void *_param)
{
	TcbdIrqData_t *irqData = (TcbdIrqData_t *)_param;
	/*                                      */

	if (TcbdIrqData.isIrqEnable)
		//                                               
		TcbdInterruptWork(&irqData->work);

	return IRQ_HANDLED;
}
#else

#if !defined (_I2C_STS_)
static irqreturn_t TcbdIrqThreadfn(int _irq, void* _param)
{
	I32U fifoSize;
	int old_irq_enable = 0;
#ifdef INT_TIME_CHECK
	ktime_t st, et, delta_t;
	long delta;
#endif
	old_irq_enable = TcbdIrqData.isIrqEnable;

#ifdef INT_TIME_CHECK
	st = ktime_get();
#endif

#ifdef CPU_AFFINITY
	{
		if(cpu_state == 0)
		{
			struct cpumask cpus;
			printk("%s: Enter  Set CPU Affinity only to cpu0\n", __func__);
			cpumask_clear(&cpus);
			cpumask_set_cpu(0, &cpus);		 
			if (sched_setaffinity(current->pid, &cpus))
				printk("%s: tcc() set CPU affinity failed\n",__func__);
			else
				printk("%s: tcc() set CPU affinity Succeed  PID = %d\n",__func__, current->pid);
			cpu_state = 1;

		}
	}
#endif	

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if (TcbdIrqData.isIrqEnable == 0) {
		  TcpalPrintErr((I08S *)"[MMB_DEBUG] TcbdIrqThreadfn_irqDisabled. old_irq_enable[%d]\n",
		  		old_irq_enable);
		  TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		  return IRQ_HANDLED;
	}

	fifoSize = Tcc353xInterruptProcess();
	if(fifoSize)
		Tcc353xInterruptGetStream(fifoSize);

	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
	
#ifdef INT_TIME_CHECK
	et = ktime_get();
	delta_t = ktime_sub(et, st);
	delta = (ktime_to_ns(delta_t) >> 10);
	if(delta>(50*1000))
		TcpalPrintStatus((I08S *)"[MMB_DEBUG]delta %ld [%ld]\n", delta, fifoSize);
#endif
	return IRQ_HANDLED;	
}
#endif
#endif

int TcpalRegisterIrqHandler(void)
{
#if defined (_I2C_STS_)
	TcbdIrqData.isIrqEnable = 0;
#else
	struct spi_device *spi = TCC_GET_SPI_DRIVER();
	TcbdIrqData.isIrqEnable = 0;
	TcbdIrqData.tcbd_irq = spi->irq;
#endif

#if !defined (_I2C_STS_)
#ifdef USE_TCC_WORK_QUEUE
	TcbdIrqData.workQueue = create_singlethread_workqueue("tcc353x_work");
	INIT_WORK(&TcbdIrqData.work, TcbdInterruptWork);

	TcpalPrintStatus((I08S *)"[%s] irq:%d\n", __func__, TcbdIrqData.tcbd_irq);
	return  request_irq(TcbdIrqData.tcbd_irq, TcbdIrqHandler,
		IRQF_DISABLED | IRQF_TRIGGER_FALLING, "tc3530_stream", &TcbdIrqData);
#else	
	return request_threaded_irq(TcbdIrqData.tcbd_irq, NULL, TcbdIrqThreadfn,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,  "tc3530_stream", &TcbdIrqData);
#endif

#else
	return 0;
#endif
}

int TcpalUnRegisterIrqHandler(void)
{
#if !defined (_I2C_STS_)
	disable_irq(TcbdIrqData.tcbd_irq);
	free_irq(TcbdIrqData.tcbd_irq, NULL);
#ifdef USE_TCC_WORK_QUEUE	
	flush_workqueue(TcbdIrqData.workQueue);
	destroy_workqueue(TcbdIrqData.workQueue);
#endif	
#endif	
	return 0;
}

int TcpalIrqEnable(void)
{
	TcbdIrqData.isIrqEnable = 1;
#if !defined (_I2C_STS_)
	enable_irq(TcbdIrqData.tcbd_irq);
#endif
	return 0;
}

int TcpalIrqDisable(void)
{
#if !defined (_I2C_STS_)
	disable_irq_nosync(TcbdIrqData.tcbd_irq);
#endif
	TcbdIrqData.isIrqEnable = 0;
	return 0;
}

