# FlowSerial
FlowSerial is a library that can be used to share a array with another peer. The array can both be read as been written by the other party. This simplifies and generalizes the way to communicate.
The array is to be generated by the user and given to the FlowSerial class. This is done by assigning a pointer.

## Install
This library can be used statically by adding the source files to your own project but can also be installed in the system as a shared library. 

Most things can be done by using the shell script ./library-control.sh that can be found in this repository. The following options are available in this script

# Install dependencies only
```
./library-control.sh install-dep
```

# Install as shared library
```
./library-control.sh install
```

# Remove shared library and dependencies (does not check if dependencies are user by something else)
```
./library-control.sh remove-all
```

# Remove shared library
```
./library-control.sh remove
```

# Remove dependencies
```
./library-control.sh remove-deb
```
To use this repository in another git repository
```
git submodule add https://github.com/overlord1123/FlowSerial.git
```
## Example

```c++
A example implementation will be made in the near future...

```
