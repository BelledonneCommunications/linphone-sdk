# $Id: .cshrc,v 1.1.1.2 2001-08-13 05:12:07 anilsaharan Exp $
# Luis Francisco González <luisgh@debian.org> based on that of Vadik Vygonets
# Please check /usr/doc/tcsh/examples/cshrc to see other possible values.
if ( $?prompt ) then
  set autoexpand
  set autolist
  set cdpath = ( ~ )
  set pushdtohome

# Load aliases from ~/.alias
  if ( -e ~/.alias )	source ~/.alias

endif
