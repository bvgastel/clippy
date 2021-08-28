#!/bin/sh

# originally from https://github.com/danrue/oneoff-pkg-create, adjusted
usage ()
{
	echo "Usage: $0 <manifest_template> <temp_manifest_location> <post-install> <post-deinstall> (run in files_directory)"
	exit
}

if [ "$#" -ne 4 ]
then
	usage
fi

manifest_template=$1
manifest=$2
files_dir=.
DIR_SIZE=$(find ${files_dir} -type f -exec stat -f %z {} + | awk 'BEGIN {s=0} {s+=$1} END {print s}')
export DIR_SIZE
{
	${manifest_template} "$3" "$4"
	# Add files in
	echo " files: {"
	find ${files_dir} -type f -exec sha256 -r {} + |
    awk '{print "    /" substr($2, 3) ": \"" $1 "\","}'
	# Add symlinks in
	find ${files_dir} -type l |
    awk '{print "    /" substr($1, 3) ": \"-\","}'
	echo " }"

	# Add files_directories in
	echo " files_directories: {"
	find ${files_dir} -type d -mindepth 1 |
    awk '{print "    /" substr($1, 3) ": y,"}'
  echo " }"

} | sed -e "s:${files_dir}::" > ${manifest}

# Create the package
pkg create --verbose -r ${files_dir} -M ${manifest} -o .
ls -l *.* || true
