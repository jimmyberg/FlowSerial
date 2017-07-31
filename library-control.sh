#!/bin/bash
##
MAJOR=4
MINOR=0

LIB_PATH_INSTALL=/usr/local/lib/
INC_PATH_INSTALL=/usr/local/include/

LIBNAME=libflowserial

function install-dependencies {
	git submodule update --init --recursive
	cd CircularBuffer &&
	./install.sh install &&
	cd ../LinearBuffer &&
	./install.sh install &&
	cd ..
}

function remove-dependencies {
	cd CircularBuffer &&
	./install.sh remove &&
	cd ../LinearBuffer &&
	./install.sh remove &&
	cd ..
}

function compile-shared {
	echo compiling..
	# Compile local cpp files
	g++ -Wall -std=c++11 -fPIC -c *.cpp &&
	# Make a dynamic library
	g++ -Wall -shared -Wl,-soname,$LIBNAME.so.$MAJOR -o $LIBNAME.so.$MAJOR.$MINOR *.o
}

case "$1" in
	install)
		# install dependencies if necessary.
		if [[ -f /usr/local/include/CircularBuffer && -f /usr/local/include/LinearBuffer ]]; then
			echo "Dependencies installed."
		else
			echo "Dependencies not installed. Installing dependencies..."
			install-dependencies;
		fi
		compile-shared
		echo "Installing dynamic library.."
		sudo cp -v -n "$LIBNAME.so.$MAJOR.$MINOR" "$LIB_PATH_INSTALL" &&
		echo "Making symbolic links for running and for development."
		sudo ln -sfv "$LIB_PATH_INSTALL""$LIBNAME".so.$MAJOR.$MINOR "$LIB_PATH_INSTALL""$LIBNAME".so.$MAJOR &&
		sudo ln -sfv "$LIB_PATH_INSTALL""$LIBNAME".so.$MAJOR "$LIB_PATH_INSTALL""$LIBNAME".so &&
		echo "Installing header file(s)"
		sudo cp -v -n FlowSerial.hpp "$INC_PATH_INSTALL"FlowSerial &&
		echo "Cleanup"
		rm *.so* *.o &&
		echo "Updating ld cache"
		sudo ldconfig
		echo "Install successful. Have a nice day!"
		;;
	remove)
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so.$MAJOR.$MINOR" &&
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so.$MAJOR" &&
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so" &&
		sudo rm -v "$INC_PATH_INSTALL"FlowSerial
		;;
	remove-all)
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so.$MAJOR.$MINOR" &&
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so.$MAJOR" &&
		sudo rm -v "$LIB_PATH_INSTALL""$LIBNAME.so" &&
		sudo rm -v "$INC_PATH_INSTALL"FlowSerial &&
		remove-dependencies
		;;
	reinstall)
		compile-shared &&
		# install dynamic library
		sudo cp -v SOFILE_NAME LIB_PATH_INSTALL &&
		# Make links for running and for development.
		sudo ln -sf "$LIB_PATH_INSTALL"$LIBNAME.so.$MAJOR.$MINOR "$LIB_PATH_INSTALL"$LIBNAME.so.$MAJOR &&
		sudo ln -sf "$LIB_PATH_INSTALL"$LIBNAME.so.$MAJOR "$LIB_PATH_INSTALL"$LIBNAME.so &&
		# Install header file(s)
		sudo cp -v FlowSerial.hpp "$INC_PATH_INSTALL"FlowSerial &&
		# cleanup
		rm *.so *.o
		;;
	install-dep)
		install-dependencies
		;;
	remove-dep)
		remove-dependencies
		;;
	*)
		echo $"Usage: $0 {install|remove|remove-all|reinstall|install-dep|remove-dep}"
		exit 1
esac
