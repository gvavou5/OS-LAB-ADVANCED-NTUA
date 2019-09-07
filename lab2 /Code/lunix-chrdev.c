/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * Giorgos Vavouliotis
 * A.M. : 03112083
 * e-mail : <nuovocominzio@hotmail.com>
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	uint32_t sensor_timestamp_temp;
	
	/* see that the sensor exists and it is not NULL */ 
	WARN_ON (!(sensor = state->sensor));  

	debug( "quick check for update without lock\n" );
	
	/* put in sensor_timestamp the last update */ 
	sensor_timestamp_temp = sensor->msr_data[state->type]->last_update;	
	
	/* The following return is bogus, just for the stub to compile */
	
	/* here i check if i have to make UPDATE */
	if (state->buf_timestamp < sensor_timestamp_temp){
		/* if returns 1 means that an update is needed */
		debug("I have to make an update guys\n");
		return 1;
	}
	else return 0; 
}

static long convert( long index, enum lunix_msr_enum type )
{
	/* i can't have more than 65535 states */
	WARN_ON( index < 0 || index > 65535 );
	
	/* check the type of the data and take the value of the right lookup table */
	switch ( type )
	{
		case BATT:
			return lookup_voltage[ index ];
			break;
		case TEMP:
			return lookup_temperature[ index ];
			break;
		case LIGHT:
			return lookup_light[ index ];
			break;
		default:	
			printk( KERN_ALERT "there is not lookup table to convert from!\n" );
			return 0;
			break;	
	}
}

static int finalConvert(long val,char *buf)
{
	int counter; /* how many bytes snprintf writes */
	
	/* lookup tables can have also negative values */ 
	if (val>=0) 
		counter = snprintf( buf, LUNIX_CHRDEV_BUFSZ,"%02ld.%03ld\n", val / 1000, val % 1000 );
	else 
		counter = snprintf( buf, LUNIX_CHRDEV_BUFSZ,"-%02ld.%03ld\n", -val / 1000, -val % 1000 );
	
	/* WARN_ON is not neccesary here because 
	 * snprintf doesnt write more than LUNIX_CHRDEV_BUFSZ bytes
	 * but i put it to be sure.
	 */
	WARN_ON( !( counter < LUNIX_CHRDEV_BUFSZ ) );
	return counter;
}


/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	int ret; 
	int need_update; /* the integer need_update informs us if an update is needed */
		
	uint32_t new_data; /* is the newly received data(index to a specific lookuptable) in raw form*/
	uint32_t new_timestamp;	/* is the time that the newly data has been received */

	long converted_data; /* this variable has the data after the convertion */	
	
	unsigned long flags; /* Spinlock flags variable */

	debug("Update Starting\n");
	
	/* take my sensor so i have access to the struct to make the update */
	sensor = state->sensor ; 
	/* check if an update i needed */
	need_update = lunix_chrdev_state_needs_refresh(state);
	
	if (need_update == 1){

		/* I lock the critical section with spinlock. I save interrupt flags,
	 	 * disable interrupts, acquire the spinlock and finally 
		 * restore the interrupts. In this way i avoid deadlocks.		
		 */ 
		spin_lock_irqsave(&sensor->lock,flags);

		/* take the new data and the new timestamp */
		new_data = sensor->msr_data[state->type]->values[0];
		new_timestamp = sensor->msr_data[state->type]->last_update;

		/* i unlock because i leave from the critical section */
		spin_unlock_irqrestore( &sensor->lock, flags );
		
		converted_data = convert( new_data, state->type );
		state->buf_lim = finalConvert(converted_data, state->buf_data);
		state->buf_timestamp = new_timestamp; /* renew the timestamp */
		ret = 0;
	}
	else{
		debug("Update not needed\n");
		ret = -EAGAIN; //this means that i didn't make an update
	}

	debug("Leaving\n");
	return ret;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/
