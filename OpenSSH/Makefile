##
# Makefile for OpenSSH
##
# Wilfredo Sanchez, wsanchez@apple.com
##

# Project info
Project               = openssh
ProjectName           = OpenSSH
UserType              = Administrator
ToolType              = Services
Extra_CC_Flags        = -Wno-precomp -DBROKEN_GETADDRINFO -Dgetaddrinfo=fake_getaddrinfo -Dcrc32=crcsum32 # crc32() symbol conflicts with zlib (!)
Extra_LD_Flags        = -L.
Extra_Configure_Flags = --sysconfdir="/etc" --disable-suid-ssh
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) manpages
