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
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);

/* Funciones auxiliares de create */
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block);
void assoofs_save_sb_info(struct super_block *vsb);
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode);
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    /* Dado un nombre de fichero obtener su identificador
    * Params: parent_inode(puntero al inodo padre), child_dentry(Se usa para pasarle el nombre) y flags sin relevancia 
    * Devuelve una entrada de directorio (dentry) */

    /* Variables necesarias paso 1 */
    struct assoofs_inode_info *parent_info = parent_inode->i_private; // Informacion persistente del padre
    struct super_block *sb = parent_inode->i_sb; // Se saca el superbloque
    struct buffer_head *bh; // Un buffer head para leer un bloque

    /* Variables necesarias paso 2 */
    struct assoofs_dir_record_entry *record;
    struct inode *inode;
    int i;

    /* 1.- Acceder al bloque de disco con el contenido del directorio apuntado por parent_inode */
    bh = sb_bread(sb, parent_info->data_block_number); // Se lee el bloque que contiene la informacion del directorio parent
    printk(KERN_INFO "Lookup request in inode %llu in the block %llu.\n", parent_info->inode_no, parent_info->data_block_number);

    /* 2.- Recorrer este bloque de disco de forma secuencial */
    record = (struct assoofs_dir_record_entry *)bh->b_data;
    for (i = 0; i < parent_info->dir_children_count; i++) { // Se busca hasta dir_children_count
        if (!strcmp(record->filename, child_dentry->d_name.name)) { // Se compara el fichero del puntero actual con el argumento
            // Si son iguales ( el strcmp devuelve 0 si son iguales, por eso el !)
            printk(KERN_INFO "File %s found in inode %llu at pos %d of the dir inode %llu.\n", record->filename, record->inode_no ,i, parent_info->inode_no);
            inode = assoofs_get_inode(sb, record->inode_no); // Guardar la informacion del inodo en cuestion
            inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode); // Le damos padre y modo del inode
            d_add(child_dentry, inode); // Se llama a la funcion que construye el arbol de inodos para que meta este
            return NULL;
        }
        record++;
    }

    /* Si se sale del bucle es que no se encontro el inodo */
    printk(KERN_ERR "Inode with filename %s not found.\n", child_dentry->d_name.name); //Control de errores
    return NULL;
}

static struct inode *assoofs_get_inode(struct super_block *sb, int ino){

    /* El inodo que vamos a rellenar */
    struct inode *inode;
    struct assoofs_inode_info *inode_info;

    /* 1.- Obtener la informacion persistente del inodo */
    inode_info = assoofs_get_inode_info(sb, ino); 

    /* 2.- Crear y asignar el campos al inodo */
    inode = new_inode(sb); // Se crea
    inode->i_ino = ino; // Se le asigna el numero al parametro
    inode->i_sb = sb; // Se le asigna el superbloque de los parametros de funcion
    inode->i_op = &assoofs_inode_ops; // Se le dan las operaciones al inodo (create, lookup y mkdir)
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode); //Asignamos el tiempo actual al tiempo de creacion acceso y modificacion
    inode->i_private = inode_info; // Se guarda en el private la informacion persistente obtenida
    // Segun si es directorio o archivo se le dan las operaciones especiales
    if (S_ISDIR(inode_info->mode))
        inode->i_fop = &assoofs_dir_operations;
    else if (S_ISREG(inode_info->mode))
        inode->i_fop = &assoofs_file_operations;
    else
        printk(KERN_ERR "Unknown inode type.\n"); //Control de errores

    return inode;
}


