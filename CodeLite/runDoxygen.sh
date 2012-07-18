#!/bin/sh

# runDoxygen.sh
#
# Created by René J.V. Bertin on 20120714.


#  Build the doxygen documentation for a CodeLite project.

#  Use the following to adjust the value of the $DOXYGEN_PATH User-Defined Setting:
#    Binary install location: /Applications/Productivity/Doxygen.app/Contents/Resources/doxygen
#    Source build install location: /usr/local/bin/doxygen

#  If the config file doesn't exist, run 'doxygen -g $SOURCE_ROOT/doxygen.config' to 
#   a get default file.

if [ "${DOXYGEN_PATH}" = "" ] ;then
	DOXYGEN_PATH="/usr/bin/doxygen"
fi

# CodeLite project lives in a subdirectory of the true source root; commands are executed
# in this directory:
SOURCE_ROOT=".."
TEMP_DIR="."

if ! [ -f $SOURCE_ROOT/doxygen.config ] 
then 
  echo doxygen config file does not exist
  $DOXYGEN_PATH -g $SOURCE_ROOT/doxygen.config
fi

#  Append the proper input/output directories and docset info to the config file.
#  This works even though values are assigned higher up in the file. Easier than sed.

cp $SOURCE_ROOT/doxygen.config $TEMP_DIR/doxygen.config

echo "INPUT = $SOURCE_ROOT" >> $TEMP_DIR/doxygen.config

OUTPUT_DIRECTORY="$SOURCE_ROOT/SynchroTr.docset"
echo "OUTPUT_DIRECTORY = $OUTPUT_DIRECTORY" >> $TEMP_DIR/doxygen.config
# set in doxygen.config
# echo "DOCSET_BUNDLE_ID       = org.RJVB.SynchroTr" >> $TEMP_DIR/doxygen.config

# add the standard compiler tags to the defined tokens:
echo "PREDEFINED += linux __GNUC__" >> $TEMP_DIR/doxygen.config

#  Run doxygen on the updated config file.
#  Note: doxygen creates a Makefile that does most of the heavy lifting.

$DOXYGEN_PATH $TEMP_DIR/doxygen.config

exit 0