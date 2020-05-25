#!/bin/bash
echo Script para ejecutar assoofs de pana
entrada=0
salida=4
while test $entrada -ne $salida
do
	echo 
	echo Introduce una opción para realizar la acción deseada:
	echo 1 - Cleanear
	echo 2 - Makear
	echo 3 - Montar
	echo 4 - Salir
	read entrada
	echo  
	case $entrada in
		1)	
			make clean
		;;
		2)
			make
		;;
		3)
			echo sin hacer
		;;
		4)
			echo Saliendo del programa
		;;
		*)
			echo Introduce un valor entre 1 y 4
			echo Relanzando menu
		;;
	esac
done
