/*
 * crypto-chrdev.c
 *
 * Implementation of character devices
 * for virtio-crypto device 
 * 
 * Giorgos Vavouliotis <nuovocominzio@hotmail.com>
 * 03112083
 *
 */
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>

#include "crypto.h"
#include "crypto-chrdev.h"
#include "debug.h"

#include "cryptodev.h"

/*
 * Global data
 */
struct cdev crypto_chrdev_cdev;
#define KEY_SIZE 16 

#define IV	"0123456789ABCDEF"
#define KEY	"0123456789ABCDEF"
#define MSG_LEN 256

spinlock_t chdevlock;

/**
 * Given the minor number of the inode return the crypto device 
 * that owns that number.
 **/
static struct crypto_device *get_crypto_dev_by_minor(unsigned int minor)
{
	struct crypto_device *crdev;
	unsigned long flags;

	debug("Entering ");

	/* i lock here because a dont want someone enter 
	   here and delete the device with rmmod
	*/
	spin_lock_irqsave(&crdrvdata.lock, flags);
	list_for_each_entry(crdev, &crdrvdata.devs, list) {
		if (crdev->minor == minor) goto out;
	}
	crdev = NULL;

out:
	spin_unlock_irqrestore(&crdrvdata.lock, flags);

	debug(" Leaving");
	return crdev;
}

/*************************************
 * Implementation of file operations
 * for the Crypto character device
 *************************************/

static int crypto_chrdev_open(struct inode *inode, struct file *filp)
{
	int ret = 0, host_fd = -1;
	/* posa 8a steilw kai den 8a peiraxtoun */	
	unsigned int num_in = 0;
	/* posa 8a steilw kai 8a peiraxtoun */		
	unsigned int num_out = 0;
	unsigned long flags;
	unsigned int len;
	unsigned int syscall_type = VIRTIO_CRYPTO_SYSCALL_OPEN;
	struct crypto_open_file *cropenfile;
	struct crypto_device *crdev;
	/* sg list for syscall_type */	
	struct scatterlist sglist_syscall_type;
	/* sg list for host_fd(i get it from the host) */		
	struct scatterlist sglist_host_fd;
	/* 2 cell array with pointers to sg lists */	
	struct scatterlist *sgs[2];
	
	debug("Entering to open");

	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto fail;

	/* Associate this open file with the relevant crypto device. */
        /* Given the minor number of the inode return the crypto device that owns that number. */
	crdev = get_crypto_dev_by_minor(iminor(inode));
	if (!crdev) {
		debug("Could not find crypto device with %u minor",iminor(inode));
		ret = -ENODEV;
		goto fail;
	}

        /*The memory is set to zero with kzalloc. */
	cropenfile = kzalloc(sizeof(*cropenfile), GFP_KERNEL);
	if (!cropenfile) {
		ret = -ENOMEM;
		goto fail;
	}

	/* enhmerwsh pediwn */
	cropenfile->crdev = crdev;
	cropenfile->host_fd = -1;
	filp->private_data = cropenfile;

	/* bazw sthn sg list sglist_syscall_type to syscall_type to opoio einai
	   num_out dhladh den 8a peiraxtei h timh tou.
	*/ 
	 sg_init_one(&sglist_syscall_type,&syscall_type,sizeof(syscall_type)); 
	 sgs[num_out++] = &sglist_syscall_type;
	 
	/* bazw sthn sg list sglist_host_fd to host_fd to opoio einai
	   num_in dhladh 8a allaksei h timh tou.
	*/
	 sg_init_one(&sglist_host_fd,&host_fd,sizeof(host_fd));
	 sgs[num_out + num_in++] = &sglist_host_fd;

	/* spinlock => lock here */
	spin_lock_irqsave(&chdevlock, flags); 

	/* here add data to the virtqueue */ 
	virtqueue_add_sgs(crdev->vq,sgs,num_out,num_in,&sglist_syscall_type,GFP_ATOMIC); //ret_value GFP ATOMIC ************************

        /* the data are placed ==> inform with kick */ 	
	virtqueue_kick(crdev->vq);
	
	/* here i do nothing. I wait for the host to proccess the data */
	while (virtqueue_get_buf(crdev->vq,&len) == NULL );

	/* unlock because i have the fd of the host crypto device. */
	spin_unlock_irqrestore(&chdevlock,flags); //time to unlock
	
	debug( "The host returned fd: %d", host_fd );

	/* store the fd of the host device */
    	cropenfile->host_fd = host_fd;

	/* If host failed to open() return -ENODEV. */
	if (cropenfile->host_fd == -1){ 
		ret = -ENODEV;
	}	

fail:
	debug("Leaving from open");
	return ret;
}

