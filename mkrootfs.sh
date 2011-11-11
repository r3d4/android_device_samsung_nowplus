#!/bin/bash

function check_variant()
{
    if [ "$ANDROID" = "" ]; then
        ech"define \$ANDROID first."
        exit 0
    fi
    
    if [ "$PRJROOT" = "" ]; then
        echo "define \$PRJROOT first."
        exit 0
    fi
    
    if [ "$KDIR" = "" ]; then
        echo "define \$KDIR first."
        exit 0
    fi 
    
    DEVICEDIR=$ANDROID/device/samsung/nowplus
    OUTDIR=$DEVICEDIR/out
	ANDROIDROOTFS=$OUTDIR/rootfs
	ANDROIDSYSOUT=$ANDROID/out/target/product/nowplus/system
    rm -rf $OUTDIR
    mkdir -p $OUTDIR
}

function copy_modules()
{
    echo "copying kernel modules."
    
    rm -rf $ANDROIDSYSOUT/lib/modules/
    mkdir -p $ANDROIDSYSOUT/lib/modules/
#	find $KDIR/modules -name '*.ko' -exec cp {} $ANDROIDSYSOUT/lib/modules/  \;
	find $DEVICEDIR/prebuilt/modules -name '*.ko' -exec cp {} $ANDROIDSYSOUT/lib/modules/  \;
	
    #find $PRJROOT/TI_Android_SGX_SDK/gfx_rel_es3.x_android -name '*.ko' -exec cp {} $ANDROID/out/target/product/nowplus/system/lib/modules/  \;
}

function copy_sgx_modules()
{
    echo "copying sgx modules."
	
    #CDIR=$PWD
    #echo "copy sgx driver"
    #cd $ANDROID/../sgx/
    #make install OMAPES=3.x
    #cd $CDIR
	#rm $ANDROIDROOTFS/powervr_ddk_install.log
	
	cp -Rdpf $DEVICEDIR/prebuilt/sgx/* $ANDROIDROOTFS/system/
	
}

function create_rootfs()
{
    echo "creating rootfs."
       
    #copy_modules
    
    mkdir -p $ANDROIDROOTFS
    mkdir $ANDROIDROOTFS/lib/
    cp -Rdpf $ANDROID/out/target/product/nowplus/root/* $ANDROIDROOTFS/
    cp -Rdpf $ANDROID/out/target/product/nowplus/system/* $ANDROIDROOTFS/system/
    cp -Rdpf $ANDROID/out/target/product/nowplus/data/* $ANDROIDROOTFS/data/
    cp -Rdpf $ANDROID/out/target/product/nowplus/lib/* $ANDROIDROOTFS/lib/

#    cp -Rdpf $DEVICEDIR/proprietary/dsp $ANDROIDROOTFS/system/lib/
#    cp -Rdpf $DEVICEDIR/proprietary/libsec*.so $ANDROIDROOTFS/system/lib/
#copy sgx 
   #cp -Rdpf $DEVICEDIR/prebuilt/sgx/system/* $ANDROIDROOTFS/system/
        
    #copy_sgx_modules


    #echo "creating rootfs.tar.bz2"
    #cd $ANDROIDROOTFS
    #tar jcf ../rootfs.tar.bz2 .
    
    echo "change permissions"
	chown -R root.root $ANDROIDROOTFS/*
	chmod -R a+rwx $ANDROIDROOTFS/system
	
	echo "done."
}

function create_img()
{
    echo "creating image."
    
    ANDROIDIMG=$OUTDIR/img

    copy_modules
    
    #patch init.rc
    mkdir -p $ANDROIDIMG
    mkcramfs $ANDROID/out/target/product/nowplus/root/ $ANDROIDIMG/initrd.cramfs
    mkcramfs $ANDROID/out/target/product/nowplus/system/ $ANDROIDIMG/factoryfs.cramfs
    #undo patch init.rc
    
    echo "done."
}

check_variant
create_rootfs
#create_img

