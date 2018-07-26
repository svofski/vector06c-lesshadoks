ctags -o .tags1 -R ~/tmpsrc/esp-open-rtos
ctags -o .tags3 -R .
cat .tags3 .tags1 >tags
rm -f .tags1 .tags3
