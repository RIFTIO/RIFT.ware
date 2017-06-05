#!/bin/bash
#
# STANDARD_RIFT_IO_COPYRIGHT
#

# this script extracts the rpm files
# Usage:
#     extract_rpm.sh <rootdir> <rpmfile>

function extract_rpm()
{
    echo "rpm --dbpath $rootdir/usr/lib/rpm --relocate /usr/rift=$rootdir --nodeps -ivh $rpmfile" >&2
    rpm --dbpath $rootdir/usr/lib/rpm --relocate /usr/rift=$rootdir --nodeps -ivh $rpmfile
    return $?
}

function strip_rpaths()
{
    rpm -qlp $rpmfile | while read line; do
        # If the RPM command failed (no files or whatever) then get out.
        if [ ${PIPESTATUS[0]} -ne 0 ]; then
          return
        fi

        # dest_file will replace the / with <rootdir> (install path)
        dest_file=$rootdir/${line#"/usr/rift"}
        if [ ! -e "$dest_file" ]; then
          echo "****Cannot find dest file: $dest_file"
          continue
        fi

        # Skip installed directories
        if [ ! -f "$dest_file" ] ; then
          continue
        fi

        # Get the list of rpaths for this particular file.
        # If the command fails, then move on to the next file
        file_rpath=$(chrpath --list "$dest_file" 2>/dev/null)
        if [ $? -ne 0 ]; then
          continue
        fi

        # Extract the beginning of chrpath --list command
        # that has no value
        rpath_string=${file_rpath##*RPATH=}

        # If there is an empty RPATH then move on to next file
        if [ "$rpath_string" == "" ]; then
          continue
        fi

        # Split the RPATH into the array of directories
        IFS=':' read -a rpath_array <<< "$rpath_string"

        for index in "${!rpath_array[@]}"
        do
          rpath_element=${rpath_array[index]}
          # If this rpath element contains a .install then readjust it to match
          # the current install path.
          if [[ $rpath_element == */build/*/install/* ]]; then
            unset "rpath_array[index]"
            # This logic below is to replace the .install path with the workspace's
            # .install path. Unfortunately becuase of the chrpath limitation of the
            # replacement rpath being <= source rpath, it is difficult to pull off.
            #
            #cache_install_path=${rpath_element%\.install*}".install"
            #new_rpath_element=${rpath_element//$cache_install_path/$rootdir}
            #rpath_array[index]=$new_rpath_element
          fi

          # If the rpath entry contains a .build directory then strip it out completely
          if [[ $rpath_element == */build/*/build/* ]]; then
            unset "rpath_array[index]"
          fi
        done

        # Reassemble the rpath string
        new_rpath_string=$(IFS=$':'; echo "${rpath_array[*]}")

        # Replace the existing rpath with our newly contructed one.
        chrpath --replace "$new_rpath_string" $dest_file
        if [ $? -ne 0 ]; then
          echo "ERROR:  ***Failed to replace rpath in $dest_file****"
          echo "****************************************************"
          echo "Please see RIFT-3498.  If the chrpath fails due to path length issues"
          echo "A solution is to increase Jenkins/CI build path length"
          echo "or shorten your workspace path length."
          exit 1
        fi

      done
}

# Make an RPM database in this directory
rootdir=$1
# rm -rf $rootdir
mkdir -p $rootdir || exit 1
rpm --initdb --dbpath $rootdir/usr/lib/rpm || exit 1

# Set which rpm file to work on
rpmfile=$2

# Extract the RPM file
extract_rpm || exit 1

strip_rpaths
