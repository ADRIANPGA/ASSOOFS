#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

/*
 *  Operaciones sobre ficheros
 */
ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);


const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Read request\n");
    return 0;
}

ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos) {
    printk(KERN_INFO "Write request\n");
    return 0;
}

/*
 *  Operaciones sobre directorios
 */
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);

const struct file_operations assoofs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = assoofs_iterate,
};

static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    printk(KERN_INFO "Iterate request\n");
    return 0;
}

/*
 *  Operaciones sobre inodos
 */
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);
static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    printk(KERN_INFO "Lookup request\n");
    return NULL;
}


static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    printk(KERN_INFO "New file request\n");
    return 0;
}

static int assoofs_mkdir(struct inode *dir , struct dentry *dentry, umode_t mode) {
    printk(KERN_INFO "New directory request\n");
    return 0;
}

/*
 *  Operaciones sobre el superbloque
 */
static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct buffer_head *bh; // Un struct buffer head es un bloque
    struct assoofs_super_block_info *assoofs_sb; // assoofs superblock info (hecha por nosotros)

    struct inode *root_inode; // Variable necesaria en el paso 4 (Es un inodo)

    printk(KERN_INFO "assoofs_fill_super request\n");

    /* 1.- Leer la información persistente del superbloque del dispositivo de bloques */
    // sb lo recibe assoofs_fill_super como argumento y es un puntero a una variable superbloque en memoria y ASSOOFS_SUPERBLOCK_NUMBER es un numero del 0 al 63 (El del superbloque es 0)
    bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);  
    assoofs_sb = (struct assoofs_super_block_info *)bh->b_data; // En assoofs_sb se mete solo b_data que es (void*) y se castea.
    printk(KERN_INFO "Loaded assoofs sb correctly from disk");

    /* 2.- Comprobar los parámetros del superbloque */
    if(unlikely(assoofs_sb->magic != ASSOOFS_MAGIC)){
        /* Si el numero magic leido del superbloque no es el definido en el .h se lanza un error */
        printk(KERN_ERR "The filesystem is not a assoofs, magic numbers does not match.");
        brelse(bh);
        return -1;
    } else if(unlikely(assoofs_sb->block_size != ASSOOFS_DEFAULT_BLOCK_SIZE)){
        /* Si el tamaño de bloque leido del superbloque no se corresponde con el marcado en el .h se lanza un error */
        printk(KERN_ERR "The block size is not %u, could not iniciate assoofs", ASSOOFS_DEFAULT_BLOCK_SIZE);
        brelse(bh);
        return -1;
    }
    printk(KERN_INFO "assoofs v%llu correctly formatted.\n", assoofs_sb->version);

    /* 3.- Escribir la información persistente leída del dispositivo de bloques en el superbloque sb (memoria), incluído el campo s_op con las operaciones que soporta. */
    sb->s_magic = ASSOOFS_MAGIC;
    sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
    sb->s_op = &assoofs_sops; 
    sb->s_fs_info = assoofs_sb; // Para no tener que hacer tantos accesos a discos se guarda en el campo s.fs.info de sb

    /* 4.- Crear el inodo raíz y asignarle operaciones sobre inodos (i_op) y sobre directorios (i_fop) */
    root_inode = new_inode(sb);
    // Se asignan permisos. Params: inodo, propietario y permisos (S_IFDIR si es directorio o S_IFREG si es fichero)
    inode_init_owner(root_inode, NULL, S_IFDIR); 
    root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER; // Numero de inodo (1)
    root_inode->i_sb = sb;  // Puntero al superbloque
    root_inode->i_op = &assoofs_inode_ops; // Operaciones para trabajar con inodos
    root_inode->i_fop = &assoofs_dir_operations; // Operaciones dependiendo si es dir o file
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode); // Fecha de ultimo acceso, modificacion y creacion
    root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER); // Información persistente del inodo
    sb->s_root = d_make_root(root_inode); // Asignar el inodo a la jerarquia (Solo para el root)

    brelse(bh); // Se libera el buffer_head

    return 0; // Se devuelve un 0 que indica que todo esta bien
}


struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no){
    // Acceder al disco para leer el bloque que contiene el almacen de inodos
    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    struct assoofs_super_block_info *afs_sb = sb->s_fs_info;
    struct assoofs_inode_info *buffer = NULL;
    int i;

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);  
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    /* En bucle se busca en el almacen desde 0 al ultimo inodo si coincide con nuestro parametro */
    for(i = 0; i < afs_sb->inodes_count; i++){
        if(inode_info->inode_no == inode_no){
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }

    brelse(bh); // Se libera el lector
    return buffer; // Se devuelve la info encontrada (si estaba)
} 


/*
 *  Montaje de dispositivos assoofs
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    /* Declaracion variable necesaria */
    struct dentry *ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);

    printk(KERN_INFO "assoofs_mount request\n");

    /* Control de errores */
    if(unlikely(IS_ERR(ret)))
        printk(KERN_ERR "Error ocurred while assoofs mount proccess.\n");
    else
        printk(KERN_INFO "assoofs mounted on %s\n", dev_name);
    
    return ret;
}


/*
 *  assoofs file system type
 */
static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_litter_super,
};

static int __init assoofs_init(void) {
    /* Declaracion variable necesaria */
    int ret;

    printk(KERN_INFO "assoofs_init request\n");
    ret = register_filesystem(&assoofs_type);

    /* Control de errores */
    if(ret != 0)
        printk(KERN_ERR "Failed while assoofs init (Error %d)\n", ret);
    else
        printk(KERN_INFO "assoofs started successfully\n");
        
    return ret;
}


static void __exit assoofs_exit(void) {
    int ret;

    printk(KERN_INFO "assoofs_exit request\n");
    ret = unregister_filesystem(&assoofs_type);

    /* Control de errores */
    if(ret != 0)
        printk(KERN_INFO "Failed while assoofs exits (Error %d)\n", ret);
    else
        printk(KERN_ERR "assoofs stopped correctly\n");
}

module_init(assoofs_init);
module_exit(assoofs_exit);