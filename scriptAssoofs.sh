#!/bin/bash
echo Script para ejecutar assoofs
entrada=0
salida=3
while test $entrada -ne $salida
do
	echo 
	echo Introduce una opción para realizar la acción deseada:
	echo 1 - Clean
	echo 2 - Make
	echo 3 - Salir
	read entrada
	echo  
	case $entrada in
		1)	
			make clean
      clear
		;;
		2)
			make
		;;
		3)
      		echo Saliendo
		;;
		*)
			echo Introduce un valor entre 1 y 3
			echo Relanzando menu
		;;
	esac
done


#Montar la primera vez
#sudo su
#dd bs=4096 count=100 if=/dev/zero of=image
#./mkassoofs image
#insmod assoofs.ko
#mkdir mnt
#mount -o loop -t assoofs image mnt

#Desmontar y montar
#umount mnt
#rmmod assoofs.ko
#make clean
#make
#./mkassoofs image
#insmod assoofs.ko
#mount -o loop -t assoofs image mnt/
