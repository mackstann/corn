#!/bin/sh
aclocal -I m4 && \
autoheader && \
autoconf && \
automake --foreign --add-missing && \
automake --foreign

