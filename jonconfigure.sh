#!/bin/bash

# Nautilus
#if [ "$HOSTNAME" == "conseil" ]
#then
if [ $CUE_HOST_PROMPT = nautilus ]; then
    cp .bashrc.gcc .bashrc 
    # then relogin.  Can't swap or unload, doesn't work for some reason.

    cd /lustre/medusa/jmckinne/harmgit.verylatest/Vis5dPlus/

fi


DOLATESTAUTOMAKE=1

if [ $DOLATESTAUTOMAKE -eq 1 ]
then
    
    ############
    # for drafnel-Vis5d-326e84b patched with diff of vis5d+1.3.0-beta and jon's updates:
    #
    # primary files:
    # 1) Makefile.am in base and other dirs
    # 2) configure.in
    
    # Rest of files are secondary
    #
    export CC=gcc
    export CFLAGS="-lm"
    export FFLAGS="-lm -lg2c"
    export F77=gfortran
    #
    # clean-up
    #
    # if possible, do: make distclean
    # then do:
    rm -f configure */Makefile.in gtk/gradients/Makefile.in Makefile.in INSTALL aclocal.m4 config.guess config.h.in config.sub install-sh ltconfig ltmain.sh missing mkinstalldirs stamp-h.in
    rm -rf autom4te.cache 
    #
    #
    # only required to do below if using po and intl, but po doesn't work since po/Makefile not created correctly (just duplicates Makefile.in)
    if [ 1 -eq 0 ]
    then
        # ensure gettext installed
        # ftp://ftp.gnu.org/gnu/gettext/
        # note where po intl stuff is
        #jmckinne@ki-jmck:~/gettext-0.18$ ls -alrt ~/share/gettext/po/
        #~/bin/gettextize  -f  # for po dir
        # below needed for later gettextsize to avoid config.status error.
        # http://gspy.sourceforge.net/gettext.html
        gettextize --force --copy --intl --no-changelog
        #
        # hit RETURN
        #
        alias cp='cp'
        cp po/Makevars.template po/Makevars
        alias cp='cp -i'
        # REMOVE intl po   from Makefile.am
        # REMOVE GTK stuff from bottom of configure.in
        # to become:
        #
        # SUBDIRS = lui5 src util doc gtk
        #
        # OR: 
        #
        # SUBDIRS = lui5 src util doc
        #
        # can't go with intl -- fails with No rule to make target `all-@USE_INCLUDED_LIBINTL@' on ki-rh42
        #
        # AND DO:
        #
        # AC_CONFIG_FILES([vis5d+.pc Makefile src/Makefile doc/Makefile lui5/Makefile util/Makefile src/tclheader.h gtk/Makefile gtk/vis5dgtk.pc gtk/gradients/Makefile])
        # OR:
        # AC_CONFIG_FILES([vis5d+.pc Makefile src/Makefile doc/Makefile lui5/Makefile util/Makefile  intl/Makefile])
        #
        # OR:
        #AC_CONFIG_FILES([vis5d+.pc Makefile src/Makefile doc/Makefile lui5/Makefile util/Makefile src/tclheader.h gtk/Makefile gtk/vis5dgtk.pc gtk/gradients/Makefile intl/Makefile po/Makefile.in])
        #
        # Check that po and intl stuff doesn't appear twice
        #
    fi
    #
    # Once:
    # scp jon@ki-rh42:/data/jon/packages/harm_vis5dplus/gtk/*interface* gtk/
    # scp jon@ki-rh42:/data/jon/packages/harm_vis5dplus/gtk/*support* gtk/
    #
    libtoolize --force
    aclocal
    aclocal -I m4
    libtoolize --force
    autoheader
    autoconf
    autoconf # need to go twice to get AM_PATH_GTK and AM_PATH_GTKGL correctly set
    autoreconf --verbose --install --symlink --force
    autoconf
    automake --add-missing
    #./configure --enable-threads --prefix=/u/ki/jmckinne/
    #FINALDIR=/home/$USER/vis5dalt/
    FINALDIR=/home/$USER/
    # Nautilus:
    if [ $CUE_HOST_PROMPT = nautilus ]; then
    #    if [ "$HOSTNAME" == "conseil" ]
    #then
        FINALDIR=/nics/b/home/$USER/
    fi
    #
    ./configure --enable-threads --prefix=$FINALDIR
    # -enable-gtk
    ### NO: cp po/Makefile.in po/Makefile # because Makefile.in.in from external source
    make distclean
    ./configure --enable-threads --prefix=$FINALDIR
    # -enable-gtk
    make
    make install
    chown -R $USER $FINALDIR/lib/
    chown -R $USER $FINALDIR/bin/
    chgrp -R $USER $FINALDIR/lib/
    chgrp -R $USER $FINALDIR/bin/
    if [ $CUE_HOST_PROMPT = nautilus ]; then
        chgrp -R tug1111 $FINALDIR/lib/
        chgrp -R tug1111 $FINALDIR/bin/
    fi    
    # if modifying glade/gtk stuff (interactive during make):
    # ./configure --enable-threads --prefix=/home/$USER/ --enable-maintainer-mode     
fi





CREATEPATCH=0


