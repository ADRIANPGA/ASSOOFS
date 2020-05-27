#!/bin/bash
echo Script para ejecutar assoofs de pana
entrada=0
salida=6
while test $entrada -ne $salida
do
	echo 
	echo Introduce una opción para realizar la acción deseada:
	echo 1 - Cleanear
	echo 2 - Makear
	echo 3 - Crearlo No
  echo 4 - Remontarlo No
  echo 5 - Ver los logs del kernel
	echo 6 - Salir
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
      echo Solo consulta
			#dd bs=4096 count=100 if=/dev/zero of=image
      #./mkassoofs image
      #sudo su
      #insmod assoofs.ko
      #mkdir mnt
      #mount -o loop -t assofs image mnt
		;;
    4)
      echo Solo consulta
      #./mkassoofs image
      #sudo su
      #insmod assoofs.ko
      #mount -o loop -t assofs image mnt
    ;;
    5)
      dmesg
    ;;
		6)
			echo Saliendo del programa
		;;
		*)
			echo Introduce un valor entre 1 y 4
			echo Relanzando menu
		;;
	esac
done
