# How to make a new release

1. Tag the source code. E.g.:

   ```
   git tag -a 1.0.3 -m 'Release 1.0.3'
   git push origin 1.0.3
   ```

2. **Check out a clean tree**, do a local build and do a `make dist` to generate the source tarball:

   ```
   $ git clone https://fabriziobertocci@github.com/rticommunity/connext-config.git
   $ cd connext-config
   $ autoreconf -ivf
   $ ./configure
   $ make
   $ make dist
   ```

3. Copy the `connext-config-1.0.3.tar.gz` on the target build machines.

   ```
   Target: x64Linux2.6gcc4.4.5  machine=rhel61-64-dev
   Target: x64Darwin15clang7.0  machine=cupertino4
   ```

4. un-tar and build (build system under `/home/fabrizio/connext-config/<target>`):

   ```
   $ cd connext-config/x64Linux2.6gcc4.4.5
   $ tar xvfz ~/connext-config-1.0.3.tar.gz
   $ cd connext-config-1.0.3
   $ ./configure --with-rticonnextdds=/local/preship/ndds/ndds.6.0.1/unlicensed/rti_connext_dds-6.0.1
   $ make
   ```

5. Copy built files in the staging area and tar:

   ```
   $ cd releases/resource/app/bin
   $ rm x64Linux2.6gcc4.4.5/connext-config
   $ scp fabrizio@brosno:/home/fabrizio/connext-config/x64Linux2.6gcc4.4.5/connext-config-1.0.3/src/connext-config x64Linux2.6gcc4.4.5
   $ rm x64Darwin15clang7.0/connext-config
   $ scp fabrizio@brosno:/home/fabrizio/connext-config/x64Darwin15clang7.0/connext-config-1.0.3/src/connext-config .
   $ cd ../../..
   $ tar cvfz connext-config-1.0.3.tar.gz bin resource
   ```

   