static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	/* We follow the C90 standard and declare everything at the top */
	unsigned sensor_minor; // The minor number passed file special character file
	unsigned sensor_type; // The sensor type is battery(0), themperature(1) or light(2)
	unsigned sensor_id; // Each sensor can measure many things (view type above)
	int ret; // Return value of this function
	
	struct lunix_chrdev_state_struct *dev_state;	

	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0) goto out;
	
	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */

	debug("imajor is: %d, iminor is: %d\n", imajor( inode ), iminor( inode ) );

	/* Obvious Check: Major number must match */
	if (imajor(inode) != 60){
		printk(KERN_ERR "The driver is not suited for major number 60...What???\n");
		ret = -ENODEV;
		goto out;
	}

	
	sensor_minor = iminor(inode); /* get minor number */
	sensor_type = sensor_minor%8; /* get the sensor type */
	sensor_id = sensor_minor / 8; /* find which sensor is */

	if (sensor_minor >= lunix_sensor_cnt*8){
		printk( KERN_ERR "up to %d sensors are supported,with 8 measurements each (maximum)\n", lunix_sensor_cnt );
		ret = -ENODEV;
		goto out;
	}
	
	debug( "sensor type is: %d\n", sensor_type );

        if (sensor_type >= N_LUNIX_MSR){
                printk(KERN_ERR "no sensor for this purpose\n");
                ret = -ENODEV;
                goto out;
        }

	/* Allocate a new Lunix character device private state structure */
	dev_state = kmalloc (sizeof *dev_state, GFP_KERNEL );
	if (!dev_state){
		printk( KERN_ERR "could not allocate memory for private field of file pointer.\n" );
		ret = -ENOMEM;
		goto out;
	}
	
	/* Fill up private data ---> save to my state all the necessary infos about my device  */
	dev_state->type = sensor_type;
	
	/* Note: dev_state sensor is a struct constaining
	 * batt, temp, light together with spinlock and wait queue.
	 */	
	dev_state->sensor = &lunix_sensors[ sensor_id ];

	/* This variable holds the bytes in the buf_data buffer.
	 * Thus, we do not need to zero out the actual buffer.
	 */
	dev_state->buf_lim = 0;
	
	/* Initially the buffer was never updated */
	dev_state->buf_timestamp = 0;

	/* Initially the semaphore (used as mutex) is released.
	 * We know that each open call creates a new file pointer. Why
	 * bother to lock the private data? That is because threaded
	 * applications share the same open file descriptors, unless
	 * stated otherwise.
	 */
	sema_init (&dev_state->lock,1);
	/* Update the open file's private data */
	filp->private_data = dev_state;

out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	/* release is called once every an effective
	 * open call is made. e.g. a fork call does not
	 * allocate memory if not needed, thus release does
	 * not create memory leaks
	 */

	debug( "Freeing the private data struct\n" );

	/* Need to be carefull to call kfree */
	WARN_ON ( !filp->private_data );
	kfree( filp->private_data );

	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;
	size_t count; /* has the number of bytes that the buffer contains */
	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	
	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock */
	if ( down_interruptible(&state->lock) )
	{
		ret = -ERESTARTSYS;
		goto out;
	}

	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */

	if (*f_pos == 0) {
		/* Issue a new read command */
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* EAGAIN is returned only if the cache is empty and new data are not available */
			up(&state->lock);
			debug("it is time to go for a sleep\n");

			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			if (wait_event_interruptible(sensor->wq,lunix_chrdev_state_needs_refresh(state))){
				ret = -ERESTARTSYS;
				goto out;
			}
			
			if (down_interruptible(&state->lock)){
				ret=-ERESTARTSYS;
				goto out;
			}
			debug("now it is time to wake up\n");
		}
	}

	/* Determine the number of cached bytes to copy to userspace */
	count = state->buf_lim - *f_pos;
	
	/* If userspace buffer does not have enough space, 
	 * then fill the buffer with cnt bytes
	 */
	if (count > cnt ){
		count = cnt;
	}
	
	debug( "copying %ld chars starting at position %lu\n",(long) count, (unsigned long)*f_pos );
	
	/* copy to user buffer */
	if (copy_to_user(usrbuf,state->buf_data+(*f_pos),count)){
		ret = -EFAULT;
		goto out_with_lock;
	}
	/* if count !=0 something is wrong */
	ret = count;
	/* the file pointer position is increased by the
	 * same amount */
	*f_pos += count;
	
	/* Auto-rewind on EOF mode? */
	if (*f_pos >= state->buf_lim){
		*f_pos = 0;
	}

out_with_lock:
	up(&state->lock);
out:
	/* Unlock? */
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
    .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */

	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;


	debug("initializing character device\n");
	
	/* initialize the driver */
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops); 
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	/* MKDEV does 20 left shifting and then OR with the minor number */
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);

	/* register the region that you want for device with name lunix */
	ret = register_chrdev_region(dev_no,lunix_minor_cnt,"lunix");
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	

	/* add the device that is represented by 
	 * the 1st operand of the cdev_add,to the system 
         */
	ret = cdev_add(&lunix_chrdev_cdev,dev_no,lunix_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}

	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
