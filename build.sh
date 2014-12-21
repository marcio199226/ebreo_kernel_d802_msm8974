#!/bin/bash
# If you're using my script, all you need to do is edit the following lines to work for you.
# Final zip will exist in "build" as "furnace-1.0.0-aosp_hammerhead.zip" for example
# If you are using a non-appended dt.image, you will need to add "--dt path/to/dt.img" and
# the appropriate lines for dtbTool to compile it, if not prebuilt.
export ARCH=arm
if [ "$1" == "local" ]; then
	echo "GITHUB Build"
	build=/home/marcio/android/system/kernel/lge/msm8974
	export CROSS_COMPILE=/home/marcio/android/kernel/toolchains/arm-cortex_a15-linux-gnueabihf-linaro_4.9.2-2014.10/bin/arm-cortex_a15-linux-gnueabihf-
else
	echo "TEST Build"
	build=/home/marcio/android/system/kernel/lge/msm8974
	export CROSS_COMPILE=/home/marcio/android/kernel/toolchains/arm-cortex_a15-linux-gnueabihf-linaro_4.9.2-2014.10/bin/arm-cortex_a15-linux-gnueabihf-
fi
kernel="ebreo_Kernel"
version="1.2.9"
variant="g2"
ramdisk=ramdisk
config="cyanogenmod_d802_defconfig"
kerneltype="zImage"
jobcount="-j$(grep -c ^processor /proc/cpuinfo)"
ps=2048
base=0x00000000
ramdisk_offset=0x05000000
tags_offset=0x04800000
cmdline="console=ttyHSL0,115200,n8 androidboot.hardware=g2 user_debug=31 msm_rtb.filter=0x0 mdss_mdp.panel=1:dsi:0:qcom,mdss_dsi_g2_lgd_cmd"

echo "Pick variant..."
select var in d800 d801 d802 ls980 vs980
do
case "$var" in
		"d800")
		variant="d800"
		config="cyanogenmod_d800_defconfig"
		break;;
		"d801")
                variant="d801"
                config="cyanogenmod_d801_defconfig"
                break;;
		"d802")
                variant="d802"
                config="cyanogenmod_d802_defconfig"
                break;;
		"ls980")
                variant="ls980"
                config="cyanogenmod_ls980_defconfig"
                break;;
		"vs980")
                variant="vs980"
                config="cyanogenmod_vs980_defconfig"
                break;;

esac
done

function cleanme {
	if [ -f arch/arm/boot/"$kerneltype" ]; then
		echo "  CLEAN   ozip"
	fi
	rm -rf ozip/boot.img
	rm -rf arch/arm/boot/"$kerneltype"
	mkdir -p ozip/system/lib/modules
	make clean && make mrproper
	echo "Working directory cleaned..."
}

rm -rf out
mkdir out
mkdir out/tmp
echo "Checking for build..."
if [ -f ozip/boot.img ]; then
	read -p "Previous build found, clean working directory..(y/n)? : " cchoice
	case "$cchoice" in
		y|Y )
			cleanme;;
		n|N )
			exit 0;;
		* )
			echo "Invalid...";;
	esac
	read -p "Begin build now..(y/n)? : " dchoice
	case "$dchoice" in
		y|Y)
			make "$config"			
			make "$jobcount"
			exit 0;;
		n|N )
			exit 0;;
		* )
			echo "Invalid...";;
	esac
fi
echo "Extracting files..."
if [ -f arch/arm/boot/"$kerneltype" ]; then
	cp arch/arm/boot/"$kerneltype" out/
	rm -rf ozip/system
	mkdir -p ozip/system/lib/modules
	find . -name "*.ko" -exec cp {} ozip/system/lib/modules \;
	if [ "$(ls -A ozip/system/lib/modules)" ]; then
		echo "Modules found."
	else
		echo "No modules"
		rm -rf ozip/system
	fi
else
	echo "Nothing has been made..."
	read -p "Clean working directory..(y/n)? : " achoice
	case "$achoice" in
		y|Y )
			cleanme;;
		n|N )
			exit 0;;
		* )
			echo "Invalid...";;
	esac
	read -p "Begin build now..(y/n)? : " bchoice
	case "$bchoice" in
		y|Y)
			make "$config"
			make "$jobcount"
			exit 0;;
		n|N )
			exit 0;;
		* )
			echo "Invalid...";;
	esac
fi

echo "Making dt.img..."
if [ -f arch/arm/boot/"$kerneltype" ]; then
	./dtbToolCM -o out/dt.img -s 2048 -p scripts/dtc/ arch/arm/boot/
	echo "dt.img created"
else
	echo "No build found..."
	exit 0;
fi

echo "Making ramdisk..."
if [ -d $ramdisk ]; then
	./mkbootfs $ramdisk | gzip > out/ramdisk.gz
else
	echo "No $ramdisk directory found..."
	exit 0
fi

echo "Making boot.img..."
if [ -f out/"$kerneltype" ]; then
	./mkbootimg --kernel out/"$kerneltype" --ramdisk out/ramdisk.gz --cmdline "$cmdline" --base $base --pagesize $ps --ramdisk_offset $ramdisk_offset --tags_offset $tags_offset --dt out/dt.img --output ozip/boot.img
else
	echo "No $kerneltype found..."
	exit 0
fi

echo "Bumping..."
if [ -f arch/arm/boot/"$kerneltype" ]; then
	python2 open_bump.py ozip/boot.img
	rm ozip/boot.img
 	cp ozip/boot_bumped.img ozip/boot.img
	rm ozip/boot_bumped.img
echo "Zipping..."
	cd ozip
	zip -r ../"$kernel"-"$version"_"$variant"_signed.zip .
	mv ../"$kernel"-"$version"_"$variant"_signed.zip $build
	cd ..
	rm -rf out ozip/system
	echo "Done..."
	echo "Output zip: $build/$kernel-$version_$variant_signed.zip"
	exit 0;
else
	echo "No $kerneltype found..."
	exit 0;
fi
# Export script by Savoca -- S2
# Edited by ebreo (aka marcio marcio199226@gmail.com)
