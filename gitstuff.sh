# on ki-jmck:

cd ~/
git clone git://github.com/drafnel/Vis5d.git
rm -rf Vis5dPlus
mv Vis5d Vis5dPlus

cd Vis5dPlus/
git remote -v
git remote set-url origin git@github.com:pseudotensor/Vis5dPlus.git
git remote -v
git status
git branch -a


git fetch origin
# if repeating:
# git checkout nothing ; cp README.md .. ; git checkout master
cp ../README.md .
git add README.md
git push origin master
git pull origin master
git push origin remotes/origin/api_exp_branch
git push origin remotes/origin/none
git push origin remotes/origin/vis5d-1-2
git checkout -b api_exp_branch remotes/origin/api_exp_branch
git push origin api_exp_branch
git checkout -b none remotes/origin/none
git push origin none
git checkout -b vis5d-1-2 remotes/origin/vis5d-1-2
git push origin vis5d-1-2
git branch -a
git checkout master
# if repeating:
# Goto Admin on github and choose master as default branch
# git push origin :nothing ; git branch -D nothing

# http://ivanz.com/2009/01/15/selective-import-of-svn-branches-into-a-gitgit-svn-repository/
# one version of adding branch
emacs .git/config &
# add:
# [svn-remote "jonupdate"]
#         url = https://harm.unfuddle.com/svn/harm_vis5dplus/
#        fetch = :refs/remotes/git-svn-jonupdate
#sudo aptitude install git-svn
git svn fetch jonupdate -r 1
git branch --track jonupdate git-svn-jonupdate
git checkout jonupdate
git svn rebase
# remove svn stuff
git branch -D -r git-svn-jonupdate
# git push origin :jonupdate
git gc


###
# http://stackoverflow.com/questions/79165/how-to-migrate-svn-with-history-to-a-new-git-repository
# or try
git svn clone  https://harm.unfuddle.com/svn/harm_vis5dplus/
git branch -a
git status
ls harm_vis5dplus/


## try:
# http://maururu.net/2009/svn-to-git-take-2/
#git svn clone -s --no-metadata https://harm.unfuddle.com/svn/harm_vis5dplus/
git svn clone https://harm.unfuddle.com/svn/harm_vis5dplus/
git fetch . refs/remotes/*:refs/heads/*
git branch -rd git-svn
git branch -D git-svn
git config --remove-section svn-remote.svn
rm -Rf .git/svn/



# or just assume all (only 14) jon updates are 1 commit on top of ralf code, which is how applied the patch
# do the svn clone as one of the above
# then remove .git from that dir so no svn or git stuff
# then go into new code directory:
# jmckinne@ki-jmck:~/Vis5dPlus
# cp gitstuff.sh to local dir
# git add gitstuff.sh 
# now overwrite entire distribution (all except .git):
# rm -rf * # keeps .git and .gitignore
# cp -a ../svngitvis5d/harm_vis5dplusnogit/* .
# cp ../README.md .
# cp ~/gitstuff.sh .
# things to add:
git add contrib/CVS/ contrib/SimonBaas/CVS/ contrib/epa/CVS/ contrib/gemvis/CVS/ contrib/gemvis/source/CVS/ contrib/grads/CVS/ contrib/grib/CVS/ contrib/grib/src/CVS/ contrib/hdftovis/CVS/ contrib/uw-meteor/CVS/ contrib/witte/CVS/ convert/CVS/ doc/html/ doc/script52.html doc/vis5d.pdf jonconfigure.sh m4/ChangeLog m4/Makefile.am m4/isc-posix.m4 pixmaps/CVS/ po/es.gmo po/pt_BR.gmo scripts/CVS/ scripts/earl.tcl src/binio.origkinda.c src/volume.backup.c toadd.sh toremove.sh toremovenew.sh userfuncs/CVS/

COMMENT="patch applied on top of 326e84b7df of https://github.com/drafnel/Vis5d where the patch was formed as a diff between http://sourceforge.net/projects/vis5d/files/latest/download of vis5d+-1.3.0-beta.tar.gz and 14 major svn edits made by Anatoly Spitkovsky and mostly Jonathan McKinney.  Those 14 major edits allow for greater than 4GB memory use, better stereo control, better trajectory control, and higher limits on static constants for objects."

git commit -a -m "$COMMENT"

git commit --amend -a -m "$COMMENT"




### to clean-up do:
rm -rf harm_vis5dplus/
git branch -D api_exp_branch
git branch -D none
git branch -D vis5d-1-2
git push origin :remotes/origin/api_exp_branch
git push origin :remotes/origin/none
git push origin :remotes/origin/vis5d-1-2
git push origin --delete api_exp_branch
git push origin --delete none
git push origin --delete vis5d-1-2
# create fake branch
git branch nothing
git checkout nothing
# now to to Admin and choose nothing as default branch
git branch -D master
git push origin :master
git push origin --delete master