if [ $CREATEPATCH -eq 1 ]
then
    
    
    #to create patch to update better vis5d+ version with jon edits:
    #
    # 1) remove all same generated stuff as start at normal compile
    # 2) rmallsvndots from each dir
    # 3) remove Makefile's if can't do: make distclean (or try distclean on some computer where that distribution almost works up to and including configure)
    
    rm -rf ./gtk/Makefile ./gtk/gradients/Makefile ./po/Makefile ./Makefile ./doc/Makefile ./m4/Makefile ./util/Makefile ./intl/Makefile ./src/Makefile ./lui5/Makefile
    # whole intl can be removed:
    rm -rf ABOUT-NLS
    rm -rf config.rpath
    rm -rf intl/
    
    # M-x replace-string C-q C-j RET RET
    rm -rf po/Makefile.in.in po/boldquot.sed po/en@boldquot.header po/en@quot.header po/insert-header.sin po/Makevars.template po/quot.sed po/remove-potcdate.sin po/Rules-quot
    rm -rf ./m4/gettext.m4 ./m4/iconv.m4 ./m4/lib-ld.m4 ./m4/lib-link.m4 ./m4/lib-prefix.m4 ./m4/nls.m4 ./m4/po.m4 ./m4/progtest.m4 ./m4/codeset.m4 ./m4/fcntl-o.m4 ./m4/glibc2.m4 ./m4/glibc21.m4 ./m4/intdiv0.m4 ./m4/intl.m4 ./m4/intldir.m4 ./m4/intlmacosx.m4 ./m4/intmax.m4 ./m4/inttypes_h.m4 ./m4/inttypes-pri.m4 ./m4/lcmessage.m4 ./m4/lock.m4 ./m4/longlong.m4 ./m4/printf-posix.m4 ./m4/size_max.m4 ./m4/stdint_h.m4 ./m4/threadlib.m4 ./m4/uintmax_t.m4 ./m4/visibility.m4 ./m4/wchar_t.m4 ./m4/wint_t.m4 ./m4/xsize.m4 
    rm -rf ./po/Makevars
    
    cd m4/
    rm -rf ltoptions.m4 libtool.m4 ltversion.m4 ltsugar.m4 lt~obsolete.m4
    cd ..
    
    rm -rf src/api-config.h depcomp make.log
    
    # remove any backup files
    find ./ -name '*~' | xargs rm
    
    # detailed removal
    
    rm -rf harm_vis5dpluscleanattempt/src/volume.backup.c
    rm -rf harm_vis5dpluscleanattempt/disk3d.v5d
    rm -rf harm_vis5dpluscleanattempt/src/binio.origkinda.c
    rm -rf harm_vis5dpluscleanattempt/ChangeLog
    cd harm_vis5dpluscleanattempt
    rm -rf script52.html
    #rm -rf po/pt_BR.gmo  po/pt_BR.po  util/igg3d
    rm -rf util/igg3d util/igu3d util/gg3d util/gr3d_to_v5d
    cd ..
    #
    rm -rf harm_vis5dpluscleanattempt/ChangeLog
    cp vis5d+-1.3.0-beta/ChangeLog harm_vis5dpluscleanattempt
    rm -rf harm_vis5dpluscleanattempt/m4/ChangeLog
    cp vis5d+-1.3.0-beta/m4/ChangeLog harm_vis5dpluscleanattempt/m4/
    
    # 3) See what files are different, remove more auto-generated files.
    
    diff --brief -bBdrupN vis5d+-1.3.0-beta harm_vis5dpluscleanattempt/ > patch.anatolyjon
    
    # or do:
    cd harm_vis5dpluscleanattempt/
    #diff --brief --exclude-from=../excludelist.txt -bBdrupN  ../vis5d+-1.3.0-beta ./ > ../patch.anatolyjon
    diff --brief -x "configure.in" -x "Makefile.am" -x "tclheader.h.in" -bBdrupN  ../vis5d+-1.3.0-beta ./ > ../patch.anatolyjon
    # NOW do without --brief
    
    scp ../patch.anatolyjon jmckinne@ki-jmck:
    
    # go to other computer and apply patch:
    
    
    # fine rejects and fix manually:
    find . -name "*.rej"
    
    # Once done, copy to new source tree a saved copy of doc/script52.html jonconfigure.sh
    
    # then push new changes as svn changes


fi











#####################################################
### Below are OLD information before auto-generated files
#####################################################


DOOLDSTUFF=0

if [ $DOOLDSTUFF -eq 1 ]
then
    
    
    # ki-rh42
    # had to get new automake 1.9, but fails to compile
    # http://rpmfind.net//linux/RPM/redhat/enterprise/4/s390x/src/automake-1.9.2-3.src.html
    # http://perso.b2b2c.ca/sarrazip/dev/rpm-building-crash-course.html
    #aclocal -I m4
    # need automake 1.9 or newer
    #autoconf
    # this no longer works on ki-rh42
    ./configure --enable-threads
    
    
    
    # ki-rh39
    ./configure --enable-threads --prefix=/u/ki/jmckinne/
    
    
    
    
    # ki-jmck (Ubuntu natty)
    #sudo bash
    export CC=gcc
    export CFLAGS="-lm"
    export FFLAGS="-lm -lg2c"
    export F77=gfortran
    ./configure --enable-threads --prefix=/home/$USER/
    make distclean
    ./configure --enable-threads --prefix=/home/$USER/
    make
    make install
    chown -R $USER /home/$USER/lib/
    chgrp -R $USER /home/$USER/lib/
    chown -R $USER /home/$USER/bin/
    chgrp -R $USER /home/$USER/bin/
    
    
    # note that toremove.sh removes too much (e.g. po/pt_BR.*)
    
    # added after remove:
    #list=`svn status | awk '{print $2}'`
    #root@ki-jmck:~/harm_vis5dpluscleanattempt# echo $list
    #autom4te.cache toremove.sh po/pt_BR.po po/pt_BR.gmo po/POTFILES lui5/.deps gtk/Makefile gtk/vis5dgtk.pc gtk/gradients/Makefile src/.deps src/.libs src/stamp-h2 src/api-config.h stamp-h1 jonconfigure.sh util/.libs util/.deps util/igg3d


fi