static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
	/* 
	* Parametros
	* 1- directorio al que va a estar asociado el archivo
	* 2- la entrada en el dir padre de este archivo
	* 3- los permisos
	* el ultimo no se usa
	*/

	/* Variables necesarias */
	/* Paso 1 */
	struct inode *inode;
	struct assoofs_inode_info *inode_info;
	struct super_block *sb;
	uint64_t count;
	/* Paso 2 */
	struct assoofs_inode_info *parent_inode_info;
	struct assoofs_dir_record_entry *dir_contents;
	struct buffer_head *bh; // Un buffer head para leer un bloque
	/* Paso 3 */

	/* 1.- Crear el nuevo inodo */
	sb = dir->i_sb; //Obtengo el superbloque del directorio padre
	count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count; // Obtengo el num inodos de la info persistente del sb
	inode = new_inode(sb); // Se crea el inodo
	if(count >= ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED /* (64) */){
		printk(KERN_ERR "assoofs can not hold more files. (%lld of %d).\n", count, ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED); //Control de errores
		return -EPERM;
	}
	inode->i_ino = count + 1; // Se le asigna el siguiente numero
	inode->i_fop=&assoofs_file_operations; //Es un fichero nunca un directorio (mkdir)
	/* Asignar las propiedades del inodo */
	inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL); // Reservamos memoria
	inode_info->inode_no = inode->i_ino;
	inode_info->mode = mode; // El segundo mode me llega como argumento
	inode_info->file_size = 0;
	inode->i_private = inode_info; // No inode info
	inode_init_owner(inode, dir, mode);
	d_add(dentry, inode);

	if(assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number) != 0){ //Funcion auxiliar que busca un bloque libre para el inodo
		printk(KERN_ERR "assoofs can not hold more files. (%llu of %u).\n", count, ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED); //Control de errores
		return -EPERM;
	} 

	assoofs_add_inode_info(sb, inode_info);

	/* 2.- Modificar el contenido del directorio padre para meter el inodo */
	parent_inode_info = dir->i_private; // Informacion persistente del inodo padre
	bh = sb_bread(sb, parent_inode_info->data_block_number); // Creamos un buffer head para leer el bloque del directorio padre

	dir_contents = (struct assoofs_dir_record_entry *)bh->b_data; 
	dir_contents += parent_inode_info->dir_children_count; // Se avanza los hijos que ya tiene hasta el primer hueco libre
	dir_contents->inode_no = inode_info->inode_no; // Se actualiza el numero de hijos
	strcpy(dir_contents->filename, dentry->d_name.name); // Se copia el nombre
	mark_buffer_dirty(bh); // Se marca como sucio (Indicar al SO que hay que escribirlo al sacarlo de memoria)
	sync_dirty_buffer(bh); // Se sincroniza, todos los cambios que haya en bh se llevan a disco
	brelse(bh); // Se libera el buffer head

	/* 3.- Actualizar la informacion persistente del inodo padre */
	parent_inode_info->dir_children_count++;
	assoofs_save_inode_info(sb, parent_inode_info); // Funcion auxiliar que actualiza la informacion persistente del inodo padre 

    return 0; // Todo ha ido bien 
}

/* FUNCIONES AUXILIARES DE CREATE */
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block){
	struct assoofs_super_block_info *assoofs_sb;
	int i;

	assoofs_sb = sb->s_fs_info; // Obtenemos la informacion persistente del superbloque
	
	for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) // Se empieza por 2 porque los dos primeros bloques son sb y almacen de inodos
		if (assoofs_sb->free_blocks & (1 << i)) //Comprueba si el bit del indice vale 1
			break; // cuando aparece el primer bit 1 en free_block dejamos de recorrer el mapa de bits

	*block = i; // Escribimos el valor de i en la direccion de memoria que vamos a devolver

	assoofs_sb->free_blocks &= ~(1 << i); // Marca que el bloque i ahora es 0 (En memoria)
	assoofs_save_sb_info(sb); // Funcion auxiliar que guarda en disco la parte persistente del sb
	return 0;
}