static int crypto_chrdev_release(struct inode *inode, struct file *filp)
{
	int ret = 0, host_fd = -1;
	unsigned long flags;
	unsigned int len;
	unsigned int syscall_type = VIRTIO_CRYPTO_SYSCALL_CLOSE;
	/* posa 8a steilw kai den 8a peiraxtoun */	
	unsigned int num_out = 0;
	/* posa 8a steilw kai 8a peiraxtoun */
	unsigned int num_in = 0;	
	struct crypto_open_file *cropenfile = filp->private_data;
	struct crypto_device *crdev = cropenfile->crdev;
	/* sg list for syscall_type */	
	struct scatterlist sglist_syscall_type;
	/* sg list for host_fd(i get it from the host) */		
	struct scatterlist sglist_host_fd;
	/* 2 cell array with pointers to sg lists */	
	struct scatterlist *sgs[2];

	debug("Entering Release");

	host_fd = cropenfile->host_fd;
	
	/* bazw sthn sg list sglist_syscall_type to syscall_type to opoio einai
	   num_out dhladh den 8a peiraxtei h timh tou.
	*/ 
	 sg_init_one(&sglist_syscall_type,&syscall_type,sizeof(syscall_type)); 
	 sgs[num_out++] = &sglist_syscall_type;

	/* bazw sthn sg list sglist_host_fd to host_fd to opoio einai
	   num_out dhladh den 8a allaksei h timh tou.
	*/
	 sg_init_one(&sglist_host_fd,&host_fd,sizeof(host_fd));
	 sgs[num_out++] = &sglist_host_fd;


	/* lock */
	spin_lock_irqsave(&chdevlock, flags); 

	/* here add data to the virtqueue */ 
	virtqueue_add_sgs(crdev->vq,sgs,num_out,num_in,&sglist_syscall_type,GFP_ATOMIC);

	/* the data are placed ==> inform with kick */ 
	virtqueue_kick(crdev->vq);
	
	/* here i do nothing. I wait for the host to proccess the data */
	while (virtqueue_get_buf(crdev->vq,&len) == NULL );
	 	
	/* unlock */
	spin_unlock_irqrestore(&chdevlock,flags);

	/* The fd is closed. */
	debug( "The host closed the fd: %d", host_fd );

	kfree(cropenfile);
	debug("Leaving");
	return ret;

}

