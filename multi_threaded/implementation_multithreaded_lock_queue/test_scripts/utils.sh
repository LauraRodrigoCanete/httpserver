cleanup() {
    for file in $@; do
	rm -r $file
    done
}

check_dir() {
    if [ ! -f queue.o ]; then
	echo "I cannot find queue.o.  Did you..."
	echo "(1) build your assignment?"
	echo "(2) run this script from your assignment directory?"
	return 1
    fi
    return 0
}