/* Guardar la informacion persistente del superbloque a disco */
void assoofs_save_sb_info(struct super_block *vsb){
	struct buffer_head *bh; // Se crea un buffer head para leer un bloque
	struct assoofs_super_block *sb; // Informacion persistente del superbloque en memoria

	bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER); // Me traigo de disco el bloque del superbloque
	sb = vsb->s_fs_info; // Cojo la informacion de memoria del parametro
	bh->b_data = (char *)sb; // Sobreescribo los datos de disco con la informacion en memoria

	mark_buffer_dirty(bh); // Se marca como sucio (Indicar al SO que hay que escribirlo al sacarlo de memoria)
	sync_dirty_buffer(bh); // Se sincroniza, todos los cambios que haya en bh se llevan a disco
	brelse(bh); // Se libera el buffer head
}

void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode){
	/*
	* Argumentos
	* Puntero al superbloque
	* Informacion persistente que tiene que llegar al disco (sb->sb_info)
	*/

	struct buffer_head *bh; // Se crea un buffer head para leer un bloque
	struct assoofs_inode_info *inode_info;
	struct assoofs_super_block_info *assoofs_sb = (struct assoofs_super_block_info *)sb->s_fs_info;

	bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER); // Se lee de disco el bloque que contiene el almacen de inodos (1)
	inode_info = (struct assoofs_inode_info *)bh->b_data; // Se guarda en una variable el bloque leido (Apuntando al principio)
	inode_info += assoofs_sb->inodes_count; // Para que apunte al ultimo se avanza el numero de inodos (Apunta justo al final)
	memcpy(inode_info, inode, sizeof(struct assoofs_inode_info)); // Copio de memoria en inode_info en inode parametro

	mark_buffer_dirty(bh); // Se marca como sucio (Indicar al SO que hay que escribirlo al sacarlo de memoria)
	sync_dirty_buffer(bh); // Se sincroniza, todos los cambios que haya en bh se llevan a disco

	assoofs_sb->inodes_count++; // Se aumenta el numero de inodos que se tenia
	assoofs_save_sb_info(sb); // Se llama a la funcion que guarda en disco los cambios del sb
}

int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info){
	struct buffer_head *bh; // Se crea un buffer head para leer un bloque
	struct assoofs_inode_info *inode_pos;

	bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER); // Se lee de disco el bloque que contiene el almacen de inodos (1)
	inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info); // Se usa una funcion auxiliar para buscar este inodo en el almacen

	if(inode_pos != NULL){
		printk(KERN_ERR "assoofs error: Inode could not be finded in inode store.\n");
		return -EPERM;
	}

	memcpy(inode_pos, inode_info, sizeof(*inode_pos)); // Se copia en la posicion la info del parametro
	mark_buffer_dirty(bh); // Se marca como sucio (Indicar al SO que hay que escribirlo al sacarlo de memoria)
	sync_dirty_buffer(bh); // Se sincroniza, todos los cambios que haya en bh se llevan a disco

	return 0; // Todo ha ido bien
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search){
	uint64_t count = 0;

	//Start es la variable iteradora, si no se corresponde es cuando avanzas y count que no se pase del numero de inodos para parar si no se encuentra 
	while (start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count) {
		count++;
		start++;
	}	

	/* Si es el que estamos buscnaod se devuelve, si no NULL */
	if (start->inode_no == search->inode_no)
		return start;
	else
		return NULL;
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
    printk(KERN_INFO "Loaded assoofs sb correctly from disk\n");

    /* 2.- Comprobar los parámetros del superbloque */
    if(unlikely(assoofs_sb->magic != ASSOOFS_MAGIC)){
        /* Si el numero magic leido del superbloque no es el definido en el .h se lanza un error */
        printk(KERN_ERR "The filesystem is not a assoofs, magic numbers does not match.\n");
        brelse(bh);
        return -1;
    } else if(unlikely(assoofs_sb->block_size != ASSOOFS_DEFAULT_BLOCK_SIZE)){
        /* Si el tamaño de bloque leido del superbloque no se corresponde con el marcado en el .h se lanza un error */
        printk(KERN_ERR "The block size is not %u, could not iniciate assoofs\n", ASSOOFS_DEFAULT_BLOCK_SIZE);
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