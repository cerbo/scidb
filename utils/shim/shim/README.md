Shim is a super-basic SciDB client that exposes limited SciDB functionality
through a simple HTTP API. It's based on the mongoose web server.  It's a shim
between the low-level SciDB C API and a higher-level and lightweight web
service API.

The API documentation is here: http://htmlpreview.github.io/?https://raw.github.com/Paradigm4/shim/master/wwwroot/help.html

The shim program tracks SciDB releases because it uses the SciDB client API.
You need to use a version of shim that matches your SciDB release. You can
check your shim version from a web browser with:
```
http://localhost:8080/version
```
The upcombing release will have the tag v15.7 and will be configured for
SciDB version 15.7. You can checkout any previously released version
by git tag.


## New changes for SciDB version 15.7

PAM authentication has been replaced with SciDB authentication (available
in SciDB version 15.7). In order to maintain compatibility with previous
shim clients, the HTTPS authentication API for SciDB logins
remains the same as PAM.

Many other changes -- see http://htmlpreview.github.io/?https://raw.github.com/Paradigm4/shim/master/wwwroot/help.html

## Notes

Shim supports TLS/SSL encryption and implements SciDB password authentication.
The encrypted, authenticated API adds one new argument to the unencrypted,
non-authentication API, but is otherwise identical. See
[Paradigm4/shim/master/wwwroot/help.html](http://htmlpreview.github.io/?https://raw.github.com/Paradigm4/shim/master/wwwroot/help.html)
for complete details.

The `shim` program must run on the system that a SciDB coordinator runs on.

Note: libscidbclient.so and SciDB's boost libraries must be in shim's library
path. This may entail setting
LD_LIBRARY_PATH=/opt/scidb/<whatever>/lib:/opt/scidb/<whatever>/3rdparty/boost/lib
before
running shim.  You don't have to worry about that if you install and run shim
as a service.

Note: Shim queries are limited to at most 1,000,000 characters.

## HTTP API documentation
See the wwwroot/help.html document for the API documentation, or compile and
start shim running and point a browser to http://localhost:8080/help.html.
You can also preview the help.html page directly from github at:

[Paradigm4/shim/master/wwwroot/help.html](http://htmlpreview.github.io/?https://raw.github.com/Paradigm4/shim/master/wwwroot/help.html)


The wwwroot directory also includes an example simple javascript client.

##Installation from binary packages

### NOTE!

The SciDB developers package a version of shim in SciDB pacakges with the
phrase  "dev-tools" in them. *Do not install that package and also follow the
instructions below--they are unfortunately not compatible with each other.* You
can run shim from the SciDB packages but you need to manually configure the
service.

## Non-dev-tools package installation.

This is the fastest/easiest way to install shim as a system service. We provide some pre-built binary packages.
SciDB 15.7 on Ubuntu 14.04:
* [http://paradigm4.github.io/shim/ubuntu_14.04_shim_15.7_amd64.deb](http://paradigm4.github.io/shim/ubuntu_14.04_shim_15.7_amd64.deb)

For SciDB 14.12 on Ubuntu 14.04 as well as 12.04:
* [http://paradigm4.github.io/shim/ubuntu_12.04_shim_14.12_amd64.deb](http://paradigm4.github.io/shim/ubuntu_12.04_shim_14.12_amd64.deb)
* [http://paradigm4.github.io/shim/ubuntu_14.04_shim_14.12_amd64.deb](http://paradigm4.github.io/shim/ubuntu_14.04_shim_14.12_amd64.deb)


```
# Install with:
sudo gdebi ubuntu_14.04_shim_15.7_amd64.deb

# Uninstall with (be sure to uninstall any existing copy before re-installing shim):
apt-get remove shim
```

SciDB on RHEL/Centos 6 here:

* [http://paradigm4.github.io/shim/shim-15.7-1.x86_64.rpm](http://paradigm4.github.io/shim/shim-15.7-1.x86_64.rpm)
* [http://paradigm4.github.io/shim/shim-14.12-1.x86_64.rpm](http://paradigm4.github.io/shim/shim-14.12-1.x86_64.rpm)

Packages for some older versions can be found at https://github.com/paradigm4/shim/tree/gh-pages

```
# Install with:
rpm -i shim-15.7-1.x86_64.rpm
# shim depends on libgomp. If installation fails, install libgomp and try again:
yum install libgomp

# Uninstall with:
yum remove shim
```

# Configuring  shim
The `shim` service script consults the `/var/lib/shim/conf` file for
configuration options. The default configuration options are shown below,
and optional aut-configured values are indicated. Those values are set if
you install `shim` from a binary rpm or deb package.
```
auth=login
ports=8080,8083s
scidbport=1239 (or auto-configured by apt/yum to a local SciDB port)
user=root
tmp=/tmp  (or auto-configured by apt/yum to local SciDB storage directory)
auth=login
max_sessions=50
timeout=60
instance=0  (or auto-configured by apt/yum to a local SciDB instance ID)

```
If an option is missing from the config file, the default value will be used.
The options are:

* `auth` A PAM authentication method (limited to 'login' for now).
* `ports` A comma-delimited list of HTTP listening ports. Append the lowercase
letter 's' to indicate SSL encryption.
* `scidbport` The local port to talk to SciDB on.
* `user` The user that the shim service runs under. Shim can run as a non-root
user, but then SSL authenticated port logins are limited to the user that shim
is running under.
* `tmp` Temporary I/O directory used on the server.
* `max_sessions` Maximum number of concurrent HTTP sessions.
* `timeout` Timeout after which an inactive HTTP session may be declared dead and reclaimed for use elsewhere.
* `instance` Which SciDB instance should save data to files or pipes? This instance must have write permission to the `tmp` directory.

Restart shim to effect option changes with `/etc/init.d/shimsvc restart`.

## Note on the SSL Key Certificate Configuration

Shim uses a cryptographic key certificate for SSL encrypted web connections.
When you instal shim from a binary package, a new certificate key is
dynamically generated and stored in `/var/lib/shim/ssl_cert.pem`. Feel free to
replace the certificate with one of your own. You should then also set the
permissions of the `/var/lib/shim/ssl_cert.pem` file to restrict all read and
write access to the user that shim is running under.  Restricting access
permissions to the SSL certificate is particularly important for general
machines with many untrusted users (an unlikely setting for an installation of
SciDB).

You can alternatively run `shim` from the command line and use command line
switches to set the configuration options. Run `shim -h` to see a full list
of options. When you run shim from a non-standard location, the program
expects to find the ssl_cert.pem file one directory above the wwwroot
directory.

##Packages Required for Building
Note that because shim is a SciDB client it needs the boost, zlib, log4cpp and log4cxx development libraries installed to compile. And because shim now uses PAM authentication, you'll now need the PAM development libraries for your system installed too. You also optionally need an SSL development library if you want to support TLS. Moreover, 15.7 uses a newer compiler and the paradigm4-15.7-dev package is not available if you are building SciDB from source.

### Ubuntu
For SciDB 15.7 on Ubuntu 14.04 use the below. Note if you are building SciDB from source, make sure to install SciDB at /opt/scidb and do NOT use the paradigm4-15.7-dev package:
```
sudo apt-get install paradigm4-15.7-dev make git scidb-15.7-libboost1.54-dev g++-4.9 gcc-4.9 libpqxx-dev liblog4cxx10-dev liblog4cpp5-dev libpam0g-dev zlib1g-dev ruby-dev build-essential libboost-system-dev gcc gdebi
sudo gem install fpm
```

For older SciDB versions, note `scidb-14.12-libboost1.54-all-dev` and `scidb-14.12-dev` correspond to your
installed version of SciDB, replace those package names as required for your
version. Use `apt-cache search scidb` to find the exact package names:
```
sudo apt-get install liblog4cpp5-dev liblog4cxx10-dev libboost-dev libboost-system-dev libpam0g-dev zlib1g-dev lib64z1-dev ruby-dev build-essential scidb-14.12-dev scidb-14.12-libboost1.54-all-dev
sudo gem install fpm
```


### CentOS and RHEL
For SciDB 15.7 some extra steps are needed to get the new compiler. If you are building SciDB from source, make sure to install SciDB at /opt/scidb and do NOT use the paradigm4-15.7-dev package:
```
sudo yum install scl-utils
wget https://www.softwarecollections.org/en/scls/rhscl/devtoolset-3/epel-6-x86_64/download/rhscl-devtoolset-3-epel-6-x86_64.noarch.rpm
sudo rpm -i rhscl-devtoolset-3-epel-6-x86_64.noarch.rpm 
sudo yum install paradigm4-15.7-dev git devtoolset-3-gcc-c++.x86_64 scidb-15.7-libboost-devel libpqxx-devel log4cxx-devel openssl-devel build-essential zlib-devel pam-devel ruby-devel rubygems rpm-build gcc
sudo gem install fpm
```

For older SciDB versions:
```
sudo yum install pam-devel ruby-devel rubygems rpm-build
sudo yum install --enablerepo=scidb3rdparty scidb-14.12-dev scidb-14.12-libboost-devel-1.54.0
```

## Build and install
```
make
sudo make install

# Or, if SCIDB is not in the PATH, can set a Make variable SCIDB that points
# to the SCIDB home directory, for example for version 14.12:

make SCIDB=/opt/scidb/15.7
sudo make SCIDB=/opt/scidb/15.7 install

```
### Optionally install as a service
You can install shim as a system service so that it just runs all the time with:
```
sudo make SCIDB=/opt/scidb/15.7 service
```
If you install shim as a service and want to change its default options, for example the default HTTP port or port to talk to SciDB on, you'll need to edit the shim configuration file. See the discussion of command line parameters below.
### Optionally build deb or rpm packages
You can build the service version of shim into packages for Ubuntu 12.04 or RHEL/CentOS 6 with
```
make deb-pkg
make rpm-pkg
```
respectively. Building packages requires that certain extra packaging programs are available,
including rpmbuild for RHEL/CentOS and the Ruby-based fpm packaging utility on all systems.

## Usage
```
shim [-h] [-f] [-p <http port>] [-r <document root>] [-s <scidb port>]
```
where, -f means run in the foreground (defaults to background), -h means help.

If you installed the service version, then you can control when shim is running with the usual mechanism, for example:
```
/etc/init.d/shimsvc stop
/etc/init.d/shimsvc start
```

## Uninstall
We explicitly define our SCIDB home directory for Make in the example below:
```
sudo make SCIDB=/opt/scidb/15.7 uninstall
```


## Log files
Shim prints messages to the system log. The syslog file location varies, but can usually be found in /var/log/syslog or /var/log/messages.