static long crypto_chrdev_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
	int host_fd, host_return_val=-1;
	long ret = 0,ret_value;
	unsigned long flags;
	unsigned char output_msg[MSG_LEN], input_msg[MSG_LEN];
	unsigned char *session_key, *src,*dst,*iv;
	unsigned int num_out=0, num_in=0, syscall_type = VIRTIO_CRYPTO_SYSCALL_IOCTL,len;	
	u_int32_t sess_ses,ses_id,key_size;
	u_int16_t oper;
	u_int msg_len;

	/* Structs */
	struct crypto_open_file *cropenfile = filp->private_data;
	struct crypto_device *crdev = cropenfile->crdev;
	struct virtqueue *vq = crdev->vq;
	struct session_op sess,*session_pointer;
    	struct crypt_op cryp,*cryp_pointer;

	/* All the scatterlists */ 
	struct 	scatterlist  sglist_syscall_type;
	struct 	scatterlist  sglist_fd;
	struct 	scatterlist  sglist_request;
	struct 	scatterlist  sglist_return_val;
	
	/* CIOCGSESSION scatterlists */
	struct 	scatterlist  sglist_session_op;
	struct 	scatterlist session_key_list;
	struct 	scatterlist sglist_keysize;

	/* CIOCFSESSION scatterlist */
	struct 	scatterlist  sglist_sess_id;

	/* CIOCCRYPT scatterlists */
	struct 	scatterlist  sglist_outmsg;
	struct 	scatterlist sglist_inmsg;
	struct 	scatterlist sglist_iv;
	struct 	scatterlist sglist_crypto;
	struct 	scatterlist sglist_operation;
	struct scatterlist sglist_msglen;

	struct scatterlist *sgs[10];
	
	/* Iniatilizations */
	session_pointer = NULL;
	cryp_pointer = NULL;
	dst=NULL;
	host_fd = cropenfile->host_fd ;

	debug("Entering Ioctl");

	/* bazw sthn sg list sglist_syscall_type to syscall_type to opoio einai
	   num_out dhladh den 8a peiraxtei h timh tou.
	*/
	sg_init_one(&sglist_syscall_type, &syscall_type, sizeof(syscall_type));
	sgs[num_out++] = &sglist_syscall_type; //sg_out[0]
	
	/* bazw sthn sg list sglist_fd to host_fd to opoio einai
	   num_out dhladh den 8a peiraxtei h timh tou.
	*/
	sg_init_one(&sglist_fd,&host_fd,sizeof(host_fd));
	sgs[num_out++] = &sglist_fd; // sg_out[1]
	
	/* omoia */
	sg_init_one(&sglist_request,&cmd,sizeof(cmd));
	sgs[num_out++] = &sglist_request;

	switch (cmd) {
	case CIOCGSESSION:
		/* Start a new session */
		debug("CIOCGSESSION");
		
		session_pointer = (struct session_op*)arg;
		/* Returns number of bytes that could not be copied. On success, this will be zero. */
		ret_value = copy_from_user(&sess, session_pointer , sizeof(struct session_op));
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);		

		session_key = kmalloc(sess.keylen+1, GFP_KERNEL);
		
		ret_value = copy_from_user(session_key, session_pointer->key, session_pointer->keylen*sizeof(unsigned char));
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		session_key[sess.keylen]='\0';
		
		key_size = session_pointer->keylen;
		
		/* bazw sthn sg list sglist_session_key to session_key to opoio einai
	   	   num_out dhladh den 8a peiraxtei h timh tou. Omoia gemizw kai tis 
		   epomenes sg lists.
		*/
		sg_init_one(&session_key_list,session_key,sess.keylen);
		sgs[num_out++] = &session_key_list; 
		

		sg_init_one(&sglist_keysize,&key_size,sizeof(key_size));
		sgs[num_out++] = &sglist_keysize;
		
		/* host_return_val will show if the open succeeded */
		sg_init_one(&sglist_return_val,&host_return_val,sizeof(host_return_val));
		sgs[num_out + num_in++] = &sglist_return_val; 
		
		sg_init_one(&sglist_session_op,&sess,sizeof(sess));
		sgs[num_out + num_in++] = &sglist_session_op;	

		break;

	case CIOCFSESSION: 
		/* Close the session */
		debug("CIOCFSESSION");
				
		ret_value = copy_from_user(&sess_ses, (u_int32_t *)arg , sizeof(sess_ses));
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		sg_init_one(&sglist_sess_id,&sess_ses,sizeof(sess_ses));
		sgs[num_out++] = &sglist_sess_id; //sg_out[3];

		/* ret_val_host will show if the close succeeded */		
		sg_init_one(&sglist_return_val,&host_return_val,sizeof(host_return_val));
		sgs[num_out+num_in++] = &sglist_return_val; //sg_in[0];			
		
		break;

	case CIOCCRYPT: 
		/* Start a crypto session */
		debug("CIOCCRYPT");

		cryp_pointer = (struct crypt_op * ) arg;
	
		ret_value = copy_from_user(&cryp, cryp_pointer , sizeof(struct crypt_op));
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		src = kmalloc(cryp.len,GFP_KERNEL);

		ret_value = copy_from_user(src, cryp.src , cryp.len);
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		iv = kmalloc(sizeof(cryp.iv)*2,GFP_KERNEL);

		ret_value = copy_from_user(iv,cryp.iv,sizeof(cryp.iv)*2);
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);
		
		oper = cryp.op;
		dst = kmalloc(cryp.len,GFP_KERNEL);
		msg_len = cryp.len;
		
		sg_init_one(&sglist_crypto,&cryp,sizeof(struct crypt_op));
		sgs[num_out++] = &sglist_crypto; 
		
		sg_init_one(&sglist_inmsg,src,cryp.len * sizeof(unsigned char));
		sgs[num_out++] = &sglist_inmsg; 	
		
		sg_init_one(&sglist_iv,iv,sizeof(cryp.iv)*2);
		sgs[num_out++] = &sglist_iv; 	
		
		sg_init_one(&sglist_operation,&oper,sizeof(oper));
		sgs[num_out++] = &sglist_operation;
		
		sg_init_one(&sglist_msglen,&msg_len,sizeof(msg_len));
		sgs[num_out++]=&sglist_msglen;
		
		sg_init_one(&sglist_return_val,&host_return_val,sizeof(host_return_val));
		sgs[num_out+num_in++] = &sglist_return_val; ;
		
		sg_init_one(&sglist_outmsg,dst,cryp.len * sizeof(unsigned char));
		sgs[num_out + num_in++] = &sglist_outmsg;
		
		break;

	default:
		debug(" Unsupported ioctl command");

		break;
	}

	/* lock */
	spin_lock_irqsave(&chdevlock, flags); 

	/* add the data to the virtqueue */
	virtqueue_add_sgs(crdev->vq, sgs, num_out, num_in , &sglist_syscall_type, GFP_ATOMIC);
	
	/* inform that the data are placed */		
	virtqueue_kick(crdev->vq);

	/* do nothing until you get the data from the host */
	while (virtqueue_get_buf(crdev->vq, &len) == NULL);
	spin_unlock_irqrestore(&chdevlock, flags); 
	
	/* analoga me to cmd 8a prepei na steilw ta data ston user */
	switch (cmd) {
	case CIOCGSESSION: 
		debug("CIOCGSESSION SEND DATA TO USER");
		ret_value = copy_to_user (&(session_pointer->ses),&(sess.ses),sizeof (u_int32_t));
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		if (host_return_val < 0){
			/* FAIL */
			return -1;
		}	
		break;

	case CIOCFSESSION: //CLOSE
		debug("CIOCFSESSION SEND DATA TO USER");
		if (host_return_val < 0){
			/* FAIL */
			return -1;
		}		
		break;

	case CIOCCRYPT: //CRYPTO
		debug("CIOCCRYPT SEND DATA TO USER");
		if (host_return_val < 0){
			/* FAIL */
			return -1;
		}	
		ret_value = copy_to_user(cryp_pointer->dst,dst,cryp.len);
		if( ret_value != 0) debug("I didn't copy %lu\n bytes", ret_value);

		break;

	default:
		debug("Unsupported ioctl command");
		break;
	}
	
	debug("Leaving");

	return ret;
}


static ssize_t crypto_chrdev_read(struct file *filp, char __user *usrbuf, 
                                  size_t cnt, loff_t *f_pos)
{
	debug("Entering");
	debug("Leaving");
	return -EINVAL;
}


static struct file_operations crypto_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = crypto_chrdev_open,
	.release        = crypto_chrdev_release,
	.read           = crypto_chrdev_read,
	.unlocked_ioctl = crypto_chrdev_ioctl,
};


int crypto_chrdev_init(void)
{
	int ret;
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;
	
	debug("Initializing character device...");
	cdev_init(&crypto_chrdev_cdev, &crypto_chrdev_fops);
	crypto_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	ret = register_chrdev_region(dev_no, crypto_minor_cnt, "crypto_devs");
	if (ret < 0) {
		debug("failed to register region, ret = %d", ret);
		goto out;
	}
	ret = cdev_add(&crypto_chrdev_cdev, dev_no, crypto_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device");
		goto out_with_chrdev_region;
	}

	debug("Completed successfully");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
out:
	return ret;
}
/*No changes made*/
void crypto_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;

	debug("entering");
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	cdev_del(&crypto_chrdev_cdev);
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
	debug("leaving");
}
