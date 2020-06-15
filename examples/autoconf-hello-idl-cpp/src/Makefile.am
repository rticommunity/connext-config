#########################################################################
# APPLICATION SECTION
#########################################################################

bin_PROGRAMS = Hello

noinst_HEADERS = Hello.h \
		 HelloPublisher.h \
		 HelloSubscriber.h \
		 HelloWorld.h \
		 HelloWorldSupport.h \
		 HelloWorldPlugin.h

Hello_SOURCES = HelloWorld.cxx \
		HelloWorldSupport.cxx \
		HelloWorldPlugin.cxx \
		HelloPublisher.cxx \
		HelloSubscriber.cxx \
		Hello.cxx


# Any additional RTI library you want to use, add it here. For example
# to add the Request/Reply messaging library, uncomment the following line:
#
# Hello_LDADD = $(LDADD) -lrticonnextmsgcpp@RTI_LIB_SUFFIX@
#

# The rule that invokes rtiddsgen to generate type support code from the IDL
HelloWorld.cxx HelloWorld.h HelloWorldSupport.cxx HelloWorldSupport.h HelloWorldPlugin.cxx HelloWorldPlugin.h: HelloWorld.idl
	${NDDSHOME}/bin/rtiddsgen -language C++ -replace HelloWorld.idl

# Extends the "make clean" rule to delete the sources generated from rtiddsgen
clean-local:
	-rm -rf HelloWorld.cxx HelloWorld.h HelloWorldPlugin.cxx HelloWorldPlugin.h HelloWorldSupport.cxx HelloWorldSupport.h

