/*
 * Virtio Crypto Device
 *
 * Implementation of virtio-crypto qemu backend device.
 *
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr> 
 * Vavouliotis Giorgos <nuovocominzio@hotmail.com>
 * 03112083
 */

#include <qemu/iov.h>
#include "hw/virtio/virtio-serial.h"
#include "hw/virtio/virtio-crypto.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>

#define KEY_SIZE 24
#define BLOCK_SIZE 16
#define MSG_SIZE 16384
#define IV	"0123456789ABCDEF"
#define KEY	"0123456789ABCDEF"

#define MYDEBUG 1
#define DEBUG_print(fmt, ...) \
            do { if ( MYDEBUG ) fprintf(stderr, fmt, ## __VA_ARGS__); } while (0)
            
static uint32_t get_features(VirtIODevice *vdev, uint32_t features)
{
	DEBUG_IN();
	return features;
}

static void get_config(VirtIODevice *vdev, uint8_t *config_data)
{
	DEBUG_IN();
}

static void set_config(VirtIODevice *vdev, const uint8_t *config_data)
{
	DEBUG_IN();
}

static void set_status(VirtIODevice *vdev, uint8_t status)
{
	DEBUG_IN();
}

static void vser_reset(VirtIODevice *vdev)
{
	DEBUG_IN();
}

static void vq_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
	DEBUG_IN();
	int ret;	
	unsigned int *syscall_type;
    	unsigned char * key;
	u_int32_t *session_ses,*key_size;
    	u_int16_t *operation;
    	u_int *messaglen;
	VirtQueueElement element;
	struct session_op *sess;
    	struct crypt_op *cryp;
    	
	if (!virtqueue_pop(vq, &element)) {
		DEBUG("No item to pop from VQ");
		return;
	} 

	DEBUG(" I have got an item from VQ");

	/* i take the syscall type that frontend sent */
	syscall_type = element.out_sg[0].iov_base;
	
	switch (*syscall_type) {
	case VIRTIO_CRYPTO_SYSCALL_TYPE_OPEN:	
		DEBUG("VIRTIO_CRYPTO_SYSCALL_TYPE_OPEN");
		/* Try to open the real crypto device */
		int fdo = open(CRYPTODEV_FILENAME,O_RDWR,0);
		if ( fdo < 0 ){
		    perror( "Failed to open(" CRYPTODEV_FILENAME ")" );
	    	}
	     	
	   	/* Store the result into the gather list of host_fd */
	    	mempcpy(element.in_sg[0].iov_base,(char *) &fdo, sizeof(fdo));

	   	DEBUG( " OK with opening.\n" );

		break;

	case VIRTIO_CRYPTO_SYSCALL_TYPE_CLOSE:
		DEBUG("VIRTIO_CRYPTO_SYSCALL_TYPE_CLOSE");
		/* i take the fd of the device which i want to close */
		int * fdcl = element.out_sg[1].iov_base;

	    	if ( (close(*(fdcl))) < 0 ){
		    perror( "Failed to close(" CRYPTODEV_FILENAME ")" );
	    	}
	    	
	   	DEBUG( " OK with closing.\n" );
	
		break;

	case VIRTIO_CRYPTO_SYSCALL_TYPE_IOCTL:
		DEBUG("VIRTIO_CRYPTO_SYSCALL_TYPE_IOCTL");

		/* take the fd and the target request that frontent sent */ 		
		int *fdcr = element.out_sg[1].iov_base;
		unsigned int *request = element.out_sg[2].iov_base;

		switch (*request){
		case CIOCGSESSION: 
			/* OPEN  the real crypto device */
			DEBUG("CIOCGSESSION BACKEND");

			/* take the sess to edit it */
			sess = element.in_sg[1].iov_base;
			/* take the key size and the key */
		    	key = element.out_sg[3].iov_base;
		    	key_size = element.out_sg[4].iov_base;

			/* fill the proper fields to open the real cryptodev */
		    	(*sess).cipher = CRYPTO_AES_CBC;
	    		(*sess).keylen = *key_size;
	    		(*sess).key    = (__u8 __user *)key;

	    		if (ret=ioctl(*fdcr, CIOCGSESSION, sess)) {
				perror("ioctl(CIOCGSESSION)");
				return;
	    		}
	    		
			/* copy the return value to the right place of the element_in */
			mempcpy( element.in_sg[0].iov_base, &ret , sizeof(ret) );
			/* copy the struct session_op to the right place of the element_in */
			mempcpy( element.in_sg[1].iov_base, sess , sizeof(struct session_op) );

			/* all the input elemenets(these that i have to edit) are edited 
			   and placed to the right place */

			break;

		case CIOCFSESSION: 
			/* CLOSE  the device */
			DEBUG("CIOCFSESSION BACKEND");

			/* take the sess to edit it */
			session_ses = element.out_sg[3].iov_base;
			
			if ((ret=ioctl(*fdcr, CIOCFSESSION, session_ses))) {
				perror("ioctl(CIOCFSESSION)");
				return;
			}

			/* copy the return value to the right place of the element_in
			   to know the frontend if close succeeded */
			mempcpy( element.in_sg[0].iov_base, &ret , sizeof(ret) );
			
			break;

		case CIOCCRYPT: 
			/* CRYPTO */
			DEBUG("CIOCCRYPT BACKEND");

			/* take all the necessary to make the encryption/decryption */

			/* num_out data ==> not edit */

			cryp      = element.out_sg[3].iov_base;
			cryp->src = element.out_sg[4].iov_base;
			cryp->iv  = element.out_sg[5].iov_base;
			messaglen   = element.out_sg[7].iov_base;
			cryp->len = *messaglen;			
			/* this show if have to encrypt or decrypt */
			operation =  element.out_sg[6].iov_base; 
			cryp->op = *operation;
			/* num_in data ==> edit */ 
			cryp->dst = element.in_sg[1].iov_base;

			/* classic ioctl() call */
			if (ret = ioctl(*fdcr, CIOCCRYPT, cryp)) {
				perror("ioctl(CIOCCRYPT)");
				return;
			}		
			/* copy the return value to the right place of the element_in
			   to know the frontend if crypto succeeded */
			mempcpy(element.in_sg[0].iov_base, &ret , sizeof(ret) );
			/* copy the destination to the right place of the element_in
			   to know the frontend if the result of crypto */
			mempcpy(element.in_sg[1].iov_base, cryp->dst,cryp->len*sizeof(unsigned char));

			break;

		default:
			DEBUG("Unsupported ioctl command BACKEND");

			break;
		}
		break;

	default:
		DEBUG("Unknown syscall_type");
	}
	virtqueue_push(vq, &element, 0);
	DEBUG_print( "Notify VM" );	
	virtio_notify(vdev, vq);
		
}

static void virtio_crypto_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);

	DEBUG_IN();

    virtio_init(vdev, "virtio-crypto", 13, 0);
	virtio_add_queue(vdev, 128, vq_handle_output);
}

static void virtio_crypto_unrealize(DeviceState *dev, Error **errp)
{
	DEBUG_IN();
}

static Property virtio_crypto_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_crypto_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *k = VIRTIO_DEVICE_CLASS(klass);

	DEBUG_IN();
    dc->props = virtio_crypto_properties;
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);

    k->realize = virtio_crypto_realize;
    k->unrealize = virtio_crypto_unrealize;
    k->get_features = get_features;
    k->get_config = get_config;
    k->set_config = set_config;
    k->set_status = set_status;
    k->reset = vser_reset;
}

static const TypeInfo virtio_crypto_info = {
    .name          = TYPE_VIRTIO_CRYPTO,
    .parent        = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtCrypto),
    .class_init    = virtio_crypto_class_init,
};

static void virtio_crypto_register_types(void)
{
    type_register_static(&virtio_crypto_info);
}

type_init(virtio_crypto_register_types)
