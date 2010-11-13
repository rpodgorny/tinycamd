#
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile 6008 2007-01-06 18:39:10Z nbd $

include $(TOPDIR)/rules.mk

PKG_NAME:=tinycamd
PKG_VERSION:=0.3
PKG_RELEASE:=1

#PKG_MD5SUM:=ea50419f99472200c4184a304e3831ea
#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
#PKG_SOURCE_URL:=http://download.insecure.org/nmap/dist
#PKG_CAT:=bzcat

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/tinycamd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=A tiny webcam daemon for v4l2
  URL:=http://code.google.com/p/tinycamd/
  DEPENDS:=+libpthread +libjpeg
endef

define Package/tinycamd/description
  A tiny webcam daemon for v4l2:
  A minimal webcam daemon that works efficiently with v4l2 devices. It is particularly CPU efficient
  with cameras that support JPEG or MJPEG encoding.
endef

define Build/Prepare
	echo PREPARE PREPARE
	mkdir -p $(PKG_BUILD_DIR)
	svn checkout https://tinycamd.googlecode.com/svn/trunk/ $(PKG_BUILD_DIR) --username jimstudt
endef

define Package/tinycamd/install	
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tinycamd $(1)/usr/bin/
endef

$(eval $(call BuildPackage,tinycamd))
