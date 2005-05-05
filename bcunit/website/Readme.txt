This directory contains the version 2 CUnit website pages.
The ./images subfolder contains gif screenshot images
referred to in the screenshot pages.  The web pages and
images are all committed to the CUnit cvs on SourceForge.

NOTE:

The page 'documentation.html' links to html versions of
both the CUnit docs (distributed with CUnit) and the
doxygen-generated programmers reference.  It expects to
find the starting pages of these 2 documents at the
following locations:

  ./doc/index.html        Start page of standard docs

  ./doxdocs/index.html    Start page of doxygen docs


These documentation files are NOT archived in the CUnit 
cvs on SourceForge.  To publish the website, the following
steps need to be taken to provide valid links to these
documents:

  1. In these steps 'DIST' refers to the top directory of
     the CUnit distribution tree.  'WEB' refers to the top
     directory in the website tree.

  2. Copy SOURCE/doc/*.html and SOURCE/doc/*.css to WEB/doc.

  3. Copy SOURCE/CUnit/Headers/*.h to WEB/doc/headers.

  4. Generate the doxygen docs for the CUnit source using
     the doxygen configuration file SOURCE/Doxyfile.  The
     doxygen docs will be generated in SOURCE/doc/html.

  5. Copy SOURCE/doc/html/*.* to WEB/doxdocs.
  
  
JDS
4-May-2005


