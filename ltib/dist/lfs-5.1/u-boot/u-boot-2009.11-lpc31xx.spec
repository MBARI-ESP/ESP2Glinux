%define pfx /opt/nxp/rootfs/%{_target_cpu}

Summary         : Universal Bootloader firmware
Name            : u-boot
Version         : 2009.11
Release         : 1
License         : GPL
Vendor          : NXP
Packager        : Kevin Wells
Group           : Applications/System
Source          : %{name}-%{version}.tar.bz2
Patch0		: u-boot-2009.11_lpc313x.patch
Patch1		: u-boot-2009.11_lpc313x_v1.01.patch
Patch2          : u-boot-2009.11-disable_NAND_hardware_ecc.patch
Patch3          : u-boot-2009.11-spansionSPIflash.patch
Patch4          : u-boot-2009.11-mbari3.patch
Patch5          : u-boot-2009.11-mbari4.patch
Patch6          : u-boot-2009.11-mbari5.patch
Patch7          : u-boot-2009.11-270mhz.patch
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

This specfile attempts to recreate the 2009.11 u-boot binaries
needed for boards using u-boot and the LPC31XX.

%Prep
%setup -n %{name}-%{version}
%patch0 -p1
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1

%Build
: ${PKG_U_BOOT_CONFIG_TYPE:?must be set, e.g. MPC8548CDS_config}
PKG_U_BOOT_PATH_PRECONFIG=$(eval echo $PKG_U_BOOT_PATH_PRECONFIG)
SRC_DIR=${PKG_U_BOOT_PATH_PRECONFIG:-%{_builddir}/%{buildsubdir}}
%{!?showsrcpath: %define showsrcpath 0}
%if %{showsrcpath}
%{echo:%(eval echo ${PKG_U_BOOT_PATH_PRECONFIG:-%{_builddir}/%{buildsubdir}})}
%endif

BUILD_DIR=%{_builddir}/%{buildsubdir}
if [ $SRC_DIR != $BUILD_DIR ]
then
    mkdir -p $BUILD_DIR
fi
cd $SRC_DIR
if [ -n "$LTIB_FULL_REBUILD" ]
then
    make HOSTCC="$BUILDCC" CROSS_COMPILE=$TOOLCHAIN_PREFIX O=$BUILD_DIR distclean
fi
make HOSTCC="$BUILDCC" CROSS_COMPILE=$TOOLCHAIN_PREFIX O=$BUILD_DIR $PKG_U_BOOT_CONFIG_TYPE
make HOSTCC="$BUILDCC" HOSTSTRIP="$BUILDSTRIP" \
     CROSS_COMPILE=$TOOLCHAIN_PREFIX $PKG_U_BOOT_BUILD_ARGS \
     O=$BUILD_DIR all

%Install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{pfx}/boot
BUILD_DIR=%{_builddir}/%{buildsubdir}
cd $BUILD_DIR
dd if=u-boot.bin of=init-u-boot.bin bs=1024 count=78
for i in u-boot.bin init-u-boot.bin
do
    cp $i $RPM_BUILD_ROOT/%{pfx}/boot
done

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*

